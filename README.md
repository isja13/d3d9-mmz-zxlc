# Mega Man Zero/ZX Legacy Collection 'FilterMod' d3d9.dll wrapper
![20260219171737_1](https://github.com/user-attachments/assets/bb76db53-a81f-4fc9-9fe7-77e758625acc)

Features:
- Adding Native D3D9 Support for RetroArch's [slang-shaders](https://github.com/libretro/slang-shaders) into Capcom's Mega Man Zero/ZX Legacy Collection
- Forced Integer Scaling to correct pixel art stretching artifacts.
- Software Interface for effecting the game's rendering pipeline.
- Various Graphical Improvements
- Controller/Hotkey Togggles

Download from [here](https://github.com/isja13/d3d9-mmz-zxlc).

*Based on xzn's MegaMan X Collection d3d10 wrapper. 

//////////////////

scalefx+lcd3x
![20260221183613_1](https://github.com/user-attachments/assets/982529c6-fc38-44e5-851f-be4312634895)

/////////////////

## Usage
Choose your presets from the slang_shaders folder and they will build from the path you set in the INI. This runs entirely native D3D9 so whether a shader preset works or not will depend on whether the given .slang code can be compiled to HLSL Shader Model 3.0 bytecode within DirectX 9's register and sampler limitations. Multipass is supported, but extreme crt chains like Royale and Mega Bezel will need to be patched for full compatibility due to reliance on features beyond the API's rendering and semantics contract.

Replaces the in-game Type 1 filter, and can be toggled with either ` or Left Analog Stick by default (ini configurable). Presets will depend on preference but the hierarchy is as such//

-Stock : base game image, with corrected pixel scaling
![20260223234041_1](https://github.com/user-attachments/assets/ef5fded0-6937-4b87-be46-9717c7662fbf)

-Smoothing : filters that sample the game at a higher resolution to improve edges.

Xbrz-smooth & animated
![20260214160932_1](https://github.com/user-attachments/assets/755cb529-02ec-40ad-a471-3776cda3bcb7)

Scalefx- just more pixels
![20260216182940_1](https://github.com/user-attachments/assets/3fc6c824-cbd0-4558-9d2b-512e2c76495d)

-ScreenFX : derives overlay math for subpixel lattice/chroma effects to emulate the look of a specific device, like the LCD handheld consoles the games were designed for, or an old CRT TV.

LCD3x-looks most like GBA
![lcd slang](https://github.com/user-attachments/assets/0f3dcff9-678a-463d-b6f9-b8e3ad37c79a)

Crt Lottes- old arcade console
<img width="2560" height="1440" alt="Screenshot 2026-02-15 150419" src="https://github.com/user-attachments/assets/714c85f0-8222-4804-b56d-54e9c502caa1" />

-Customs : combination multipass filters to drastically change the look of the game.

freescale psp - upscaled handheld with backlight saturation
![20260215192057_1](https://github.com/user-attachments/assets/6c9cede1-95ca-466f-abe8-16fd21506d5c)

Xbrz+LCD3x - upscaled then fed through original resolution filter
![20260216003453_1](https://github.com/user-attachments/assets/8d76d183-94ed-429a-864b-32a2f37ff026)

////////////////////////////////////////////////////////////////////

As this is not emulation, there are concessions made on a per game basis that certain toggles have been provided to handle based on player preference. In short the game composites an image like this:

1. Opaque black background
2. Background/Wallpaper Elements
3. Game Layer
4. FMV Cutscenes
5. UI buttons/continue
6. FX flashes

What this mod does to function is intercept the game draw for Game Layer 3 and feed the original source resolution texture for scaling+shader math which encodes opacity, before rendering the full image at screen scale pixels to the full monitor res backbuffer. This is essentially on top of a 'layer 7' and thus, the remaining composite must be overlayed ontop of this. That means the Black background and clear the game uses for blending, as well as background textures like during the text crawls for Zero 3/4 openings will obscure the game layer text if not encoded with transparency. Hence the first toggle.

Zero 4 opening text crawl edge case:
![20260219144905_1](https://github.com/user-attachments/assets/15829528-cdfc-4d6e-ae41-0077ffbf0337)

This allows UI and FX elements to blend seamlessly for the most part, and allows cutscenes to play, albeit with an altered visual style.

Cutscene - Transparent
![20260219171138_1](https://github.com/user-attachments/assets/8ddd01d8-074e-412f-8bc9-13f1534f6113)

Cutscene - Opaque
![20260223203446_1](https://github.com/user-attachments/assets/f2d5389d-3dbb-49ef-9bbf-a7bcbc0eb158)

It is reccomended for gameplay to be always done in Transparent mode to preserve the FX layer however Cutscene style will be a matter of preference. A Flash kill is also implemented for edge cases in which the FX layer misbehaves under certain configurations. The mod is configured by default to favor gameplay, but this is all configurable in the INI. If all else fails, the entire shader chain can be disabled to temporarily let the game draw through for something like the ZX menu, as well as providing a quick A/B test toggle for the custom shader with the game's own default filter.

High Contrast Edge Detection Blending Shader
![20260219171627_1](https://github.com/user-attachments/assets/10e03bcf-5960-4d77-a3e2-dee6385cdf03)

///////////////////////////////////////////////////////////////////////////

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

Copy `dinput8.dll`, `filter-mod.ini`, and the `slang-shaders\` directory to your game folders, e.g.:

- `SteamLibrary\steamapps\common\MMZZXLC`

## Configuration

`filter-mod.ini` contains options to configure the mod.

```ini
[graphics]
                           _
slang_shader=slang-shaders/custom/ScaleFx+LCD.slangp
                           ^   _
;slang_shader_gba=slang-shaders/custom/Xbrz-Free+LCD3x.slangp
 slang_shader_ds=slang-shaders/xbrz/xbrz-freescale.slangp
                               ^
[Favorites]
custom/ScaleFx+LCD.slangp
custom/psp-freescale.slangp
custom/Xbrz-Free+LCD3x.slangp

[Smoothing]
custom/scalefx-d3d9.slangp
xbrz/xbrz-freescale.slangp

[ScreenFx]
crt/crt-lottes.slangp
crt/crt-lottes-fast.slangp
handheld/lcd3x.slangp
handheld/lcd1x_psp.slangp

;linear=false
;interp=false
;enhanced=false
;custom/Stock.slangp

//------------------------

[toggles]
hotkey_shader_toggle=OEM_3
hotkey_flash_kill=1
hotkey_transparent_cutscenes=2

hotkey_shader_toggle_pad=XINPUT_LS
hotkey_flash_kill_pad=XINPUT_RS
hotkey_transparent_cutscenes_pad=XINPUT_RT

[defaults]
;shader_toggle=TRUE
;flash_kill=FALSE
;transparent_cutscenes=TRUE

[logging]
; enabled=true
; hotkey_toggle=VK_CONTROL+O
; hotkey_frame=VK_CONTROL+P

```

If all goes well you should now be able to start the game and see the overlay on top-left of the screen showing the status of the mod.
<img width="476" height="87" alt="Overlay" src="https://github.com/user-attachments/assets/42941e01-84ec-4e26-91a0-26486e22f550" />

`filter-mod.ini` can be edited and have its options applied while the game is running.

## License

Source code for this mod, without its dependencies, is available under MIT. Dependencies such as `RetroArch` are released under GPL.

- `RetroArch` is needed only for `slang_shader` support.
- `SPIRV-Cross` and `glslang` are used for `slang_shader` support.
- `HLSLcc` is used for debugging.

Other dependencies are more or less required: 

- `minhook` is used for intercepting calls to `d3d9.dll`.
- `imgui` is used for overlay display.
- `smhasher` is technically optional. Currently used for identifying the built-in Type 1 filter shader.
