#include "Configuration.h"

CUVIDDECODECREATEINFO DecodeConfiguration::AsCuvidCreateInfo(CUvideoctxlock lock,
                                                             const unsigned short int left,
                                                             const unsigned short int top) const {
    assert(left < SHRT_MAX);
    assert(top < SHRT_MAX);
    assert(width < SHRT_MAX);
    assert(height < SHRT_MAX);
    if(left > width)
        throw InvalidArgumentError("Left must be less than frame width", "left");
    else if(top > height)
        throw InvalidArgumentError("Top must be less than frame height", "top");
    else if(!codec.cudaId().has_value())
        throw GpuCudaRuntimeError("Codec does not have a CUDA equivalent", CUDA_ERROR_INVALID_VALUE);
    else {
        CUVIDDECODECREATEINFO info;
        memset(&info, 0, sizeof(info));
        info.ulWidth = 1920; // TODO: This should use coded width/height from configuration.
        info.ulHeight = 1088;
        info.ulNumDecodeSurfaces = decode_surfaces;
        info.CodecType = codec.cudaId().value();
        info.ChromaFormat = chroma_format;
        info.ulCreationFlags = creation_flags;
        info.display_area = {
                .left = static_cast<short>(left),
                .top = static_cast<short>(top),
                .right = static_cast<short>(width - left),
                .bottom = static_cast<short>(height - top),
        };
        info.OutputFormat = output_format;
        info.DeinterlaceMode = deinterlace_mode;
        info.ulTargetWidth = 1920;
        info.ulTargetHeight = 1088;
        info.ulNumOutputSurfaces = output_surfaces;
        info.vidLock = lock;

        info.ulMaxWidth = 1920;
        info.ulMaxHeight = 1088; // TODO: This should be max height.

        return info;
    }
}

unsigned int DecodeConfiguration::DefaultDecodeSurfaces() const {
    unsigned int decode_surfaces;

    if ((codec.cudaId() == cudaVideoCodec_H264) ||
        (codec.cudaId() == cudaVideoCodec_H264_SVC) ||
        (codec.cudaId() == cudaVideoCodec_H264_MVC)) {
        // Assume worst-case of 20 decode surfaces for H264
        decode_surfaces = 20;
    //} else if (codec.cudaId() == cudaVideoCodec_VP9) {
    //    decode_surfaces = 12;
    } else if (codec.cudaId() == cudaVideoCodec_HEVC) {
        // ref HEVC spec: A.4.1 General tier and level limits
        auto maxLumaPS = 35651584u; // currently assuming level 6.2, 8Kx4K
        auto maxDpbPicBuf = 6u;
        auto picSizeInSamplesY = width * height;
        unsigned int MaxDpbSize;
        if (picSizeInSamplesY <= (maxLumaPS >> 2))
            MaxDpbSize = maxDpbPicBuf * 4;
        else if (picSizeInSamplesY <= (maxLumaPS >> 1))
            MaxDpbSize = maxDpbPicBuf * 2;
        else if (picSizeInSamplesY <= ((3 * maxLumaPS) >> 2))
            MaxDpbSize = (maxDpbPicBuf * 4) / 3;
        else
            MaxDpbSize = maxDpbPicBuf;
        MaxDpbSize = MaxDpbSize < 16 ? MaxDpbSize : 16;
        decode_surfaces = MaxDpbSize + 4;
    } else {
        decode_surfaces = 8;
    }

    return decode_surfaces;
}
