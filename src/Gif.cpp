#include "Gif.hpp"

#include <algorithm>
#include <cstdio>
#include <unordered_map>

namespace bakery
{
namespace
{

/* GIF codes are packed low bits first and run across byte boundaries. */
class BitWriter
{
  public:
    void write(uint32_t code, int length)
    {
        accumulator_ |= code << bits_;
        bits_ += length;
        while (bits_ >= 8)
        {
            bytes.push_back(static_cast<uint8_t>(accumulator_ & 0xFFu));
            accumulator_ >>= 8;
            bits_ -= 8;
        }
    }

    void flush()
    {
        if (bits_ > 0)
        {
            bytes.push_back(static_cast<uint8_t>(accumulator_ & 0xFFu));
            accumulator_ = 0;
            bits_        = 0;
        }
    }

    std::vector<uint8_t> bytes;

  private:
    uint32_t accumulator_ = 0;
    int      bits_        = 0;
};

/* Smallest power-of-two colour table that holds n entries, minimum 2. */
int tableBits(int n)
{
    int bits = 1;
    while ((1 << bits) < n)
    {
        bits++;
    }
    return std::max(bits, 1);
}

std::vector<uint8_t> lzwCompress(const std::vector<uint8_t>& indices, int minCodeSize)
{
    const int clearCode = 1 << minCodeSize;
    const int endCode   = clearCode + 1;

    std::unordered_map<uint32_t, int> dictionary;
    int                               next     = endCode + 1;
    int                               codeSize = minCodeSize + 1;

    BitWriter writer;
    writer.write(static_cast<uint32_t>(clearCode), codeSize);

    int prefix = indices[0];
    for (size_t i = 1; i < indices.size(); i++)
    {
        const uint8_t  ch  = indices[i];
        const uint32_t key = (static_cast<uint32_t>(prefix) << 8) | ch;
        const auto     found = dictionary.find(key);
        if (found != dictionary.end())
        {
            prefix = found->second;
            continue;
        }
        writer.write(static_cast<uint32_t>(prefix), codeSize);
        dictionary[key] = next;
        if (next == (1 << codeSize))
        {
            if (codeSize < 12)
            {
                codeSize++;
            }
            else
            {
                writer.write(static_cast<uint32_t>(clearCode), codeSize);
                dictionary.clear();
                next     = endCode;
                codeSize = minCodeSize + 1;
            }
        }
        next++;
        prefix = ch;
    }
    writer.write(static_cast<uint32_t>(prefix), codeSize);
    writer.write(static_cast<uint32_t>(endCode), codeSize);
    writer.flush();
    return writer.bytes;
}

void putShort(std::vector<uint8_t>& out, int value)
{
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

} // namespace

int Gif::indexOf(uint32_t rgb)
{
    for (size_t i = 1; i < palette_.size(); i++)
    {
        if (palette_[i] == rgb)
        {
            return static_cast<int>(i);
        }
    }
    if (palette_.size() >= MaxColours)
    {
        return -1;
    }
    palette_.push_back(rgb);
    return static_cast<int>(palette_.size() - 1);
}

bool Gif::addFrame(const uint8_t* rgba, int width, int height)
{
    if (frames_.empty())
    {
        width_  = width;
        height_ = height;
        /* Entry 0 is the transparent slot and never matches a real colour. */
        palette_.assign(1, 0u);
    }
    else if (width != width_ || height != height_)
    {
        error_ = "frames differ in size";
        return false;
    }

    std::vector<uint8_t> indices(static_cast<size_t>(width) * height);
    for (size_t p = 0; p < indices.size(); p++)
    {
        const uint8_t* px = rgba + p * 4;
        if (px[3] == 0)
        {
            indices[p] = 0;
            continue;
        }
        const uint32_t rgb = (static_cast<uint32_t>(px[0]) << 16)
                             | (static_cast<uint32_t>(px[1]) << 8) | px[2];
        const int index = indexOf(rgb);
        if (index < 0)
        {
            error_ = "more colours than a table holds";
            return false;
        }
        indices[p] = static_cast<uint8_t>(index);
    }
    frames_.push_back(std::move(indices));
    return true;
}

bool Gif::write(const std::string& path, int delayTicks) const
{
    if (frames_.empty())
    {
        error_ = "no frames";
        return false;
    }

    const int bits    = tableBits(static_cast<int>(palette_.size()));
    const int entries = 1 << bits;
    /* Hundredths of a second is all a GIF delay can say, so 60 Hz ticks round. */
    const int delay = std::max(1, (delayTicks * 100 + 30) / 60);

    std::vector<uint8_t> out;
    const char*          header = "GIF89a";
    out.insert(out.end(), header, header + 6);

    putShort(out, width_);
    putShort(out, height_);
    out.push_back(static_cast<uint8_t>(0x80 | ((bits - 1) << 4) | (bits - 1)));
    out.push_back(0); // background index
    out.push_back(0); // pixel aspect ratio

    for (int i = 0; i < entries; i++)
    {
        const uint32_t rgb = (i < static_cast<int>(palette_.size())) ? palette_[i] : 0u;
        out.push_back(static_cast<uint8_t>((rgb >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((rgb >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>(rgb & 0xFF));
    }

    // Netscape extension: loop forever.
    const uint8_t loop[] = {0x21, 0xFF, 0x0B, 'N', 'E', 'T', 'S', 'C', 'A', 'P',
                            'E', '2',  '.',  '0', 0x03, 0x01, 0x00, 0x00, 0x00};
    out.insert(out.end(), std::begin(loop), std::end(loop));

    const int minCodeSize = std::max(2, bits);
    for (const auto& indices : frames_)
    {
        // Graphic control: restore to background, entry 0 transparent.
        out.push_back(0x21);
        out.push_back(0xF9);
        out.push_back(0x04);
        out.push_back(static_cast<uint8_t>((2 << 2) | 0x01));
        putShort(out, delay);
        out.push_back(0); // transparent colour index
        out.push_back(0);

        out.push_back(0x2C);
        putShort(out, 0);
        putShort(out, 0);
        putShort(out, width_);
        putShort(out, height_);
        out.push_back(0);

        out.push_back(static_cast<uint8_t>(minCodeSize));
        const std::vector<uint8_t> data = lzwCompress(indices, minCodeSize);
        for (size_t at = 0; at < data.size();)
        {
            const size_t run = std::min<size_t>(255, data.size() - at);
            out.push_back(static_cast<uint8_t>(run));
            out.insert(out.end(), data.begin() + at, data.begin() + at + run);
            at += run;
        }
        out.push_back(0);
    }
    out.push_back(0x3B);

    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr)
    {
        error_ = "cannot open " + path;
        return false;
    }
    const bool ok = std::fwrite(out.data(), 1, out.size(), file) == out.size();
    std::fclose(file);
    if (!ok)
    {
        error_ = "short write to " + path;
    }
    return ok;
}

} // namespace bakery
