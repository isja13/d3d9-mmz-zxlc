#pragma once
#include <d3d9.h>
#include "../Cg/cg.h"

bool cg_load_init_once(HMODULE hostModule, IDirect3DDevice9* dev);
bool cg_load_is_ready();
const char* cg_load_last_error();
CGcontext cg_load_context();

void cg_load_on_reset_pre();
void cg_load_on_reset_post(IDirect3DDevice9* dev);
