/**
 * @file
 * @brief Video utility functions.
 */
#ifndef VIDEO_HPP
#define VIDEO_HPP

#include <SDL.h>

/**
 * Generate a texture for a scanline overlay effect.
 */
SDL_Texture* generateScanlineTexture(SDL_Renderer* renderer);

#endif // VIDEO_HPP
