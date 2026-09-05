#include "Renderer.hpp"

namespace bakery
{

Renderer::Renderer(int width, int height) : width_(width), height_(height)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return;
    }
    window_ = SDL_CreateWindow("bakery", width, height, SDL_WINDOW_HIDDEN);
    if (window_ == nullptr)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return;
    }
    device_ = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, true,
                                  nullptr);
    if (device_ == nullptr || !SDL_ClaimWindowForGPUDevice(device_, window_))
    {
        SDL_Log("GPU device failed: %s", SDL_GetError());
        device_ = nullptr;
        return;
    }

    const SDL_GPUTextureCreateInfo info = {
        .type                 = SDL_GPU_TEXTURETYPE_2D,
        .format               = SDL_GetGPUSwapchainTextureFormat(device_, window_),
        .usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
        .width                = static_cast<Uint32>(width),
        .height               = static_cast<Uint32>(height),
        .layer_count_or_depth = 1,
        .num_levels           = 1,
    };
    target_ = SDL_CreateGPUTexture(device_, &info);
    if (target_ == nullptr)
    {
        SDL_Log("offscreen target failed: %s", SDL_GetError());
        device_ = nullptr;
    }
}

Renderer::~Renderer()
{
    if (device_ != nullptr)
    {
        if (target_ != nullptr)
        {
            SDL_ReleaseGPUTexture(device_, target_);
        }
        SDL_ReleaseWindowFromGPUDevice(device_, window_);
        SDL_DestroyGPUDevice(device_);
    }
    if (window_ != nullptr)
    {
        SDL_DestroyWindow(window_);
    }
    SDL_Quit();
}

SDL_GPURenderPass* Renderer::beginPass()
{
    commands_ = SDL_AcquireGPUCommandBuffer(device_);
    if (commands_ == nullptr)
    {
        return nullptr;
    }
    const SDL_GPUColorTargetInfo info = {
        .texture     = target_,
        .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
        .load_op     = SDL_GPU_LOADOP_CLEAR,
        .store_op    = SDL_GPU_STOREOP_STORE,
    };
    pass_ = SDL_BeginGPURenderPass(commands_, &info, 1, nullptr);
    return pass_;
}

void Renderer::endPass()
{
    SDL_EndGPURenderPass(pass_);
    SDL_SubmitGPUCommandBuffer(commands_);
    pass_     = nullptr;
    commands_ = nullptr;
}

bool Renderer::read(int w, int h, std::vector<uint8_t>& rgba)
{
    const Uint32 pitch = static_cast<Uint32>(width_) * 4u;

    const SDL_GPUTransferBufferCreateInfo info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
        .size  = pitch * static_cast<Uint32>(height_),
    };
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device_, &info);
    if (transfer == nullptr)
    {
        SDL_Log("transfer buffer failed: %s", SDL_GetError());
        return false;
    }

    SDL_GPUCommandBuffer*      cmd  = SDL_AcquireGPUCommandBuffer(device_);
    SDL_GPUCopyPass*           copy = SDL_BeginGPUCopyPass(cmd);
    const SDL_GPUTextureRegion src  = {
         .texture = target_,
         .w       = static_cast<Uint32>(width_),
         .h       = static_cast<Uint32>(height_),
         .d       = 1,
    };
    const SDL_GPUTextureTransferInfo dst = {.transfer_buffer = transfer, .offset = 0};
    SDL_DownloadFromGPUTexture(copy, &src, &dst);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    SDL_WaitForGPUFences(device_, true, &fence, 1);
    SDL_ReleaseGPUFence(device_, fence);

    void* pixels = SDL_MapGPUTransferBuffer(device_, transfer, false);
    if (pixels == nullptr)
    {
        SDL_Log("map failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device_, transfer);
        return false;
    }

    /* The download carries the target's own channel order, which follows the
       swapchain; reading it as the wrong one swaps red and blue. */
    const SDL_GPUTextureFormat format = SDL_GetGPUSwapchainTextureFormat(device_, window_);
    const SDL_PixelFormat      layout =
        (format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM
         || format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB)
                 ? SDL_PIXELFORMAT_BGRA32
                 : SDL_PIXELFORMAT_RGBA32;

    const bool swapped = layout == SDL_PIXELFORMAT_BGRA32;
    rgba.resize(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; y++)
    {
        const uint8_t* row = static_cast<const uint8_t*>(pixels) + static_cast<size_t>(y) * pitch;
        for (int x = 0; x < w; x++)
        {
            uint8_t*       dst = rgba.data() + (static_cast<size_t>(y) * w + x) * 4;
            const uint8_t* src = row + static_cast<size_t>(x) * 4;
            dst[0]             = swapped ? src[2] : src[0];
            dst[1]             = src[1];
            dst[2]             = swapped ? src[0] : src[2];
            dst[3]             = src[3];
        }
    }
    SDL_UnmapGPUTransferBuffer(device_, transfer);
    SDL_ReleaseGPUTransferBuffer(device_, transfer);
    return true;
}

} // namespace bakery
