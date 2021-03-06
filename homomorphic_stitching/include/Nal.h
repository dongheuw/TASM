#ifndef HOMOMORPHIC_STITCHING_NAL_H
#define HOMOMORPHIC_STITCHING_NAL_H

#include "StitchContext.h"
#include "NalType.h"
#include "bytestring.h"
#include <glog/logging.h>
#include <memory>
#include <array>
#include <bitset>
#include <cmath>
#include <climits>
#include <cassert>

namespace stitching {

    class Headers;
    class SliceSegmentLayer;

    /**
     *
     * @param data The byte stream
     * @return The type of the byte stream as defined in NalType.h
     */
    inline unsigned int PeekType(char &byte) {
        return (static_cast<unsigned char>(byte) & 0x7Fu) >> 1;
    }

    inline unsigned int PeekType(const bytestring &data) {
        assert(!data.empty());
        return (static_cast<unsigned char>(data[0]) & 0x7Fu) >> 1;
    }

    inline unsigned int PeekType(std::vector<bool>::iterator startOfNalUnitType) {
        unsigned char value = 0;
        for (auto i = 0u; i < 6; i++) {
            value = (value << 1u) | *startOfNalUnitType++;
        }
        return value;
    }

    class Nal {
     public:

        /**
         * Creates a new Nal
         * @param context The context of the Nal
         * @param data The bytes representing the Nal
         */
        Nal(StitchContext context, const bytestring &data)
            : byte_data_(data),
              context_(std::move(context)),
              type_(PeekType(data)),
              is_header_(IsHeader()) {
            assert(ForbiddenZero() == 0);
        }

        /**
         *
         * @return The context associated with this Nal
         */
        inline const StitchContext& GetContext() const {
            return context_;
        }

        /**
         *
         * @return True if the nal represents a SequenceParameterSet, false otherwise
         */
        inline bool IsSequence() const {
            return type_ == NalUnitSPS;
        }

        /**
         *
         * @return True if the nal represents a PictureParameterSet, false otherwise
         */
        inline bool IsPicture() const {
            return type_ == NalUnitPPS;
        }

        /**
         *
         * @return True if the nal represents a VideoParameterSet, false otherwise
         */
        inline bool IsVideo() const {
            return type_ == NalUnitVPS;
        }

        /**
         *
         * @return True if the nal represents a header, false otherwise
         */
        inline bool IsHeader() const {
            return IsSequence() || IsPicture() || IsVideo();
        }

        inline bool IsIDRSegment() const {
            return type_ == NalUnitCodedSliceIDRWRADL;
        }

        inline bool IsTrailRSegment() const {
            return type_ == NalUnitCodedSliceTrailR;
        }

        /**
         *
         * @return The bytestring that represents this Nal
         */
        inline virtual bytestring GetBytes() const {
            return byte_data_;
        }

        /**
         * @return A string representing a nal marker
         */
        static inline bytestring GetNalMarker() {
            return bytestring{0, 0, 1};
        }

        static constexpr std::array<char, 4> kNalMarker4{0, 0, 0, 1};
        static constexpr std::array<char, 3> kNalMarker{0, 0, 1};
        static constexpr unsigned int kNalHeaderSize = 5u;
        static constexpr unsigned int kNalMarkerSize = kNalMarker.size();

    protected:
        const bytestring byte_data_;

    private:

        inline unsigned int ForbiddenZero() const {
            return byte_data_[kForbiddenZeroIndex] & kForbiddenZeroMask;
        }

        static constexpr const unsigned int kForbiddenZeroIndex = 0u;
        static constexpr const unsigned int kForbiddenZeroMask = 0x80u;

        const StitchContext context_;
        const unsigned int type_;
        const bool is_header_;
    };

/**
 *
 * @return The size of the header for a Nal in bytes
 */
inline size_t GetHeaderSize() {
    return Nal::kNalHeaderSize - Nal::kNalMarkerSize;
}

/**
 *
 * @return The size of the header for a Nal in bits
 */
inline size_t GetHeaderSizeInBits() {
    return GetHeaderSize() * CHAR_BIT;
}

/**
 *
 * @param data The byte stream
 * @return True if the byte stream represents a segment, false otherwise
 */
inline bool IsSegment(const bytestring &data) {
    auto type = PeekType(data);
    return type == NalUnitCodedSliceIDRWRADL ||
           type == NalUnitCodedSliceTrailR;
}

/**
 *
 * @param data The byte stream
 * @return True if the byte stream represents a key frame, false otherwise
 */
inline bool IsKeyframe(const bytestring &data) {
    return PeekType(data) == NalUnitCodedSliceIDRWRADL;
}


/**
 * Returns a Nal with type based on the value returned by PeekType on data. Since this takes no
 * headers, the Nal cannot be a SliceSegmentLayer. The Nal is instantiated on the heap
 * @param context The context surrounding the data
 * @param data The byte stream
 * @return A Nal with the correct type
 */
std::shared_ptr<Nal> Load(const StitchContext &context, const bytestring &data);

/**
 * Returns a Nal with type based on the value returned by PeekType on data
 * @param context The context surrounding the data
 * @param data The byte stream
 * @param headers The headers of the byte stream (necessary when the byte stream represents a
 * SliceSegmentLayer)
 * @return A Nal with the correct type
 */
SliceSegmentLayer Load(const StitchContext &context, const bytestring &data, const Headers &headers);

std::unique_ptr<Nal> LoadNal(const StitchContext &context, const bytestring &data, const Headers &headers);


// Header processing.
static void UpdateBitsInBytes(bytestring &data, unsigned int bitOffset, unsigned int newValue, unsigned int numberOfBitsToConsider = 0, unsigned int byteOffset = 0) {
    std::bitset<16> newValueAsBits(newValue);
    auto numberOfBits = floor(log2(newValue)) + 1;
    int indexInBitset = numberOfBits - 1;
    if (numberOfBitsToConsider)
        indexInBitset = numberOfBitsToConsider - 1;

    auto indexOfFirstByteToModify = bitOffset / 8 + byteOffset;
    auto bitIndex = bitOffset % 8;
    auto byteIt = data.begin() + indexOfFirstByteToModify;
    for (; indexInBitset >= 0; --indexInBitset) {
        bool desiredValue = newValueAsBits[indexInBitset];
        auto shift = 7 - bitIndex;
        if (((*byteIt >> shift) & 1) != desiredValue)
            *byteIt ^= (1u << shift);

        ++bitIndex;
        if (bitIndex == 8) {
            bitIndex = 0;
            ++byteIt;
        }
    }
}

static void UpdatePicOrderCntLsb(bytestring &data, unsigned int offset, unsigned int value, unsigned int numberOfBits, bool containsNalMarker) {
    UpdateBitsInBytes(data, offset, value, numberOfBits, containsNalMarker ? Nal::kNalMarker4.size() : 0);
}

}; //namespace stitching

#endif //HOMOMORPHIC_STITCHING_NAL_H
