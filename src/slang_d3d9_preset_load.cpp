#include "slang_d3d9_preset_load.h"

#include "../RetroArch/RetroArch/libretro-common/include/file/config_file.h"

#include "../retroarch/retroarch/gfx/video_shader_parse.h"

#include <string.h> // memset
#include <stdio.h>
#include <stdlib.h> 

namespace ZeroMod {

    static const char* zm_wrap_to_str(enum gfx_wrap_type w)
    {
        switch (w) {
        case RARCH_WRAP_BORDER:          return "BORDER";
        case RARCH_WRAP_EDGE:            return "EDGE";
        case RARCH_WRAP_REPEAT:          return "REPEAT";
        case RARCH_WRAP_MIRRORED_REPEAT: return "MIRRORED";
        default:                         return "???";
        }
    }

    static const char* zm_filter_to_str(unsigned f)
    {
        switch (f) {
        case RARCH_FILTER_UNSPEC:  return "UNSPEC";
        case RARCH_FILTER_LINEAR:  return "LINEAR";
        case RARCH_FILTER_NEAREST: return "NEAREST";
        default:                   return "???";
        }
    }

    static const char* zm_scale_to_str(enum gfx_scale_type t)
    {
        switch (t) {
        case RARCH_SCALE_INPUT:    return "INPUT";
        case RARCH_SCALE_ABSOLUTE: return "ABS";
        case RARCH_SCALE_VIEWPORT: return "VIEWPORT";
        default:                   return "???";
        }
    }

    static void zm_log_shader_summary(const char* tag, const struct video_shader* s)
    {
        if (!s) return;

        char b[1024];

        _snprintf(b, sizeof(b),
            "[ZeroMod] %s: prefix='%s' path='%s' modern=%d modified=%d passes=%u luts=%u params=%u vars=%u feedback_pass=%d history=%d\n",
            tag,
            s->prefix,
            s->path,
            (int)s->modern,
            (int)s->modified,
            (unsigned)s->passes,
            (unsigned)s->luts,
            (unsigned)s->num_parameters,
            (unsigned)s->variables,
            (int)s->feedback_pass,
            (int)s->history_size);
        OutputDebugStringA(b);

        unsigned passes = s->passes;
        if (passes > GFX_MAX_SHADERS) passes = GFX_MAX_SHADERS;

        for (unsigned i = 0; i < passes; i++)
        {
            const video_shader_pass& p = s->pass[i];

            const bool has_path = p.source.path[0] != 0;
            const bool has_vstr = (p.source.string.vertex != NULL) && p.source.string.vertex[0];
            const bool has_fstr = (p.source.string.fragment != NULL) && p.source.string.fragment[0];

            _snprintf(b, sizeof(b),
                "[ZeroMod]  pass[%u]: alias='%s' src=%s%s%s fbo=(%s,%s sx=%.3f sy=%.3f abs=%ux%u fp=%d srgb=%d) wrap=%s filter=%s mip=%d frame_mod=%u feedback=%d\n",
                i,
                p.alias,
                has_path ? "path" : "",
                (has_path && (has_vstr || has_fstr)) ? "+" : "",
                (has_vstr || has_fstr) ? "string" : "",
                zm_scale_to_str(p.fbo.type_x),
                zm_scale_to_str(p.fbo.type_y),
                p.fbo.scale_x, p.fbo.scale_y,
                (unsigned)p.fbo.abs_x, (unsigned)p.fbo.abs_y,
                (int)p.fbo.fp_fbo, (int)p.fbo.srgb_fbo,
                zm_wrap_to_str(p.wrap),
                zm_filter_to_str(p.filter),
                (int)p.mipmap,
                (unsigned)p.frame_count_mod,
                (int)p.feedback
            );
            OutputDebugStringA(b);

            if (has_path) {
                _snprintf(b, sizeof(b), "[ZeroMod]   pass[%u] path='%s'\n", i, p.source.path);
                OutputDebugStringA(b);
            }

            if (has_fstr) {
                // preview first ~120 chars
                char preview[160] = { 0 };
                strncpy(preview, p.source.string.fragment, 120);
                _snprintf(b, sizeof(b), "[ZeroMod]   pass[%u] frag_preview='%.120s'\n", i, preview);
                OutputDebugStringA(b);
            }

            if (has_vstr) {
                char preview[160] = { 0 };
                strncpy(preview, p.source.string.vertex, 120);
                _snprintf(b, sizeof(b), "[ZeroMod]   pass[%u] vert_preview='%.120s'\n", i, preview);
                OutputDebugStringA(b);
            }
        }

        unsigned luts = s->luts;
        if (luts > GFX_MAX_TEXTURES) luts = GFX_MAX_TEXTURES;

        for (unsigned i = 0; i < luts; i++)
        {
            const video_shader_lut& t = s->lut[i];
            _snprintf(b, sizeof(b),
                "[ZeroMod]  lut[%u]: id='%s' path='%s' wrap=%s filter=%s mip=%d\n",
                i, t.id, t.path, zm_wrap_to_str(t.wrap), zm_filter_to_str(t.filter), (int)t.mipmap);
            OutputDebugStringA(b);
        }

        unsigned params = s->num_parameters;
        if (params > GFX_MAX_PARAMETERS) params = GFX_MAX_PARAMETERS;

        for (unsigned i = 0; i < params; i++)
        {
            const video_shader_parameter& p = s->parameters[i];
            _snprintf(b, sizeof(b),
                "[ZeroMod]  param[%u]: id='%s' desc='%s' cur=%.6f init=%.6f min=%.6f max=%.6f step=%.6f pass=%d\n",
                i, p.id, p.desc, p.current, p.initial, p.minimum, p.maximum, p.step, p.pass);
            OutputDebugStringA(b);
        }
    }


    static void d3d9_clear_parsed_preset(d3d9_video_struct* d3d9)
    {
        if (!d3d9)
            return;

        unsigned passes = d3d9->shader.passes;
        if (passes > GFX_MAX_SHADERS)
            passes = GFX_MAX_SHADERS;

        for (unsigned i = 0; i < passes; i++)
        {
            free(d3d9->shader.pass[i].source.string.vertex);
            free(d3d9->shader.pass[i].source.string.fragment);
            d3d9->shader.pass[i].source.string.vertex = NULL;
            d3d9->shader.pass[i].source.string.fragment = NULL;
        }

        memset(&d3d9->shader, 0, sizeof(d3d9->shader));
        d3d9->shader_preset = false;
    }


    bool slang_d3d9_load_preset_parse_only(d3d9_video_struct* d3d9)
    {
#if defined(HAVE_SLANG)
        if (!d3d9 || d3d9->magic != 0x39564433)
            return false;

        if (!d3d9->shader_is_path || !d3d9->shader_path || !*d3d9->shader_path)
        {
            d3d9->shader_reload_pending = false;
            return false;
        }

        d3d9_clear_parsed_preset(d3d9);

        config_file_t* conf = video_shader_read_preset(d3d9->shader_path);
        if (!conf)
        {
            OutputDebugStringA("[ZeroMod] slang_d3d9_load_preset_parse_only: video_shader_read_preset failed\n");
            d3d9->shader_reload_pending = false;
            return false;
        }

        if (!video_shader_read_conf_preset(conf, &d3d9->shader))
        {
            OutputDebugStringA("[ZeroMod] slang_d3d9_load_preset_parse_only: video_shader_read_conf_preset failed\n");
            config_file_free(conf);
            d3d9_clear_parsed_preset(d3d9);
            d3d9->shader_reload_pending = false;
            return false;
        }

        video_shader_resolve_current_parameters(conf, &d3d9->shader);
        zm_log_shader_summary("parsed_preset", &d3d9->shader);
        config_file_free(conf);


        d3d9->shader_preset = true;
        d3d9->shader_reload_pending = false;

        {
            char b[512];
            _snprintf(b, sizeof(b),
                "[ZeroMod] slang preset parsed OK: path='%s' passes=%u luts=%u history=%u\n",
                d3d9->shader_path,
                (unsigned)d3d9->shader.passes,
                (unsigned)d3d9->shader.luts,
                (unsigned)d3d9->shader.history_size);
            OutputDebugStringA(b);
        }

        return true;
#else
        (void)d3d9;
        return false;
#endif
    }

} // namespace ZeroMod
