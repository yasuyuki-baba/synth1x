#include "EditorBackgroundImage.h"

#if defined(IGRAPHICS_SKIA)
#include "include/codec/SkCodec.h"
#include "include/core/SkData.h"
#else
// Compile a private decoder for palette validation. NanoVG may compile its
// own copy, but these symbols never leave this translation unit.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "stb_image.h"
#endif

namespace synth1x::editor::native
{
bool validateBackgroundImage(const EditorPalette& palette,std::string& error)
{
    if(!palette.backgroundImage)return true;
    const auto& image=*palette.backgroundImage;
    if(!image.supported()) {
        error="Could not decode the background image. Choose an 8-bit PNG or JPEG.";return false;
    }
#if defined(IGRAPHICS_SKIA)
    const auto codec=SkCodec::MakeFromData(SkData::MakeWithoutCopy(image.bytes.data(),image.bytes.size()));
    if(!codec) {
        error="Could not decode the background image. Choose an 8-bit PNG or JPEG.";return false;
    }
    const auto info=codec->getInfo().makeColorType(kRGBA_8888_SkColorType);
    const int width=info.width(),height=info.height();
#else
    int width=0,height=0,channels=0;
    if(!stbi_info_from_memory(image.bytes.data(),static_cast<int>(image.bytes.size()),&width,&height,&channels)) {
        error="Could not decode the background image. Choose an 8-bit PNG or JPEG.";return false;
    }
#endif
    if(width<=0 || height<=0 || width>8192 || height>8192 || static_cast<std::uint64_t>(width)*height>16*1024*1024) {
        error="Choose a PNG or JPEG up to 8 MiB, 8192 pixels per side and 16 megapixels.";return false;
    }
#if defined(IGRAPHICS_SKIA)
    const auto rowBytes=info.minRowBytes();
    std::vector<std::uint8_t> pixels(info.computeByteSize(rowBytes));
    const bool decoded=codec->getPixels(info,pixels.data(),rowBytes)==SkCodec::kSuccess;
#else
    auto* pixels=stbi_load_from_memory(image.bytes.data(),static_cast<int>(image.bytes.size()),&width,&height,&channels,4);
    const bool decoded=pixels!=nullptr;
    stbi_image_free(pixels);
#endif
    if(!decoded){error="Could not decode the background image. Choose a valid PNG or JPEG.";return false;}
    return true;
}
}
