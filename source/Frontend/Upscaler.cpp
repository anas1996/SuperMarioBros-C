// MMPX pixel-art magnification.
//
// Algorithm: "MMPX Style-Preserving Pixel-Art Magnification", Morgan McGuire and Mara Gagiu,
// Journal of Computer Graphics Techniques (JCGT) 10(2), 2021. https://jcgt.org/published/0010/02/04/
//
// The rules follow the authors' reference implementation, published under the MIT license:
//
//   Copyright 2020 Morgan McGuire & Mara Gagiu
//
//   Permission is hereby granted, free of charge, to any person obtaining a copy of this
//   software and associated documentation files (the "Software"), to deal in the Software
//   without restriction, including without limitation the rights to use, copy, modify, merge,
//   publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
//   to whom the Software is furnished to do so, subject to the following conditions:
//
//   The above copyright notice and this permission notice shall be included in all copies or
//   substantial portions of the Software.
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
//   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
//   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
//   FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
//   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//   DEALINGS IN THE SOFTWARE.

#include <algorithm>
#include <cstddef>

#include "Upscaler.hpp"

namespace
{

/**
 * MMPX reads up to this many pixels away from the pixel it enlarges.
 */
constexpr int BORDER = 3;

/**
 * Brightness used to decide which color wins where two shapes meet. The reference weights the
 * channel sum by alpha; all colors here are opaque, which gives the same ordering.
 */
inline uint32_t luma(uint32_t color)
{
    return ((color >> 16) & 0xff) + ((color >> 8) & 0xff) + (color & 0xff);
}

inline bool allEqual(uint32_t b, uint32_t a0, uint32_t a1)
{
    return b == a0 && b == a1;
}

inline bool allEqual(uint32_t b, uint32_t a0, uint32_t a1, uint32_t a2)
{
    return b == a0 && b == a1 && b == a2;
}

inline bool allEqual(uint32_t b, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    return b == a0 && b == a1 && b == a2 && b == a3;
}

inline bool anyEqual(uint32_t b, uint32_t a0, uint32_t a1, uint32_t a2)
{
    return b == a0 || b == a1 || b == a2;
}

inline bool noneEqual(uint32_t b, uint32_t a0, uint32_t a1)
{
    return b != a0 && b != a1;
}

inline bool noneEqual(uint32_t b, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    return b != a0 && b != a1 && b != a2 && b != a3;
}

/**
 * One MMPX pass. `source` points at the first pixel of a width x height image inside a buffer
 * with a BORDER-pixel border on every side and row stride `stride`.
 *
 * Neighborhood of the pixel E, named as in the paper:
 *
 *             P
 *         A   B   C
 *     Q   D   E   F   R
 *         G   H   I
 *             S
 *
 * E becomes the 2x2 block   J K
 *                           L M
 */
void magnify(const uint32_t* source, std::ptrdiff_t stride, int width, int height, uint32_t* destination)
{
    const std::ptrdiff_t s = stride;
    const std::ptrdiff_t outputWidth = 2 * static_cast<std::ptrdiff_t>(width);

    for (int y = 0; y < height; y++)
    {
        const uint32_t* row = source + y * s;
        uint32_t* out0 = destination + (2 * y) * outputWidth;
        uint32_t* out1 = out0 + outputWidth;

        int x = 0;
        while (x < width)
        {
            const uint32_t* p = row + x;
            const uint32_t E = p[0];

            // Most of a frame is flat: copy runs of pixels whose 3x3 neighborhood is all E
            if (p[-s - 1] == E && p[-s] == E && p[-1] == E && p[s - 1] == E && p[s] == E)
            {
                int run = 0;
                while (x + run < width && p[run - s + 1] == E && p[run + 1] == E && p[run + s + 1] == E)
                {
                    run++;
                }
                if (run > 0)
                {
                    std::fill(out0 + 2 * x, out0 + 2 * (x + run), E);
                    std::fill(out1 + 2 * x, out1 + 2 * (x + run), E);
                    x += run;
                    continue;
                }
            }

            const uint32_t A = p[-s - 1], B = p[-s], C = p[-s + 1];
            const uint32_t D = p[-1], F = p[1];
            const uint32_t G = p[s - 1], H = p[s], I = p[s + 1];

            // By default, E is simply doubled
            uint32_t J = E, K = E, L = E, M = E;

            if (A != E || B != E || C != E || D != E || F != E || G != E || H != E || I != E)
            {
                const uint32_t P = p[-2 * s], S = p[2 * s];
                const uint32_t Q = p[-2], R = p[2];
                const uint32_t Bl = luma(B), Dl = luma(D), El = luma(E), Fl = luma(F), Hl = luma(H);

                // 1:1 slopes
                if ((D == B && D != H && D != F) && (El >= Dl || E == A) && anyEqual(E, A, C, G) &&
                    (El < Dl || A != D || E != P || E != Q))
                {
                    J = D;
                }
                if ((B == F && B != D && B != H) && (El >= Bl || E == C) && anyEqual(E, A, C, I) &&
                    (El < Bl || C != B || E != P || E != R))
                {
                    K = B;
                }
                if ((H == D && H != F && H != B) && (El >= Hl || E == G) && anyEqual(E, A, G, I) &&
                    (El < Hl || G != H || E != S || E != Q))
                {
                    L = H;
                }
                if ((F == H && F != B && F != D) && (El >= Fl || E == I) && anyEqual(E, C, G, I) &&
                    (El < Fl || I != H || E != R || E != S))
                {
                    M = F;
                }

                // Intersections
                if (E != F && allEqual(E, C, I, D, Q) && allEqual(F, B, H) && F != p[3])
                {
                    K = M = F;
                }
                if (E != D && allEqual(E, A, G, F, R) && allEqual(D, B, H) && D != p[-3])
                {
                    J = L = D;
                }
                if (E != H && allEqual(E, G, I, B, P) && allEqual(H, D, F) && H != p[3 * s])
                {
                    L = M = H;
                }
                if (E != B && allEqual(E, A, C, H, S) && allEqual(B, D, F) && B != p[-3 * s])
                {
                    J = K = B;
                }

                // Triangle tips
                if (Bl < El && allEqual(E, G, H, I, S) && noneEqual(E, A, D, C, F))
                {
                    J = K = B;
                }
                if (Hl < El && allEqual(E, A, B, C, P) && noneEqual(E, D, G, I, F))
                {
                    L = M = H;
                }
                if (Fl < El && allEqual(E, A, D, G, Q) && noneEqual(E, B, C, I, H))
                {
                    K = M = F;
                }
                if (Dl < El && allEqual(E, C, F, I, R) && noneEqual(E, B, A, G, H))
                {
                    J = L = D;
                }

                // 2:1 slopes
                if (H != B)
                {
                    if (H != A && H != E && H != C)
                    {
                        if (allEqual(H, G, F, R) && noneEqual(H, D, p[-s + 2]))
                        {
                            L = M;
                        }
                        if (allEqual(H, I, D, Q) && noneEqual(H, F, p[-s - 2]))
                        {
                            M = L;
                        }
                    }
                    if (B != I && B != G && B != E)
                    {
                        if (allEqual(B, A, F, R) && noneEqual(B, D, p[s + 2]))
                        {
                            J = K;
                        }
                        if (allEqual(B, C, D, Q) && noneEqual(B, F, p[s - 2]))
                        {
                            K = J;
                        }
                    }
                }
                if (F != D)
                {
                    if (D != I && D != E && D != C)
                    {
                        if (allEqual(D, A, H, S) && noneEqual(D, B, p[2 * s + 1]))
                        {
                            J = L;
                        }
                        if (allEqual(D, G, B, P) && noneEqual(D, H, p[-2 * s + 1]))
                        {
                            L = J;
                        }
                    }
                    if (F != E && F != A && F != G)
                    {
                        if (allEqual(F, C, H, S) && noneEqual(F, B, p[2 * s - 1]))
                        {
                            K = M;
                        }
                        if (allEqual(F, I, B, P) && noneEqual(F, H, p[-2 * s - 1]))
                        {
                            M = K;
                        }
                    }
                }
            }

            out0[2 * x] = J;
            out0[2 * x + 1] = K;
            out1[2 * x] = L;
            out1[2 * x + 1] = M;
            x++;
        }
    }
}

} // namespace

void Upscaler::upscale(const uint32_t* source, int width, int height, uint32_t* destination)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    // Copy the image into a buffer with a border of repeated edge pixels, so that the
    // neighborhood of every pixel can be read without bounds checks
    const std::ptrdiff_t stride = width + 2 * BORDER;
    padded.resize(static_cast<std::size_t>(stride) * (height + 2 * BORDER));
    for (int y = -BORDER; y < height + BORDER; y++)
    {
        const uint32_t* sourceRow = source + static_cast<std::ptrdiff_t>(std::clamp(y, 0, height - 1)) * width;
        uint32_t* paddedRow = padded.data() + (y + BORDER) * stride;
        for (int x = -BORDER; x < width + BORDER; x++)
        {
            paddedRow[x + BORDER] = sourceRow[std::clamp(x, 0, width - 1)] | 0xff000000u;
        }
    }

    magnify(padded.data() + BORDER * stride + BORDER, stride, width, height, destination);
}
