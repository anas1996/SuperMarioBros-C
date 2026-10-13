/**
 * @file
 * @brief Tests for the HD pixel-art upscaler.
 *
 * Checks basic properties of the output, and compares a hash of the output for a test image
 * with a known-good value, so that any change to the algorithm is noticed.
 *
 * Run with --print to output the current hash, e.g. after an intentional change.
 */
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

#include "Frontend/Upscaler.hpp"

namespace
{

constexpr uint64_t EXPECTED_HASH = 0xeabba0050f67ef45;

constexpr int WIDTH = 64;
constexpr int HEIGHT = 48;
constexpr uint32_t SKY = 0xff6888fc;
constexpr uint32_t BLACK = 0xff000000;
constexpr uint32_t WHITE = 0xfffcfcfc;
constexpr uint32_t GREEN = 0xff00a800;
constexpr uint32_t BROWN = 0xff881400;

int failures = 0;

void check(bool condition, const char* description)
{
    std::printf("%s: %s\n", condition ? "ok" : "FAILED", description);
    failures += condition ? 0 : 1;
}

std::vector<uint32_t> upscale(const std::vector<uint32_t>& image, int width, int height)
{
    std::vector<uint32_t> result(static_cast<std::size_t>(width) * height * Upscaler::FACTOR * Upscaler::FACTOR);
    Upscaler().upscale(image.data(), width, height, result.data());
    return result;
}

/**
 * Pixel-art features: an outlined disc (curves), a hill (diagonals), a one-pixel line,
 * isolated pixels, and a checkerboard (dithering).
 */
std::vector<uint32_t> makeTestImage()
{
    std::vector<uint32_t> image(WIDTH * HEIGHT, SKY);
    const auto set = [&](int x, int y, uint32_t color) {
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
        {
            image[y * WIDTH + x] = color;
        }
    };

    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            const int dx = x - 14;
            const int dy = y - 12;
            const int distance = dx * dx + dy * dy;
            if (distance <= 64)
            {
                set(x, y, distance >= 49 ? BLACK : WHITE);
            }
            if (y >= 30 && y >= 30 + (x - 40 < 0 ? 40 - x : x - 40))
            {
                set(x, y, GREEN);
            }
        }
    }
    for (int x = 2; x < 30; x++)
    {
        set(x, 26, BLACK);
    }
    set(50, 4, BROWN);
    set(54, 8, WHITE);
    for (int y = 36; y < 44; y++)
    {
        for (int x = 2; x < 10; x++)
        {
            set(x, y, ((x + y) % 2) ? BROWN : WHITE);
        }
    }
    return image;
}

uint64_t hash(const std::vector<uint32_t>& pixels)
{
    uint64_t value = 0xcbf29ce484222325ull;
    for (const uint32_t pixel : pixels)
    {
        for (int shift = 0; shift < 32; shift += 8)
        {
            value = (value ^ ((pixel >> shift) & 0xff)) * 0x100000001b3ull;
        }
    }
    return value;
}

} // namespace

int main(int argc, char** argv)
{
    const std::vector<uint32_t> image = makeTestImage();
    const std::vector<uint32_t> result = upscale(image, WIDTH, HEIGHT);

    if (argc > 1 && std::strcmp(argv[1], "--print") == 0)
    {
        std::printf("0x%016" PRIx64 "\n", hash(result));
        return 0;
    }

    const std::vector<uint32_t> flat(WIDTH * HEIGHT, SKY);
    const std::vector<uint32_t> flatResult = upscale(flat, WIDTH, HEIGHT);
    check(std::set<uint32_t>(flatResult.begin(), flatResult.end()) == std::set<uint32_t>{SKY},
          "a flat image stays flat");

    bool opaque = true;
    for (const uint32_t pixel : result)
    {
        opaque = opaque && (pixel >> 24) == 0xff;
    }
    check(opaque, "the output is opaque");

    const std::set<uint32_t> inputColors(image.begin(), image.end());
    const std::set<uint32_t> outputColors(result.begin(), result.end());
    check(outputColors == inputColors, "the output uses exactly the colors of the input");

    // Mirroring the input mirrors the output
    std::vector<uint32_t> mirrored(image.size());
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            mirrored[y * WIDTH + x] = image[y * WIDTH + (WIDTH - 1 - x)];
        }
    }
    const std::vector<uint32_t> mirroredResult = upscale(mirrored, WIDTH, HEIGHT);
    const int scaledWidth = WIDTH * Upscaler::FACTOR;
    bool symmetric = true;
    for (int y = 0; y < HEIGHT * Upscaler::FACTOR && symmetric; y++)
    {
        for (int x = 0; x < scaledWidth; x++)
        {
            symmetric = symmetric && mirroredResult[y * scaledWidth + x] == result[y * scaledWidth + (scaledWidth - 1 - x)];
        }
    }
    check(symmetric, "mirroring the input mirrors the output");

    check(hash(result) == EXPECTED_HASH, "the output matches the known-good result");

    std::printf(failures == 0 ? "PASSED\n" : "FAILED\n");
    return failures == 0 ? 0 : 1;
}
