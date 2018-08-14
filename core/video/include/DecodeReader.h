#ifndef LIGHTDB_DECODEREADER_H
#define LIGHTDB_DECODEREADER_H

#include "spsc_queue.h"
#include <cuda.h>
#include <thread>

struct DecodeReaderPacket: public CUVIDSOURCEDATAPACKET {
public:
    DecodeReaderPacket() = default;

    DecodeReaderPacket(const DecodeReaderPacket &packet) = default;

    explicit DecodeReaderPacket(const CUVIDSOURCEDATAPACKET &packet)
        : CUVIDSOURCEDATAPACKET{packet.flags, packet.payload_size, nullptr, packet.timestamp},
          buffer_(std::make_shared<std::vector<unsigned char>>()) {
        buffer_->reserve(payload_size);
        buffer_->insert(buffer_->begin(), packet.payload, packet.payload + payload_size);
        payload = buffer_->data();
    }

    explicit DecodeReaderPacket(const std::vector<char> &data, const unsigned long flags=0,
                                const CUvideotimestamp timestamp=0)
            : DecodeReaderPacket(CUVIDSOURCEDATAPACKET{flags, data.size(),
                                                       reinterpret_cast<const unsigned char*>(data.data()), timestamp})
    { }

    DecodeReaderPacket& operator=(const DecodeReaderPacket &packet) = default;
    bool operator==(const DecodeReaderPacket &packet) const noexcept {
        return this->payload_size == packet.payload_size &&
               this->flags == packet.flags &&
               this->timestamp == packet.timestamp &&
               this->buffer_ == packet.buffer_;
    }

private:
    std::shared_ptr<std::vector<unsigned char>> buffer_;
};

class DecodeReader {
public:
    class iterator {
        friend class DecodeReader;

    public:
        bool operator==(const iterator& other) const { return (eos_ && other.eos_) ||
                                                              (reader_ == other.reader_ &&
                                                               *current_ == *other.current_); }
        bool operator!=(const iterator& other) const { return !(*this == other); }
        void operator++()
        {
            if (!(current_ = reader_->read()).has_value())
                eos_ = true;
        }
        DecodeReaderPacket operator++(int)
        {
            auto value = **this;
            ++*this;
            return value;
        }
        DecodeReaderPacket operator*() { return current_.value(); }

    protected:
        explicit iterator(DecodeReader &reader)
                : reader_(&reader), current_({reader.read()}), eos_(false)
        { }
        constexpr explicit iterator()
                : reader_(nullptr), current_(), eos_(true)
        { }

    private:
        DecodeReader *reader_;
        std::optional<DecodeReaderPacket> current_;
        bool eos_;
    };

    virtual iterator begin() { return iterator(*this); }
    virtual iterator end() { return iterator(); }

    virtual std::optional<DecodeReaderPacket> read() = 0;
    virtual CUVIDEOFORMAT format() const = 0;
    virtual bool isComplete() const = 0;
};

class FileDecodeReader: public DecodeReader {
public:
    explicit FileDecodeReader(const std::string &filename)
        : FileDecodeReader(filename.c_str()) { }

    explicit FileDecodeReader(const char *filename)
            : filename_(filename),
              packets_(std::make_unique<lightdb::spsc_queue<DecodeReaderPacket>>(4096)), // Must be initialized before source
              source_(CreateVideoSource(filename)),
              format_(GetVideoSourceFormat(source_)),
              decoded_bytes_(0) {
        if(format().codec != cudaVideoCodec_H264 && format().codec != cudaVideoCodec_HEVC)
            throw GpuRuntimeError("FileDecodeReader only supports H264/HEVC input video");
        else if(format().chroma_format != cudaVideoChromaFormat_420)
            throw GpuRuntimeError("FileDecodeReader only supports 4:2:0 chroma");
    }

    FileDecodeReader(FileDecodeReader&& other) = default;

    CUVIDEOFORMAT format() const override { return format_; }
    const std::string &filename() const { return filename_; }

    std::optional<DecodeReaderPacket> read() override {
        DecodeReaderPacket packet;

        while (cuvidGetVideoSourceState(source_) == cudaVideoState_Started &&
                !packets_->read_available())
            std::this_thread::yield();

        if(packets_->pop(packet)) {
            decoded_bytes_ += packet.payload_size;
            return packet;
        } else {
            LOG(INFO) << "Decoded " << decoded_bytes_ << " bytes";
            return {};
        }
    }

    bool isComplete() const override {
        return !packets_->read_available() && cuvidGetVideoSourceState(source_) != cudaVideoState_Started;
    }

private:
    static int CUDAAPI HandleVideoData(void *userData, CUVIDSOURCEDATAPACKET *packet) {
        auto *packets = static_cast<boost::lockfree::spsc_queue<DecodeReaderPacket>*>(userData);

        CUVIDSOURCEDATAPACKET *copy = new CUVIDSOURCEDATAPACKET{
                .flags = packet->flags,
                .payload_size = packet->payload_size,
                .payload =
                new unsigned char[packet->payload_size],
                .timestamp = packet->timestamp
        };
        memcpy(const_cast<unsigned char*>(copy->payload), const_cast<unsigned char*>(packet->payload),
               packet->payload_size);

        while(!packets->push(DecodeReaderPacket(*packet)))
            std::this_thread::yield();

        return 1;
    }

    CUvideosource CreateVideoSource(const char *filename) {
        CUresult status;
        CUvideosource source;
        CUVIDSOURCEPARAMS videoSourceParameters = {
                .ulClockRate = 0,
                .uReserved1 = {},
                .pUserData = packets_.get(),
                .pfnVideoDataHandler = HandleVideoData,
                .pfnAudioDataHandler = nullptr,
                0
        };

        if((status = cuvidCreateVideoSource(&source, filename, &videoSourceParameters)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Call to cuvidCreateVideoSource failed", status);
        else if((status = cuvidSetVideoSourceState(source, cudaVideoState_Started)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Call to cuvidSetVideoSourceState failed", status);

        return source;
    }

    CUVIDEOFORMAT GetVideoSourceFormat(CUvideosource source) {
        CUresult status;
        CUVIDEOFORMAT format;

        if((status = cuvidGetSourceVideoFormat(source, &format, 0)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Call to cuvidGetSourceVideoFormat failed", status);
        return format;
    }

    std::string filename_;
    std::unique_ptr<lightdb::spsc_queue<DecodeReaderPacket>> packets_;
    CUvideosource source_;
    CUVIDEOFORMAT format_;
    size_t decoded_bytes_;
};

#endif //LIGHTDB_DECODEREADER_H
