#include <vector>

#include "../Constants.hpp"

#include "Video.hpp"

SDL_Texture* generateScanlineTexture(SDL_Renderer* renderer)
{
    // Create a scanline texture for 3x rendering
    constexpr int width = RENDER_WIDTH * 3;
    constexpr int height = RENDER_HEIGHT * 3;
    constexpr uint32_t columnColors[3] = {0xfffdd6c7, 0xffbef5e1, 0xffcfe2ff};

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);
    if (texture == nullptr)
    {
        return nullptr;
    }

    std::vector<uint32_t> pixels(width * height);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            pixels[y * width + x] = columnColors[x % 3];
        }
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_MOD);
    SDL_UpdateTexture(texture, nullptr, pixels.data(), sizeof(uint32_t) * width);

    return texture;
}
