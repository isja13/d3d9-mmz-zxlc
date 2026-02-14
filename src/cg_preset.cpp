#define NOMINMAX
#include <windows.h>
#include "cg_preset.h"
#include "d3d9video.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <algorithm>

#include "../Cg/cg.h"
#include "../Cg/cgd3d9.h"

#define ZM_CGP_PARSE_ONLY 0

// from your loader
CGcontext cg_load_context();
bool cg_load_init_once(HMODULE hostModule, IDirect3DDevice9* dev);
bool cg_load_is_ready();
const char* cg_load_last_error();

namespace ZeroMod {

    // ---- small logging helper
    static void zm_logf(const char* fmt, ...)
    {
        char b[1024];
        va_list va;
        va_start(va, fmt);
        _vsnprintf(b, sizeof(b), fmt, va);
        va_end(va);
        b[sizeof(b) - 1] = 0;
        OutputDebugStringA(b);
    }
    static void dump_cg_failure(CGcontext ctx, const char* where)
    {
        const char* listing = cgGetLastListing(ctx);
        if (listing && listing[0])
            ZeroMod::zm_logf("[ZeroMod][CG][LISTING] %s\n%s\n", where, listing);
        else
            ZeroMod::zm_logf("[ZeroMod][CG][LISTING] %s\n(no listing)\n", where);

        CGerror e = cgGetError(); // consume ONCE, here
        ZeroMod::zm_logf("[ZeroMod][CG][ERR] %s: %s (%d)\n",
            where, cgGetErrorString(e), (int)e);
    }

    // ---- tiny string helpers
    static inline void ltrim(std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    }
    static inline void rtrim(std::string& s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    }
    static inline void trim(std::string& s) { ltrim(s); rtrim(s); }

    static inline void strip_quotes(std::string& s)
    {
        if (s.size() >= 2) {
            if ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')) {
                s = s.substr(1, s.size() - 2);
            }
        }
    }

    static inline std::string to_lower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return (unsigned char)std::tolower(c); });
        return s;
    }

    // ---- path helpers (Windows-friendly, accepts / or \ in inputs)
    static std::string dirname_of(const std::string& path)
    {
        size_t p1 = path.find_last_of('\\');
        size_t p2 = path.find_last_of('/');
        size_t p = (p1 == std::string::npos) ? p2 : (p2 == std::string::npos ? p1 : std::max(p1, p2));
        if (p == std::string::npos) return std::string();
        return path.substr(0, p);
    }

    static bool is_absolute_path(const std::string& p)
    {
        if (p.size() >= 2 && std::isalpha((unsigned char)p[0]) && p[1] == ':')
            return true;
        if (!p.empty() && (p[0] == '\\' || p[0] == '/'))
            return true;
        return false;
    }

    static std::string join_path(const std::string& dir, const std::string& rel)
    {
        if (rel.empty()) return rel;
        if (is_absolute_path(rel)) return rel;
        if (dir.empty()) return rel;

        char sep = '\\';
        if (!dir.empty() && (dir.back() == '\\' || dir.back() == '/'))
            return dir + rel;
        return dir + sep + rel;
    }

    // ---- INI-ish parser: key=value, strips comments (# or ;)
    static bool parse_kv_file(const char* path, std::unordered_map<std::string, std::string>& out_kv)
    {
        std::ifstream f(path, std::ios::in);
        if (!f.is_open())
            return false;

        std::string line;
        while (std::getline(f, line)) {
            // strip CR
            if (!line.empty() && line.back() == '\r') line.pop_back();

            // remove comments (# or ;)
            // NOTE: v0: not quote-aware; acceptable for CGP in practice
            size_t c1 = line.find('#');
            size_t c2 = line.find(';');
            size_t c = std::min(c1 == std::string::npos ? line.size() : c1,
                c2 == std::string::npos ? line.size() : c2);
            if (c != std::string::npos && c < line.size())
                line = line.substr(0, c);

            trim(line);
            if (line.empty()) continue;

            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;

            std::string k = line.substr(0, eq);
            std::string v = line.substr(eq + 1);

            trim(k);
            trim(v);
            strip_quotes(k);
            strip_quotes(v);

            if (k.empty()) continue;

            // normalize keys to lowercase (CGP keys vary)
            out_kv[to_lower(k)] = v;
        }
        return true;
    }

    // ---- typed accessors
    static int kv_get_int(const std::unordered_map<std::string, std::string>& kv, const char* key, int defv)
    {
        auto it = kv.find(to_lower(key));
        if (it == kv.end()) return defv;
        return atoi(it->second.c_str());
    }

    static float kv_get_float(const std::unordered_map<std::string, std::string>& kv, const char* key, float defv)
    {
        auto it = kv.find(to_lower(key));
        if (it == kv.end()) return defv;
        return (float)atof(it->second.c_str());
    }

    static bool kv_get_bool(const std::unordered_map<std::string, std::string>& kv, const char* key, bool defv)
    {
        auto it = kv.find(to_lower(key));
        if (it == kv.end()) return defv;
        std::string v = to_lower(it->second);
        trim(v);
        strip_quotes(v);
        if (v == "1" || v == "true" || v == "yes" || v == "on") return true;
        if (v == "0" || v == "false" || v == "no" || v == "off") return false;
        return defv;
    }

    static std::string kv_get_str(const std::unordered_map<std::string, std::string>& kv, const char* key, const char* defv = "")
    {
        auto it = kv.find(to_lower(key));
        if (it == kv.end()) return std::string(defv ? defv : "");
        return it->second;
    }

    // ---- minimal preset representation (v0)
    struct cgp_pass_desc {
        std::string cg_path;        // resolved absolute/relative final path
        bool filter_linear = false;
        std::string scale_type;     // "source" or "viewport" (not used yet)
        float scale = 1.0f;         // not used yet
    };

    struct cgp_preset_desc {
        std::string cgp_path;
        std::string preset_dir;
        int shaders = 0;
        std::vector<cgp_pass_desc> passes;

        // parameters: parsed but not applied in v0
        std::vector<std::string> param_names;
        std::unordered_map<std::string, float> param_values;
    };

    static void split_semicolon_list(const std::string& s, std::vector<std::string>& out)
    {
        out.clear();
        std::string cur;
        for (char ch : s) {
            if (ch == ';') {
                trim(cur);
                strip_quotes(cur);
                if (!cur.empty()) out.push_back(cur);
                cur.clear();
            }
            else {
                cur.push_back(ch);
            }
        }
        trim(cur);
        strip_quotes(cur);
        if (!cur.empty()) out.push_back(cur);
    }

    static bool load_cgp_desc(const char* cgp_path, cgp_preset_desc& out)
    {
        out = {};
        out.cgp_path = cgp_path ? cgp_path : "";
        out.preset_dir = dirname_of(out.cgp_path);

        std::unordered_map<std::string, std::string> kv;
        if (!parse_kv_file(cgp_path, kv))
            return false;

        out.shaders = kv_get_int(kv, "shaders", 0);
        if (out.shaders <= 0) {
            // infer: scan shaderN keys
            int max_i = -1;
            for (const auto& p : kv) {
                if (p.first.rfind("shader", 0) == 0) {
                    const char* s = p.first.c_str() + 6;
                    if (*s) {
                        int idx = atoi(s);
                        if (idx > max_i) max_i = idx;
                    }
                }
            }
            out.shaders = max_i + 1;
        }

        if (out.shaders <= 0)
            return false;

        out.passes.resize((size_t)out.shaders);

        for (int i = 0; i < out.shaders; i++) {
            char kshader[64]; _snprintf(kshader, sizeof(kshader), "shader%d", i);
            char kfilter[64]; _snprintf(kfilter, sizeof(kfilter), "filter_linear%d", i);
            char kstype[64];  _snprintf(kstype, sizeof(kstype), "scale_type%d", i);
            char kscale[64];  _snprintf(kscale, sizeof(kscale), "scale%d", i);

            std::string rel = kv_get_str(kv, kshader, "");
            if (rel.empty()) {
                zm_logf("[ZeroMod] CGP missing key '%s'\n", kshader);
                return false;
            }

            out.passes[i].cg_path = join_path(out.preset_dir, rel);
            out.passes[i].filter_linear = kv_get_bool(kv, kfilter, false);
            out.passes[i].scale_type = to_lower(kv_get_str(kv, kstype, "source"));
            out.passes[i].scale = kv_get_float(kv, kscale, 1.0f);
        }

        // parameters = "a;b;c"
        {
            std::string params = kv_get_str(kv, "parameters", "");
            if (!params.empty()) {
                split_semicolon_list(params, out.param_names);
                for (auto& name : out.param_names) {
                    // CGP often stores numeric values as strings, with quotes sometimes
                    std::string key = to_lower(name);
                    auto it = kv.find(key);
                    if (it != kv.end()) {
                        out.param_values[key] = (float)atof(it->second.c_str());
                    }
                }
            }
        }

        return true;
    }

    static void parse_pragma_parameters(
        const char* cg_path,
        std::vector<std::pair<std::string, float>>& out_defaults)
    {
        out_defaults.clear();
        std::ifstream f(cg_path, std::ios::in);
        if (!f.is_open()) return;

        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.find("#pragma") == std::string::npos) continue;
            if (line.find("parameter") == std::string::npos) continue;

            std::istringstream iss(line);
            std::string tok1, tok2, name;
            iss >> tok1 >> tok2;
            if (tok1 != "#pragma" || tok2 != "parameter") continue;

            iss >> name;
            if (name.empty()) continue;

            // eat quoted desc
            std::string desc;
            iss >> desc;
            if (!desc.empty() && desc[0] == '"') {
                while (desc.size() == 1 || desc.back() != '"') {
                    std::string more;
                    if (!(iss >> more)) break;
                    desc += " " + more;
                }
            }

            std::string defTok;
            iss >> defTok;
            if (defTok.empty()) continue;

            float defv = (float)atof(defTok.c_str());
            out_defaults.push_back({ name, defv }); // KEEP ORIGINAL CASE
        }
    }


    void cg_d3d9_unload_cgp(d3d9_video_struct* d3d9)
    {
        if (!d3d9) return;

        if (d3d9->cg.passes) {
            for (int i = 0; i < d3d9->cg.num_passes; i++) {
                cg_pass_rt& p = d3d9->cg.passes[i];

                if (p.rt_surf) { p.rt_surf->Release(); p.rt_surf = nullptr; }
                if (p.rt_tex) { p.rt_tex->Release();  p.rt_tex = nullptr; }

                if (p.vprog) { cgDestroyProgram(p.vprog); p.vprog = nullptr; }
                if (p.pprog) { cgDestroyProgram(p.pprog); p.pprog = nullptr; }
            }
            free(d3d9->cg.passes);
            d3d9->cg.passes = nullptr;
        }

        if (d3d9->cg.preset_dir) {
            free(d3d9->cg.preset_dir);
            d3d9->cg.preset_dir = nullptr;
        }

        d3d9->cg.num_passes = 0;
        d3d9->cg.active = false;
    }

    static CGprogram try_create_program_from_file(
        CGcontext ctx,
        const char* path,
        CGprofile profile,
        const char* entry)
    {
        const char* args[] = {
            "-DPARAMETER_UNIFORM",
              "-O3",
              "-fastmath",
             // optional but sometimes helps:
             // "-fastprecision",
            nullptr
        };

        zm_logf("[ZeroMod][CG] compiling %s entry=%s profile=%s\n",
            path, entry, cgGetProfileString(profile));

        CGprogram prog = cgCreateProgramFromFile(
            ctx, CG_SOURCE, path, profile, entry, args);

        if (!prog) {
            dump_cg_failure(ctx, "cgCreateProgramFromFile");
            return nullptr;
        }

        // IMPORTANT: check error only once (and if error, dump listing)
        if (cgGetError() != CG_NO_ERROR) {
            dump_cg_failure(ctx, "cgCreateProgramFromFile(post)");
            return nullptr;
        }

        return prog;
    }

    static bool compile_pass_programs(
        const cgp_pass_desc& pass_desc,
        cg_pass_rt& out_pass)
    {
        CGcontext ctx = cg_load_context();
        if (!ctx) return false;

        CGprofile vp = CG_PROFILE_VS_3_0;
        CGprofile fp = CG_PROFILE_PS_3_0;
       // cgD3D9SetOptimalOptions(vp);
       // cgD3D9SetOptimalOptions(fp);
        zm_logf("[ZeroMod][CG] vp=%s fp=%s\n",
            cgGetProfileString(vp), cgGetProfileString(fp));
        const char* vs_entries[] = { "main_vertex", "main", "vs_main", nullptr };
        const char* ps_entries[] = { "main_fragment", "main", "ps_main", nullptr };

        out_pass.vprog = nullptr;
        out_pass.pprog = nullptr;

        for (int i = 0; vs_entries[i]; i++) {
            out_pass.vprog = try_create_program_from_file(ctx, pass_desc.cg_path.c_str(), vp, vs_entries[i]);
            if (out_pass.vprog) break;
        }
        if (!out_pass.vprog) {
            zm_logf("[ZeroMod] CG: failed VS '%s' (%s)\n",
                pass_desc.cg_path.c_str(), cgGetErrorString(cgGetError()));
            return false;
        }

        for (int i = 0; ps_entries[i]; i++) {
            out_pass.pprog = try_create_program_from_file(ctx, pass_desc.cg_path.c_str(), fp, ps_entries[i]);
            if (out_pass.pprog) break;
        }
        if (!out_pass.pprog) {
            zm_logf("[ZeroMod] CG: failed PS '%s' (%s)\n",
                pass_desc.cg_path.c_str(), cgGetErrorString(cgGetError()));
            cgDestroyProgram(out_pass.vprog);
            out_pass.vprog = nullptr;
            return false;
        }

        // Load to D3D9 (IMPORTANT)
        cgD3D9LoadProgram(out_pass.vprog, 0, 0);
        if (cgGetError() != CG_NO_ERROR) {
            zm_logf("[ZeroMod] CG: Load VS failed: %s\n", cgGetErrorString(cgGetError()));
            cgDestroyProgram(out_pass.vprog);
            cgDestroyProgram(out_pass.pprog);
            out_pass.vprog = out_pass.pprog = nullptr;
            return false;
        }

        cgD3D9LoadProgram(out_pass.pprog, 0, 0);
        if (cgGetError() != CG_NO_ERROR) {
            zm_logf("[ZeroMod] CG: Load PS failed: %s\n");
          //  dump_cg_error("cgD3D9LoadProgram(PS)", ctx);
            cgDestroyProgram(out_pass.vprog);
            cgDestroyProgram(out_pass.pprog);
            out_pass.vprog = out_pass.pprog = nullptr;
            return false;
        }
        // Cache params (per-pass!)
        // MVP is almost always VS, but you can also probe PS if you want.
        out_pass.p_mvp = cgGetNamedParameter(out_pass.vprog, "modelViewProj");

        // IN.* are commonly in the *fragment* program in many cg presets,
        // so probe VS first, then fallback to PS.
        out_pass.p_in_video = cgGetNamedParameter(out_pass.vprog, "IN.video_size");
        if (!out_pass.p_in_video)
            out_pass.p_in_video = cgGetNamedParameter(out_pass.pprog, "IN.video_size");

        out_pass.p_in_tex = cgGetNamedParameter(out_pass.vprog, "IN.texture_size");
        if (!out_pass.p_in_tex)
            out_pass.p_in_tex = cgGetNamedParameter(out_pass.pprog, "IN.texture_size");

        out_pass.p_out = cgGetNamedParameter(out_pass.vprog, "IN.output_size");
        if (!out_pass.p_out)
            out_pass.p_out = cgGetNamedParameter(out_pass.pprog, "IN.output_size");

        // Framecount: probe a few common names, PS first.
        out_pass.p_framecount = cgGetNamedParameter(out_pass.pprog, "FrameCount");
        if (!out_pass.p_framecount) out_pass.p_framecount = cgGetNamedParameter(out_pass.pprog, "frame_count");
        if (!out_pass.p_framecount) out_pass.p_framecount = cgGetNamedParameter(out_pass.pprog, "IN.frame_count");
        if (!out_pass.p_framecount) out_pass.p_framecount = cgGetNamedParameter(out_pass.vprog, "FrameCount");
        if (!out_pass.p_framecount) out_pass.p_framecount = cgGetNamedParameter(out_pass.vprog, "frame_count");
        if (!out_pass.p_framecount) out_pass.p_framecount = cgGetNamedParameter(out_pass.vprog, "IN.frame_count");

        // Sampler is PS in almost all cases.
        out_pass.p_decal = cgGetNamedParameter(out_pass.pprog, "decal");
        zm_logf("[ZeroMod][CG] pass params: in_video=%p in_tex=%p out=%p decal=%p\n",
            (void*)out_pass.p_in_video,
            (void*)out_pass.p_in_tex,
            (void*)out_pass.p_out,
            (void*)out_pass.p_decal);

        return true;
    }

    bool cg_d3d9_load_cgp_preset(d3d9_video_struct* d3d9, const char* cgp_path)
    {
        if (!d3d9 || !cgp_path || !*cgp_path || !d3d9->dev)
            return false;

        HMODULE hostMod = GetModuleHandleW(nullptr);
        if (!cg_load_init_once(hostMod, d3d9->dev) || !cg_load_is_ready()) {
            zm_logf("[ZeroMod] CGP load: cg_load_init_once failed: %s\n", cg_load_last_error());
            return false;
        }

        cgp_preset_desc desc;
        if (!load_cgp_desc(cgp_path, desc)) {
            zm_logf("[ZeroMod] CGP load: parse failed for '%s'\n", cgp_path);
            return false;
        }

        // Destroy previous chain first
        cg_d3d9_unload_cgp(d3d9);

        const int N = desc.shaders;
        if (N <= 0) return false;

        d3d9->cg.passes = (cg_pass_rt*)calloc((size_t)N, sizeof(cg_pass_rt));
        if (!d3d9->cg.passes) return false;

        d3d9->cg.num_passes = N;
        d3d9->cg.active = false; // flip true only on success
        d3d9->cg.preset_dir = _strdup(desc.preset_dir.c_str());

        for (int i = 0; i < N; i++)
        {
            cg_pass_rt& p = d3d9->cg.passes[i];

            // Copy parsed config into runtime pass
            p.filter_linear = desc.passes[i].filter_linear;
            p.scale = desc.passes[i].scale;

            // scale_type string -> enum
            std::string st = to_lower(desc.passes[i].scale_type);
            p.scale_type = (st == "viewport") ? cg_pass_rt::SCALE_VIEWPORT : cg_pass_rt::SCALE_SOURCE;

            // Compile programs + load them
            if (!compile_pass_programs(desc.passes[i], p)) {
                zm_logf("[ZeroMod] CGP load: compile failed at pass %d\n", i);
                cg_d3d9_unload_cgp(d3d9);
                return false;
            }
            // --- Resolve #pragma parameter defaults from .cg file
            std::vector<std::pair<std::string, float>> defaults;
            parse_pragma_parameters(desc.passes[i].cg_path.c_str(), defaults);

            p.params.clear();
            p.params.reserve(defaults.size());

            for (auto& kv : defaults)
            {
                cg_pass_rt::cg_param_rt pr;
                pr.cg_name = kv.first;            // ORIGINAL CASE: "hardScan"
                pr.key_lower = to_lower(kv.first);  // for overrides: "hardscan"
                pr.value = kv.second;
                p.params.push_back(pr);
            }

            // apply .cgp overrides (these are global to preset)
            for (auto& pr : p.params)
            {
                auto it = desc.param_values.find(pr.key_lower);
                if (it != desc.param_values.end())
                    pr.value = it->second;
            }

            // resolve CGparameter handles
            for (auto& pr : p.params)
            {
                // try PS first
                pr.h = cgGetNamedParameter(p.pprog, pr.cg_name.c_str());
                if (!pr.h)
                    pr.h = cgGetNamedParameter(p.vprog, pr.cg_name.c_str());
            }

            zm_logf("[ZeroMod][CG] pass%d parameters parsed=%d\n",
                i, (int)p.params.size());
            int bound = 0;
            for (auto& pr : p.params)
                if (pr.h) bound++;

            zm_logf("[ZeroMod][CG] pass%d params total=%d bound=%d\n",
                i, (int)p.params.size(), bound);

            zm_logf("[ZeroMod] CGP load: pass%d OK (v=%p p=%p)\n", i, (void*)p.vprog, (void*)p.pprog);
            zm_logf("[ZeroMod][CG] pass%d params: decal=%p mvp=%p in_video=%p in_tex=%p out=%p framecount=%p\n",
                i,
                (void*)p.p_decal,
                (void*)p.p_mvp,
                (void*)p.p_in_video,
                (void*)p.p_in_tex,
                (void*)p.p_out,
                (void*)p.p_framecount);

        }

        d3d9->cg.active = true;
        zm_logf("[ZeroMod] CGP load: OK (%d passes)\n", N);
        return true;
    }


} // namespace ZeroMod
