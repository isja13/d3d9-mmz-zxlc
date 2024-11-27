# Define the cross prefix based on the operating system
ifeq ($(OS),Windows_NT)
    cross_prefix := x86_64-w64-mingw32-
else
    cross_prefix := x86_64-w64-mingw32-
endif
	
# Compiler settings
cc := $(cross_prefix)gcc
cxx := $(cross_prefix)g++
	
	
# Source and object file definitions
smhasher_src := smhasher/MurmurHash3.cpp
smhasher_obj := $(smhasher_src:smhasher/%.cpp=obj/smhasher/%.o)
smhasher_dir := $(sort $(dir $(smhasher_obj)))
	
HLSLcc_src := $(wildcard HLSLcc/src/*.cpp)
HLSLcc_obj := $(HLSLcc_src:HLSLcc/src/%.cpp=obj/HLSLcc/%.o)
HLSLcc_dir := $(sort $(dir $(HLSLcc_obj)))
	
cbstring_src := $(wildcard HLSLcc/src/cbstring/*.c)
cbstring_obj := $(cbstring_src:HLSLcc/src/cbstring/%.c=obj/cbstring/%.o)
cbstring_dir := $(sort $(dir $(cbstring_obj)))
	
imgui_src := $(wildcard imgui/*.cpp imgui/examples/imgui_impl_dx9.cpp imgui/examples/imgui_impl_win32.cpp)
imgui_obj := $(imgui_src:imgui/%.cpp=obj/imgui/%.o)
imgui_dir := $(sort $(dir $(imgui_obj)))
	
minhook_src := $(wildcard minhook/src/*.c minhook/src/hde/*.c)
minhook_obj := $(minhook_src:minhook/src/%.c=obj/minhook/%.o)
minhook_dir := $(sort $(dir $(minhook_obj)))
	
spirv_src := $(wildcard SPIRV-Cross/spirv_*.cpp)
spirv_obj := $(spirv_src:SPIRV-Cross/%.cpp=obj/SPIRV-Cross/%.o)
spirv_dir := $(sort $(dir $(spirv_obj)))
	
glslang_src := $(wildcard glslang/glslang/glslang/GenericCodeGen/*.cpp) \
               $(wildcard glslang/glslang/glslang/MachineIndependent/*.cpp) \
               $(wildcard glslang/glslang/glslang/MachineIndependent/preprocessor/*.cpp) \
               glslang/glslang/glslang/OSDependent/Windows/ossource.cpp \
               $(wildcard glslang/glslang/hlsl/*.cpp) \
               $(wildcard glslang/glslang/OGLCompilersDLL/*.cpp) \
               $(wildcard glslang/glslang/SPIRV/*.cpp)
glslang_obj := $(glslang_src:glslang/glslang/%.cpp=obj/glslang/%.o)
glslang_dir := $(sort $(dir $(glslang_obj)))
		
# Define the necessary flags, excluding DirectX 10 (D3D10) support
retroarch_def := GIT_VERSION= HAVE_LIBRETRODB=0 OS=Win32 HAVE_SLANG=1 HAVE_BUILTINZLIB=1 HAVE_RTGA=1 HAVE_RPNG=1 HAVE_RJPEG=1 HAVE_RBMP=1
	
# Include additional relevant flags
retroarch_def += HAVE_LANGEXTRA=1 HAVE_GLSLANG=1 HAVE_VIDEO_LAYOUT=1
	
# Use the updated retroarch_def for defining retroarch_fun
retroarch_fun = $(shell make -sf RetroArch/RetroArch/Makefile.common $$'--eval=_print-var:\n\t@echo $$($1)' _print-var $(retroarch_def))
	
# Collect objects for RetroArch
retroarch_obj := $(call retroarch_fun,OBJ) deps/glslang/glslang.o
retroarch_obj := $(addprefix obj/RetroArch/,$(retroarch_obj))
	
# Define the directories based on the object files
retroarch_dir := $(sort $(dir $(retroarch_obj)))
	
retroarch_flg := -DRARCH_INTERNAL -DHAVE_MAIN $(call retroarch_fun,DEFINES) -DENABLE_HLSL -DHAVE_GLSLANG -DHAVE_SPIRV_CROSS -IRetroArch/RetroArch/libretro-common/include -IRetroArch/RetroArch/libretro-common/include/compat/zlib -I. -Iglslang/glslang/glslang/Public -Iglslang/glslang/glslang/MachineIndependent -Iglslang/glslang -Iglslang -ISPIRV-Cross -IRetroArch/RetroArch -IRetroArch/RetroArch/deps -IRetroArch/RetroArch/gfx/common
	
# Define the compilation commands
retroarch_cc = $(cc) $(color_opt) -c -MMD -MP -o $@ $< $(dbg_opt) $(lto_opt) $(retroarch_flg) -Werror=implicit-function-declaration
retroarch_cxx = $(cxx) $(color_opt) -c -MMD -MP -o $@ $< -std=c++17 $(dbg_opt) $(lto_opt) -D__STDC_CONSTANT_MACROS $(retroarch_flg)
	
# Exclude the empty FormatConversion.cpp and include all relevant source files
src := $(wildcard src/*.cpp)
obj := $(src:src/%.cpp=obj/%.o)
dir := $(sort $(dir $(obj)))
	
# Adjusted `cxx_all` to align with source, keeping necessary includes for DX9
cxx_all = $(cxx) $(color_opt) -c -MMD -MP -o $@ $< -std=c++17 $(o3_opt) $(lto_opt) -Iglslang/glslang
	
# Consistent use of `dir` and `dir_all`
obj_all := $(obj) $(smhasher_obj) $(HLSLcc_obj) $(cbstring_obj) $(imgui_obj) $(minhook_obj) $(spirv_obj) $(glslang_obj) $(retroarch_obj)
dir_all := $(sort $(dir $(obj_all)))
dep_all := $(obj_all:%.o=%.d)
	
	
dll := dinput8.dll
	
ifeq ($(color),1)
	color_opt := -fdiagnostics-color=always
else
	color_opt :=
endif
	
ifeq ($(dbg),1)
	dbg_opt := -Og -g
	lto := 0
	dll_dbg := $(dll:%.dll=%.dbg)
else
	dbg_opt := -O2 -DNDEBUG -s
endif
	
ifeq ($(lto),1)
	lto_opt := -flto
else
	lto_opt :=
endif
	
glslang_ln := glslang/glslang.cpp glslang/glslang.hpp
retroarch_ln := RetroArch/gfx/common/d3d9_common.c RetroArch/gfx/common/d3dcompiler_common.c
retroarch_hdr := RetroArch/gfx/common/d3d9_common.h RetroArch/gfx/common/d3dcompiler_common.h
retroarch_hdr_src := $(retroarch_hdr:RetroArch/%.h=RetroArch/%.h)
retroarch_hdr_sen := obj/RetroArch/.retroarch_hdr_sen
		
prep_src := $(glslang_ln) $(retroarch_ln) $(retroarch_hdr) $(retroarch_hdr_sen)
prep: $(prep_src)
dll: $(dll) $(dll_dbg)
	
$(dll_dbg): $(dll)
	$(cross_prefix)objcopy --only-keep-debug $< $@

$(dll): $(obj_all) dinput8.def
	$(cxx) $(color_opt) -o $@ $(filter-out obj/RetroArch/gfx/drivers_shader/glslang.o, $^) $(o3_opt) $(lto_opt) -shared -static -Werror -Wno-odr -Wno-lto-type-mismatch -Wl,--enable-stdcall-fixup -ld3dcompiler_47 -ld3dx9 -lxinput -luuid -lmsimg32 -lhid -lsetupapi -lgdi32 -lcomdlg32 -ldinput8 -lole32 -ldxguid

$(glslang_ln): glslang/%: RetroArch/RetroArch/deps/glslang/%
	ln -sr $< $@
	
$(retroarch_ln): RetroArch/%: RetroArch/RetroArch/%
	ln -sr $< $@

$(retroarch_hdr): $(retroarch_hdr_sen)
	
$(retroarch_hdr_sen): $(retroarch_hdr_src) $(retroarch_dir)
	for hdr in $(retroarch_hdr_src); do \
		dest=$(<D)/$$(basename $$hdr); \
		if [ "$$hdr" != "$$dest" ]; then \
			cp $$hdr $$dest; \
		fi; \
	done
	touch $@

# Compile source files in src/
obj/%.o: src/%.cpp | $(dir)
	$(cxx_all) -Werror -Wall $(retroarch_flg) -IRetroArch/RetroArch/gfx/common
	
obj/smhasher/%.o: smhasher/%.cpp | $(smhasher_dir)
	$(cxx_all)

obj/HLSLcc/%.o: HLSLcc/src/%.cpp | $(HLSLcc_dir)
	$(cxx_all) -IHLSLcc -IHLSLcc/include -IHLSLcc/src/internal_includes -IHLSLcc/src/cbstring -IHLSLcc/src -Wno-deprecated-declarations

obj/cbstring/%.o: HLSLcc/src/cbstring/%.c | $(cbstring_dir)
	$(cc) $(color_opt) -c -MMD -MP -o $@ $< $(dbg_opt) $(lto_opt) -IHLSLcc/src/cbstring

# Compile the correct imgui sources
obj/imgui/%.o: imgui/%.cpp | $(imgui_dir)
	$(cxx_all) -Iimgui -Iimgui/backends -Iimgui/misc
	
obj/minhook/%.o: minhook/src/%.c | $(minhook_dir)
	$(cc) $(color_opt) -c -MMD -MP -o $@ $< -std=c11 -masm=intel $(dbg_opt)

obj/SPIRV-Cross/%.o: SPIRV-Cross/%.cpp | $(spirv_dir)
	$(cxx_all)

obj/glslang/%.o: glslang/glslang/%.cpp | $(glslang_dir)
	$(cxx_all) -DENABLE_HLSL

obj/RetroArch/gfx/common/d3d9_common.o: RetroArch/gfx/common/d3d9_common.c | $(retroarch_dir)
	$(retroarch_cc) -IRetroArch/RetroArch/gfx/common

obj/RetroArch/%.o: RetroArch/%.c | $(retroarch_dir)
	$(retroarch_cc) -IRetroArch/RetroArch/gfx/common -IRetroArch/RetroArch/gfx/drivers -DHAVE_DYNAMIC

obj/RetroArch/%.o: RetroArch/RetroArch/%.c | $(retroarch_dir)
	$(retroarch_cc) -DHAVE_DYNAMIC
	
obj/RetroArch/%.o: RetroArch/RetroArch/%.cpp | $(retroarch_dir)
	$(retroarch_cxx)

obj/RetroArch/deps/glslang/glslang.o: glslang/glslang.cpp | $(retroarch_dir)
	$(retroarch_cxx) -IRetroArch/RetroArch/deps/glslang
	
obj/RetroArch/%.o: RetroArch/RetroArch/%.rc | $(retroarch_dir)
	/c/msys64/mingw64/bin/windres.exe -o $@ $<
			
$(dir_all):
	@mkdir -p $@
	
.PHONY: prep dll clean retroarch_hdr
	
clean:
	-$(RM) *.dll *.dbg
	-find obj/ -type f -name '*.o' -delete
	-find obj/ -type f -name '*.d' -delete
	-find obj/ -type l -delete  # Deletes symlinks in the obj/ directory
	-find obj/ -type d -empty -delete

-include $(dep_all)
	