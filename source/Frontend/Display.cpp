#include <algorithm>
#include <cmath>
#include <cstring>

#include "../Configuration.hpp"
#include "../Constants.hpp"
#include "../Emulation/Palette.hpp"

#include "Display.hpp"

namespace
{

constexpr uint32_t OPAQUE = 0xff000000;
constexpr uint32_t WHITE = 0xfffcfcfc;
constexpr uint32_t GRAY = 0xffbcbcbc;
constexpr uint32_t BLACK = 0xff000000;

/**
 * How long the name of a newly selected video filter is shown, in frames.
 */
constexpr int NOTICE_DURATION = 90;

const char* filterName(VideoFilter filter)
{
    switch (filter)
    {
    case VideoFilter::Hd: return "HD";
    case VideoFilter::Pixel: return "PIXEL";
    case VideoFilter::Crt: return "CRT";
    }
    return "";
}

/**
 * Index of the 2-bit color of a pixel in an 8x8 CHR tile (0 is transparent).
 */
int tilePixel(const uint8_t* chr, int tile, int x, int y)
{
    const uint8_t* pattern = chr + tile * 16;
    const int bit = 7 - x;
    return ((pattern[y] >> bit) & 1) | (((pattern[y + 8] >> bit) & 1) << 1);
}

/**
 * CHR tile of a character in the game's font (in the background pattern table).
 */
int glyphTile(char c)
{
    constexpr int BACKGROUND = 256;
    if (c >= '0' && c <= '9')
    {
        return BACKGROUND + (c - '0');
    }
    if (c >= 'A' && c <= 'Z')
    {
        return BACKGROUND + 10 + (c - 'A');
    }
    if (c == '-')
    {
        return BACKGROUND + 0x28;
    }
    if (c == '!')
    {
        return BACKGROUND + 0x2b;
    }
    return -1; // Blank
}

/**
 * A 64x64 window icon of small Mario, from the sprite tiles and colors used by the game.
 */
SdlPtr<SDL_Surface> createIcon(const uint8_t* chr)
{
    constexpr int TILES[2][2] = {{0x32, 0x33}, {0x34, 0x35}}; // Walking frame 1
    constexpr uint8_t COLORS[4] = {0x00, 0x16, 0x27, 0x18};   // Mario's sprite palette
    constexpr int SCALE = 4;

    SdlPtr<SDL_Surface> icon(SDL_CreateRGBSurfaceWithFormat(0, 16 * SCALE, 16 * SCALE, 32, SDL_PIXELFORMAT_ARGB8888));
    if (!icon)
    {
        return icon;
    }

    auto* pixels = static_cast<uint32_t*>(icon->pixels);
    const int stride = icon->pitch / static_cast<int>(sizeof(uint32_t));
    for (int y = 0; y < 16 * SCALE; y++)
    {
        for (int x = 0; x < 16 * SCALE; x++)
        {
            const int sx = x / SCALE;
            const int sy = y / SCALE;
            const int color = tilePixel(chr, TILES[sy / 8][sx / 8], sx % 8, sy % 8);
            pixels[y * stride + x] = color == 0 ? 0 : OPAQUE | DEFAULT_PALETTE[COLORS[color]];
        }
    }
    return icon;
}

/**
 * Brightness of a row of the CRT effect. Each line of the picture fades out towards its
 * bottom edge, like the gaps between the scanlines of a CRT. The smooth falloff keeps the
 * lines even when the picture is not enlarged by a whole number.
 */
double scanlineBrightness(int row, int height)
{
    const double linesPerRow = static_cast<double>(RENDER_HEIGHT) / height;
    if (linesPerRow > 0.5)
    {
        return 1.0; // Too small for scanlines
    }

    const double line = (row + 0.5) * linesPerRow;
    const double positionInLine = line - std::floor(line); // 0 at the top, 1 at the bottom
    const double t = std::clamp((positionInLine - 0.45) / 0.55, 0.0, 1.0);
    return 1.0 - 0.42 * t * t * (3.0 - 2.0 * t);
}

/**
 * The largest whole-number scale at which the window fits on the screen.
 */
int fittingScale()
{
    constexpr int WINDOW_DECORATIONS = 64; // Title bar and borders, roughly
    SDL_Rect bounds;
    if (SDL_GetDisplayUsableBounds(0, &bounds) != 0)
    {
        return 3;
    }
    return std::max(1, std::min(bounds.w / RENDER_WIDTH, (bounds.h - WINDOW_DECORATIONS) / RENDER_HEIGHT));
}

} // namespace

Display::Display(const Configuration& config, const uint8_t* chr) :
    chr(chr),
    integerScaling(config.integerScaling),
    filter(config.filter),
    composedFrame(RENDER_WIDTH * RENDER_HEIGHT),
    hdFrame(RENDER_WIDTH * Upscaler::FACTOR * RENDER_HEIGHT * Upscaler::FACTOR)
{
    Uint32 windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (config.fullscreen)
    {
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    const int scale = config.scale > 0 ? config.scale : fittingScale();
    window.reset(SDL_CreateWindow(APP_TITLE,
                                  SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED,
                                  RENDER_WIDTH * scale,
                                  RENDER_HEIGHT * scale,
                                  windowFlags));
    if (!window)
    {
        throw SdlError("SDL_CreateWindow()");
    }
    SDL_SetWindowMinimumSize(window.get(), RENDER_WIDTH, RENDER_HEIGHT);
    SDL_ShowCursor(config.fullscreen ? SDL_DISABLE : SDL_ENABLE);
    if (SdlPtr<SDL_Surface> icon = createIcon(chr))
    {
        SDL_SetWindowIcon(window.get(), icon.get());
    }

    renderer.reset(SDL_CreateRenderer(window.get(), -1, config.vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
    if (!renderer)
    {
        throw SdlError("SDL_CreateRenderer()");
    }

    // Pixels stay sharp when the frame is enlarged
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    frameTexture.reset(SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, RENDER_WIDTH, RENDER_HEIGHT));
    if (!frameTexture)
    {
        throw SdlError("SDL_CreateTexture()");
    }
    SDL_SetTextureBlendMode(frameTexture.get(), SDL_BLENDMODE_NONE);

    hdTexture.reset(SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                      RENDER_WIDTH * Upscaler::FACTOR, RENDER_HEIGHT * Upscaler::FACTOR));
    if (hdTexture)
    {
        SDL_SetTextureBlendMode(hdTexture.get(), SDL_BLENDMODE_NONE);
    }
    else if (filter == VideoFilter::Hd)
    {
        filter = VideoFilter::Pixel;
    }
}

SDL_Rect Display::getOutputRect() const
{
    int outputWidth = 0;
    int outputHeight = 0;
    SDL_GetRendererOutputSize(renderer.get(), &outputWidth, &outputHeight);

    // Fit the picture, keeping its aspect ratio
    double scale = std::min(static_cast<double>(outputWidth) / RENDER_WIDTH, static_cast<double>(outputHeight) / RENDER_HEIGHT);
    if (integerScaling)
    {
        scale = std::max(1.0, std::floor(scale));
    }

    const int width = static_cast<int>(std::lround(RENDER_WIDTH * scale));
    const int height = static_cast<int>(std::lround(RENDER_HEIGHT * scale));
    return SDL_Rect{(outputWidth - width) / 2, (outputHeight - height) / 2, width, height};
}

void Display::drawSharp(SDL_Texture* texture, int width, int height, const SDL_Rect& outputRect)
{
    const bool wholeMultiple = outputRect.w % width == 0 && outputRect.h % height == 0;
    if (wholeMultiple || !SDL_RenderTargetSupported(renderer.get()))
    {
        SDL_RenderCopy(renderer.get(), texture, nullptr, &outputRect);
        return;
    }

    // "Sharp bilinear" scaling: enlarge by a whole number with sharp pixels, then smoothly
    // scale the rest of the way, so that every pixel ends up the same size
    const int factor = std::max(1, std::min(outputRect.w / width, outputRect.h / height));
    if (width * factor != scaledWidth || height * factor != scaledHeight)
    {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        scaledTexture.reset(SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET,
                                              width * factor, height * factor));
        scaledWidth = scaledTexture ? width * factor : 0;
        scaledHeight = scaledTexture ? height * factor : 0;
    }
    if (!scaledTexture)
    {
        SDL_RenderCopy(renderer.get(), texture, nullptr, &outputRect);
        return;
    }

    SDL_SetRenderTarget(renderer.get(), scaledTexture.get());
    SDL_RenderCopy(renderer.get(), texture, nullptr, nullptr);
    SDL_SetRenderTarget(renderer.get(), nullptr);
    SDL_RenderCopy(renderer.get(), scaledTexture.get(), nullptr, &outputRect);
}

void Display::drawCrtEffect(const SDL_Rect& outputRect)
{
    // The effect is generated pixel for pixel at the output size, so it stays crisp at any scale
    if (!crtTexture || outputRect.w != crtRect.w || outputRect.h != crtRect.h)
    {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
        crtTexture.reset(SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, outputRect.w, outputRect.h));
        if (!crtTexture)
        {
            filter = VideoFilter::Pixel;
            return;
        }

        // Phosphor stripes (red, green, blue) when there is room for them
        const bool stripes = outputRect.w >= RENDER_WIDTH * 3;
        constexpr double STRIPES[3][3] = {{1.0, 0.86, 0.86}, {0.86, 1.0, 0.86}, {0.86, 0.86, 1.0}};

        std::vector<uint32_t> pixels(static_cast<std::size_t>(outputRect.w) * outputRect.h);
        for (int y = 0; y < outputRect.h; y++)
        {
            // Each row repeats a pattern of three colors
            const double brightness = scanlineBrightness(y, outputRect.h);
            uint32_t pattern[3];
            for (int i = 0; i < 3; i++)
            {
                uint32_t color = OPAQUE;
                for (int channel = 0; channel < 3; channel++)
                {
                    const double level = brightness * (stripes ? STRIPES[i][channel] : 1.0);
                    color |= static_cast<uint32_t>(std::lround(255 * level)) << (16 - 8 * channel);
                }
                pattern[i] = color;
            }

            uint32_t* row = &pixels[static_cast<std::size_t>(y) * outputRect.w];
            for (int x = 0; x < outputRect.w; x++)
            {
                row[x] = pattern[x % 3];
            }
        }
        SDL_UpdateTexture(crtTexture.get(), nullptr, pixels.data(), outputRect.w * static_cast<int>(sizeof(uint32_t)));
        SDL_SetTextureBlendMode(crtTexture.get(), SDL_BLENDMODE_MOD);
        crtRect = outputRect;
    }

    SDL_RenderCopy(renderer.get(), crtTexture.get(), nullptr, &outputRect);
}

void Display::drawText(int y, const char* text, uint32_t color)
{
    const int length = static_cast<int>(std::strlen(text));
    const int left = (RENDER_WIDTH - length * 8) / 2;

    // Draw a drop shadow first, then the text
    const struct
    {
        int offset;
        uint32_t color;
    } layers[] = {{1, BLACK}, {0, color}};

    for (const auto& layer : layers)
    {
        for (int i = 0; i < length; i++)
        {
            const int tile = glyphTile(text[i]);
            if (tile < 0)
            {
                continue;
            }
            for (int row = 0; row < 8; row++)
            {
                for (int column = 0; column < 8; column++)
                {
                    if (tilePixel(chr, tile, column, row) != 0)
                    {
                        const int x = left + i * 8 + column + layer.offset;
                        composedFrame[(y + row + layer.offset) * RENDER_WIDTH + x] = layer.color;
                    }
                }
            }
        }
    }
}

const uint32_t* Display::compose(const uint32_t* frame, bool paused)
{
    if (!paused && noticeFrames == 0)
    {
        return frame;
    }

    if (paused)
    {
        // Dim the frame to half brightness and show the pause screen over it
        std::transform(frame, frame + composedFrame.size(), composedFrame.begin(), [](uint32_t pixel) {
            return OPAQUE | ((pixel >> 1) & 0x7f7f7f);
        });
        drawText(104, "PAUSED", WHITE);
        drawText(124, "PRESS P TO RESUME", GRAY);
    }
    else
    {
        std::copy(frame, frame + composedFrame.size(), composedFrame.begin());
    }

    if (noticeFrames > 0)
    {
        drawText(36, filterName(filter), WHITE);
        noticeFrames--;
    }
    return composedFrame.data();
}

void Display::present(const uint32_t* frame, bool paused)
{
    frame = compose(frame, paused);

    SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
    SDL_RenderClear(renderer.get());

    const SDL_Rect outputRect = getOutputRect();
    if (filter == VideoFilter::Hd)
    {
        constexpr int HD_WIDTH = RENDER_WIDTH * Upscaler::FACTOR;
        constexpr int HD_HEIGHT = RENDER_HEIGHT * Upscaler::FACTOR;
        upscaler.upscale(frame, RENDER_WIDTH, RENDER_HEIGHT, hdFrame.data());
        SDL_UpdateTexture(hdTexture.get(), nullptr, hdFrame.data(), HD_WIDTH * static_cast<int>(sizeof(uint32_t)));
        drawSharp(hdTexture.get(), HD_WIDTH, HD_HEIGHT, outputRect);
    }
    else
    {
        SDL_UpdateTexture(frameTexture.get(), nullptr, frame, RENDER_WIDTH * static_cast<int>(sizeof(uint32_t)));
        drawSharp(frameTexture.get(), RENDER_WIDTH, RENDER_HEIGHT, outputRect);
        if (filter == VideoFilter::Crt)
        {
            drawCrtEffect(outputRect);
        }
    }

    SDL_RenderPresent(renderer.get());
}

void Display::cycleFilter()
{
    switch (filter)
    {
    case VideoFilter::Hd: filter = VideoFilter::Pixel; break;
    case VideoFilter::Pixel: filter = VideoFilter::Crt; break;
    case VideoFilter::Crt: filter = hdTexture ? VideoFilter::Hd : VideoFilter::Pixel; break;
    }
    noticeFrames = NOTICE_DURATION;
}

void Display::toggleFullscreen()
{
    const bool fullscreen = (SDL_GetWindowFlags(window.get()) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
    SDL_SetWindowFullscreen(window.get(), fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_ShowCursor(fullscreen ? SDL_ENABLE : SDL_DISABLE);
}
