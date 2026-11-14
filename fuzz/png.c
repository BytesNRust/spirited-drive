/* PNG through stb_image: the boot and logo art, the envmap, every mask */
#include "fuzz/fuzz.h"
#include "formats/png.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    PngImage image;
    if (png_read(data, size, &image) != PNG_OK) return 0;
    fuzz_touch(image.pixels, (size_t)image.width * image.height * (image.indexed ? 1u : 4u));
    png_free(image.pixels);
    return 0;
}
