#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

namespace bakery
{

/* A GPU device with no window on screen and one offscreen target to draw into.
   SDL_GPU still wants a window to claim, so one is created hidden. */
class Renderer
{
  public:
    Renderer(int width, int height);
    ~Renderer();

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    [[nodiscard]] bool ok() const { return device_ != nullptr; }
    [[nodiscard]] int  width() const { return width_; }
    [[nodiscard]] int  height() const { return height_; }

    SDL_GPUDevice*  device() const { return device_; }
    SDL_GPUTexture* target() const { return target_; }

    /* Opens a pass on the offscreen target cleared to transparent, so whatever
       a shader discards stays a cutout. */
    SDL_GPURenderPass* beginPass();
    void               endPass();

    /* Reads the target back into rgba, its top-left w x h corner, tightly
       packed. */
    [[nodiscard]] bool read(int w, int h, std::vector<uint8_t>& rgba);

  private:
    int             width_  = 0;
    int             height_ = 0;
    SDL_Window*     window_ = nullptr;
    SDL_GPUDevice*  device_ = nullptr;
    SDL_GPUTexture* target_ = nullptr;

    SDL_GPUCommandBuffer* commands_ = nullptr;
    SDL_GPURenderPass*    pass_     = nullptr;
};

} // namespace bakery
