/**
 * @file
 * @brief Redrawing pixel art at twice its resolution.
 */
#ifndef UPSCALER_HPP
#define UPSCALER_HPP

#include <cstdint>
#include <vector>

/**
 * Redraws pixel art at twice its resolution with MMPX ("MMPX Style-Preserving Pixel-Art
 * Magnification", Morgan McGuire and Mara Gagiu, 2021). Curves and diagonals get finer pixel
 * steps, while the result stays pixel art: it only uses the original colors and never blurs.
 */
class Upscaler
{
public:
    /**
     * How many times larger than the original each side of an upscaled image is.
     */
    static constexpr int FACTOR = 2;

    /**
     * @param source width x height opaque ARGB pixels.
     * @param destination (width * FACTOR) x (height * FACTOR) pixels.
     */
    void upscale(const uint32_t* source, int width, int height, uint32_t* destination);

private:
    std::vector<uint32_t> padded; /**< The source with a border of repeated edge pixels. */
};

#endif // UPSCALER_HPP
