# Mega Man Zero/ZX Legacy Collection 'FilterHack' d3d9.dll wrapper mod

Features:
- Adding Support for RetroArch's [slang-shaders](https://github.com/libretro/slang-shaders) into Capcom's Mega Man Zero/ZX Legacy Collection
- Various graphical improvements and a software interface for effecting the game's rendering pipeline.

Download from [here](https://github.com/isja13/d3d9-mmz-zxlc).

Modified from xzn's MegaMan X Collection d3d10 wrapper. There are many challenges in porting over from that game to this. First off is that a majority of Capcom collections in the series are essentially using emulation, whilst this collection seems to have built the games for Direct X 9 with individual sprite objects and the like. The game window still exists as a texture object in the API space, but there is more to affecting the visuals than that.

The code here has been entirely ported to Direct X 9 and solved for any immediate game-breaking bugs, however...the shader system in the X collection mod was entirely ported over from the leading open-source emulation frontend RetroArch, which DOES NOT support Direct X 9 natively for it's 'slang' shading preset pipeline.

This project has made major steps in correcting that discrepancy through:
1. A complete Device level d3d9 wrapper (much like the X collection)
2. Modifications through that wrapper to the d3d9 video driver
3. A slang preset parser implementation that uses Retroarch's headers
4. A SPRIV-Cross compiling implementation that builds presets out of HLSL Vertex and Pixel shader outputs
5. A translation layer for relaying semantics and interpreting the shader bytecode in the source application

The result is that a single pass shader can be loaded, compiled, built, and applied to show a video output that is recognizable as the game, with the specific shader applied, but not without graphical glitches. Such conventions as bevels and scanlines appear, but certain phase, chroma, and subpixel effects are currently either not aligned, blown way out of proportion, or completely flipped on the bottom half of the quad. This has to do with D3D9's lack of similar features to future DX versions such as Cbuffers, sampler states, and geometry shaders, so it produces alongside mismatched texels, what can only be described as a "triangle of death."
![20260206112811_1](https://github.com/user-attachments/assets/f3f93bae-649d-47ae-baac-87e21c73ff0b)

Solutions explored to try and remedy this have included, but are not limited to:
1. Patching the HLSL to force linearity across sin() functions
2. Manpulating UV geometry vectors
3. Forcing alignment with half pixel/MVP
4. Drawing the entire quad as a single Trianglelist instead of strip

However, no solution could be found by time of writing. If this could be solve, multipass implementation would not be too far off, but as it stands, this is a hard block in the road as far as Retroarch to DX9 slang shader compatibility goes. It seems to be engine/API level convention limited, and likely cannot be remedied without serious architectural brute forcing.

![zx lcd leg](https://github.com/user-attachments/assets/5e2c49a2-15c3-4ae4-bf09-047a4201dfbe)

![crt zx](https://github.com/user-attachments/assets/2eca6115-3aef-4bb6-a1c3-547f4bfc93ef)

vs retroarch native: 
<img width="2560" height="1440" alt="zx lcd 2" src="https://github.com/user-attachments/assets/c83af612-ee2e-46f3-b2d2-a423105c076b" />
<img width="2560" height="1440" alt="ZX lcd" src="https://github.com/user-attachments/assets/fb1c0b88-79d8-4a6f-817c-7aae43d31fd0" />

<img width="2560" height="1440" alt="crt lottes" src="https://github.com/user-attachments/assets/20384c8f-55ba-4901-adfb-53f82b2b039a" />
<img width="2532" height="1170" alt="DCIM_115APPLE_IMG_5690" src="https://github.com/user-attachments/assets/aeaa12fc-f9b3-4c9f-a01f-088e152dc467" />

Close ups: 
<img width="1297" height="918" alt="crt close" src="https://github.com/user-attachments/assets/b2c77e65-2602-4dfb-a5e4-74b4212556cb" />
<img width="1903" height="921" alt="LCD close" src="https://github.com/user-attachments/assets/f12e0bb6-0652-43d4-95e3-40c5e84468b7" />

//////////////////

What has been successfully implemented is base Linear filtering, the ui overlay, and a partial logging system salvaged out of the pre-existing class via console readout and Debug Viewer alongside game selection state machine logic, and the integrity of .ini configurable settings // 

Constants table, invariants, and example HLSL included in src folder.


## Building from source

Using i686-w64-mingw32-gcc (cross compiling should work too):

```bash
# Download source
git clone https://github.com/isja13/d3d9-mmz-zxlc.git
cd d3d9-mmz-zxlc
git submodule update --init --recursive

# Create symlinks and patch files
make prep

# Build the dll
make -j$(nproc) dll
```

Some options to pass to make

```bash
# disable optimizations and prevents stripping
make o3=0 dll

# disable lto (keep -O3)
make lto=0 dll
```

## Install

Copy `dinput8.dll`, `filter-hack.ini`, and the `slang-shaders\` directory to your game folders, e.g.:

- `SteamLibrary\steamapps\common\MMZZXLC`

## Configuration

`filter-hack.ini` contains options to configure the mod.

```ini
[logging]
; enabled=true
; hotkey_toggle=VK_CONTROL+O
; hotkey_frame=VK_CONTROL+P

[graphics]
interp=false
linear=false

enhanced=true

; slang_shader=slang-shaders/xbrz/xbrz-freescale.slangp

 slang_shader=slang-shaders/handheld/lcd3x.slangp

; slang_shader=slang-shaders/crt/crt-lottes.slangp

; slang_shader_gba=slang-shaders/crt/crt-lottes-fast.slangp
; slang_shader_ds=
```

If all goes well you should now be able to start the game and see the overlay on top-left of the screen showing the status of the mod.

`filter-hack.ini` can be edited and have its options applied while the game is running.

## License

Source code for this mod, without its dependencies, is available under MIT. Dependencies such as `RetroArch` are released under GPL.

- `RetroArch` is needed only for `slang_shader` support.
- `SPIRV-Cross` and `glslang` are used for `slang_shader` support.
- `HLSLcc` is used for debugging.

Other dependencies are more or less required:

- `minhook` is used for intercepting calls to `d3d10.dll`.
- `imgui` is used for overlay display.
- `smhasher` is technically optional. Currently used for identifying the built-in Type 1 filter shader.
