#include "Gif.hpp"
#include "Renderer.hpp"

#include <SDL3/SDL.h>

#include <string>
#include <vector>

/* Bakery: run a shader with no window and keep what it drew, as a GIF.

   This is the harness the effects hang off. --selftest exercises the whole
   path - device, target, readback, encoder - without needing a shader, so a
   broken bake can be told apart from a broken shader. */

namespace
{

/* A bar crossing a field, in a handful of colours: enough to prove the
   encoder carries colours through exactly and holds a transparent cutout. */
void syntheticFrame(std::vector<uint8_t>& rgba, int w, int h, int step, int steps)
{
    rgba.assign(static_cast<size_t>(w) * h * 4, 0);
    const int bar = (w * step) / steps;
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            uint8_t* px = rgba.data() + (static_cast<size_t>(y) * w + x) * 4;
            if (y < h / 3)
            {
                continue; // left transparent
            }
            const bool lit = x >= bar && x < bar + (w / 8);
            px[0]          = lit ? 255 : 108;
            px[1]          = lit ? 255 : 40;
            px[2]          = lit ? 255 : 35;
            px[3]          = 255;
        }
    }
}

} // namespace

int main(int argc, char** argv)
{
    int         width    = 384;
    int         height   = 216;
    int         frames   = 9;
    int         hold     = 5;
    bool        selftest = false;
    std::string out      = "out.gif";

    for (int i = 1; i < argc; i++)
    {
        const std::string arg = argv[i];
        if (arg == "--size" && i + 2 < argc)
        {
            width  = SDL_atoi(argv[++i]);
            height = SDL_atoi(argv[++i]);
        }
        else if (arg == "--frames" && i + 1 < argc)
        {
            frames = SDL_atoi(argv[++i]);
        }
        else if (arg == "--hold" && i + 1 < argc)
        {
            hold = SDL_atoi(argv[++i]);
        }
        else if (arg == "--out" && i + 1 < argc)
        {
            out = argv[++i];
        }
        else if (arg == "--selftest")
        {
            selftest = true;
        }
        else
        {
            SDL_Log("usage: bakery --selftest [--size W H] [--frames N] [--hold T] [--out FILE]");
            return 2;
        }
    }

    bakery::Gif          gif;
    std::vector<uint8_t> rgba;

    if (selftest)
    {
        for (int f = 0; f < frames; f++)
        {
            syntheticFrame(rgba, width, height, f, frames);
            if (!gif.addFrame(rgba.data(), width, height))
            {
                SDL_Log("bake failed: %s", gif.error().c_str());
                return 1;
            }
        }
    }
    else
    {
        bakery::Renderer renderer(width, height);
        if (!renderer.ok())
        {
            return 1;
        }
        for (int f = 0; f < frames; f++)
        {
            if (renderer.beginPass() == nullptr)
            {
                return 1;
            }
            renderer.endPass();
            if (!renderer.read(width, height, rgba)
                || !gif.addFrame(rgba.data(), width, height))
            {
                SDL_Log("bake failed: %s", gif.error().c_str());
                return 1;
            }
        }
    }

    if (!gif.write(out, hold))
    {
        SDL_Log("bake failed: %s", gif.error().c_str());
        return 1;
    }
    SDL_Log("wrote %s: %d frames, %dx%d, %d colours", out.c_str(), frames, width, height,
            gif.colourCount());
    return 0;
}
