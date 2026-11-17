#include "EditorBackgroundImage.h"
#include "stb_image.h"

namespace synth2::editor::native
{
bool validateBackgroundImage(const EditorPalette& palette,std::string& error)
{
    if(!palette.backgroundImage)return true;
    const auto& image=*palette.backgroundImage;
    int width=0,height=0,channels=0;
    if(!image.supported() || !stbi_info_from_memory(image.bytes.data(),static_cast<int>(image.bytes.size()),&width,&height,&channels)) {
        error="Could not decode the background image. Choose an 8-bit PNG or JPEG.";return false;
    }
    if(width<=0 || height<=0 || width>8192 || height>8192 || static_cast<std::uint64_t>(width)*height>16*1024*1024) {
        error="Choose a PNG or JPEG up to 8 MiB, 8192 pixels per side and 16 megapixels.";return false;
    }
    auto* pixels=stbi_load_from_memory(image.bytes.data(),static_cast<int>(image.bytes.size()),&width,&height,&channels,4);
    if(!pixels){error="Could not decode the background image. Choose a valid PNG or JPEG.";return false;}
    stbi_image_free(pixels);return true;
}
}
