/**
 * @file
 * @brief Window and video output.
 */
#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include <cstdint>
#include <vector>

#include "../Configuration.hpp"

#include "Sdl.hpp"
#include "Upscaler.hpp"

/**
 * The game window. Scales the picture to fit while keeping its aspect ratio, through one of
 * the video filters (HD, pixel or CRT), and draws the pause screen.
 */
class Display
{
public:
    /**
     * @param config video settings.
     * @param chr the game's CHR data, used for the window icon and on-screen text.
     * @throws SdlError if the window cannot be created.
     */
    Display(const Configuration& config, const uint8_t* chr);

    /**
     * Show a frame.
     *
     * @param frame 256x240 ARGB pixels.
     * @param paused whether to dim the frame and show the pause screen.
     */
    void present(const uint32_t* frame, bool paused);

    /**
     * Switch to the next video filter (HD, pixel, CRT) and briefly show its name.
     */
    void cycleFilter();

    void toggleFullscreen();

private:
    const uint8_t* chr;
    bool integerScaling;
    VideoFilter filter;
    int noticeFrames = 0; /**< How much longer the filter's name is shown. */

    SdlPtr<SDL_Window> window;
    SdlPtr<SDL_Renderer> renderer;
    SdlPtr<SDL_Texture> frameTexture;   /**< The frame at its native resolution. */
    SdlPtr<SDL_Texture> hdTexture;      /**< The frame's pixel art redrawn at twice the resolution. */
    SdlPtr<SDL_Texture> scaledTexture;  /**< A frame enlarged by a whole number (for sharp scaling). */
    SdlPtr<SDL_Texture> crtTexture;     /**< Scanline and phosphor mask, at the output resolution. */
    int scaledWidth = 0;
    int scaledHeight = 0;
    SDL_Rect crtRect{};
    Upscaler upscaler;
    std::vector<uint32_t> composedFrame; /**< The frame with the pause screen or notices drawn over it. */
    std::vector<uint32_t> hdFrame;

    SDL_Rect getOutputRect() const;
    const uint32_t* compose(const uint32_t* frame, bool paused);
    void drawSharp(SDL_Texture* texture, int width, int height, const SDL_Rect& outputRect);
    void drawCrtEffect(const SDL_Rect& outputRect);
    void drawText(int y, const char* text, uint32_t color);
};

#endif // DISPLAY_HPP
