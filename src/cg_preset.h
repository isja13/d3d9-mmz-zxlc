#pragma once
#include <d3d9.h>

namespace ZeroMod {

	struct d3d9_video_struct; // forward (use your real type include if needed)

	// Load a .cgp preset and compile its .cg pass programs.
	// cgp_path is a filesystem path to the preset file.
	bool cg_d3d9_load_cgp_preset(d3d9_video_struct* d3d9, const char* cgp_path);

	// Free any CG programs/resources currently held by the CGP runtime.
	void cg_d3d9_unload_cgp(d3d9_video_struct* d3d9);

} // namespace ZeroMod

