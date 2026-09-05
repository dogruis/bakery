# Bakery

A shader lab. Runs Slang shaders headlessly, so an effect can be looked at,
iterated on, and baked into animation frames for hardware that has no shaders
at all.

It exists for two reasons. The first is porting shader effects to the Neo Geo,
which has no shaders and draws everything as sprites - an effect can still ship
there, as frames of art. The second is that sprites are sometimes simply
faster: where an effect would otherwise run per object on screen, drawing a
baked frame can cost less than computing it, on any hardware.

Either way the effect is authored once, as a shader, and the frames are what
that shader drew - not the same maths written a second time in a script.

## What it does

- Compiles a `.slang` file and runs it over a quad, with no window
- Reads the result back and writes it out as PNG frames
- Takes the shader as a path, so it can bake a shader that lives in another
  project without holding a copy of it

## Building

Needs `slangc` on PATH (from the Slang or Vulkan SDK). SDL3 and
SDL_shadercross are fetched at configure time.

```
cmake -B build && cmake --build build
```

## Layout

    src/Renderer.*   headless GPU device, offscreen target, readback
    src/Shader.*     slangc -> SPIR-V -> pipeline
    shaders/         effects that live here rather than in a game
