# Mega Man Zero/ZX Legacy Collection 'FilterHack' d3d9.dll wrapper mod

Features:
- Adding Support for RetroArch's [common-shaders](https://github.com/libretro/common-shaders) into Capcom's Mega Man Zero/ZX Legacy Collection
- Various graphical improvements and a software interface for effecting the game's rendering pipeline.

Download from [here](https://github.com/isja13/d3d9-mmz-zxlc).

Modified from xzn's MegaMan X Collection d3d10 wrapper. There are many challenges in porting over from that game to this. First off is that a majority of Capcom collections in the series are essentially using emulation, whilst this collection seems to have built the games for Direct X 9 with individual sprite objects and the like. The game window still exists as a texture object in the API space, but there is more to affecting the visuals than that.

//////////////////

This branch was used extensively in debug as it follows the legacy CG shader preset format and provides extensive tweakability over the shader path. Unfortunately, it is also a dead end due to the native API limitations on registers and samplers so it has only proven to run lcd3x visibly, but this build was used to actually hook the full 1280x960 game composite, differentiate GBA vs DS via Viewport Aspect Ratio, and reverse engineered the VPOS overscan (x1.07 y1.20) that the engine uses to accquire the active rect for the GBA Zero titles. There are bugs with the fingerprinting, but ZX is indistinguishable from running on an emulator. Due to scaling artifacts Zero is not as accurate, but is still entirely serviceable and may not even be noticed by the majority of players unless examined under a microscope.

CG fully native//
<img width="2560" height="1440" alt="image" src="https://github.com/user-attachments/assets/3d6887b4-57ab-4dcd-9718-3308c74fb4f7" />
<img width="2560" height="1440" alt="image" src="https://github.com/user-attachments/assets/ea184f4e-d383-4fea-91a3-1a2a44c8cb74" />

left RA right CG
<img width="2560" height="929" alt="image" src="https://github.com/user-attachments/assets/469612b2-91c5-4d60-b730-3b700463d5ab" />

If all you want is to replace the game's type 1 filter with an LCD screen, this is the build for you// Also here for toying with the old SDK.

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

Copy `dinput8.dll`, `filter-hack.ini`, `cg.dll`, `cgd3d9.dll`  and the `common-shaders\` directory to your game folders, e.g.:

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

- `Nvidia CG` is needed only for `common_shader` support.
- `minhook` is used for intercepting calls to `d3d10.dll`.
- `imgui` is used for overlay display.
- `smhasher` is technically optional. Currently used for identifying the built-in Type 1 filter shader.
