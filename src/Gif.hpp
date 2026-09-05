#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bakery
{

/* An animated GIF written from frames that already share one palette.

   The pipelines this feeds index into a fixed palette, so the colours are
   carried through exactly rather than quantised: entry 0 is transparent and
   the rest keep the order they were collected in. A frame carrying a colour
   the palette has no room for is a failure, not something to approximate. */
class Gif
{
  public:
    static constexpr int MaxColours = 256;

    /* RGBA pixels, tightly packed, all frames the same size. Every fully
       transparent pixel becomes entry 0. */
    [[nodiscard]] bool addFrame(const uint8_t* rgba, int width, int height);

    /* delayTicks is held per frame at 60 Hz, the rate the games run at. */
    [[nodiscard]] bool write(const std::string& path, int delayTicks) const;

    [[nodiscard]] int colourCount() const { return static_cast<int>(palette_.size()); }
    [[nodiscard]] const std::string& error() const { return error_; }

  private:
    int                          width_  = 0;
    int                          height_ = 0;
    std::vector<uint32_t>        palette_; // 0xRRGGBB, entry 0 is the transparent slot
    std::vector<std::vector<uint8_t>> frames_;
    mutable std::string          error_;

    int indexOf(uint32_t rgb);
};

} // namespace bakery
