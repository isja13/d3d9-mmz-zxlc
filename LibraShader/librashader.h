/*
librashader.h
SPDX-License-Identifier: MIT
This file is part of the librashader C headers.

Copyright 2022 chyyran

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


#ifndef __LIBRASHADER_H__
#define __LIBRASHADER_H__

#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#if defined(_WIN32) && defined(LIBRA_RUNTIME_D3D11)
#include <d3d11.h>
#endif
#if defined(_WIN32) && defined(LIBRA_RUNTIME_D3D12)
#include <d3d12.h>
#endif
#if defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9)
#include <D3D9.h>
#endif
#if defined(__APPLE__) && defined(LIBRA_RUNTIME_METAL) && defined(__OBJC__)
#import <Metal/Metal.h>
#endif
#if defined(LIBRA_RUNTIME_VULKAN)
#include <vulkan/vulkan.h>
#endif

/// Error codes for librashader error types.
enum LIBRA_ERRNO
#ifdef __cplusplus
  : int32_t
#endif // __cplusplus
 {
  /// Error code for an unknown error.
  LIBRA_ERRNO_UNKNOWN_ERROR = 0,
  /// Error code for an invalid parameter.
  LIBRA_ERRNO_INVALID_PARAMETER = 1,
  /// Error code for an invalid (non-UTF8) string.
  LIBRA_ERRNO_INVALID_STRING = 2,
  /// Error code for a preset parser error.
  LIBRA_ERRNO_PRESET_ERROR = 3,
  /// Error code for a preprocessor error.
  LIBRA_ERRNO_PREPROCESS_ERROR = 4,
  /// Error code for a shader parameter error.
  LIBRA_ERRNO_SHADER_PARAMETER_ERROR = 5,
  /// Error code for a reflection error.
  LIBRA_ERRNO_REFLECT_ERROR = 6,
  /// Error code for a runtime error.
  LIBRA_ERRNO_RUNTIME_ERROR = 7,
};
#ifndef __cplusplus
typedef int32_t LIBRA_ERRNO;
#endif // __cplusplus

/// An enum representing orientation for use in preset contexts.
enum LIBRA_PRESET_CTX_ORIENTATION
#ifdef __cplusplus
  : uint32_t
#endif // __cplusplus
 {
  /// Context parameter for vertical orientation.
  LIBRA_PRESET_CTX_ORIENTATION_VERTICAL = 0,
  /// Context parameter for horizontal orientation.
  LIBRA_PRESET_CTX_ORIENTATION_HORIZONTAL,
};
#ifndef __cplusplus
typedef uint32_t LIBRA_PRESET_CTX_ORIENTATION;
#endif // __cplusplus

/// An enum representing graphics runtimes (video drivers) for use in preset contexts.
enum LIBRA_PRESET_CTX_RUNTIME
#ifdef __cplusplus
  : uint32_t
#endif // __cplusplus
 {
  /// No runtime.
  LIBRA_PRESET_CTX_RUNTIME_NONE = 0,
  /// OpenGL 3.3+
  LIBRA_PRESET_CTX_RUNTIME_GL_CORE,
  /// Vulkan
  LIBRA_PRESET_CTX_RUNTIME_VULKAN,
  /// Direct3D 11
  LIBRA_PRESET_CTX_RUNTIME_D3D11,
  /// Direct3D 12
  LIBRA_PRESET_CTX_RUNTIME_D3D12,
  /// Metal
  LIBRA_PRESET_CTX_RUNTIME_METAL,
  /// Direct3D 9
  LIBRA_PRESET_CTX_RUNTIME_D3D9_HLSL,
};
#ifndef __cplusplus
typedef uint32_t LIBRA_PRESET_CTX_RUNTIME;
#endif // __cplusplus

/// Opaque struct for a Direct3D 11 filter chain.
typedef struct _filter_chain_d3d11 _filter_chain_d3d11;

/// Opaque struct for a Direct3D 12 filter chain.
typedef struct _filter_chain_d3d12 _filter_chain_d3d12;

/// Opaque struct for a Direct3D 9 filter chain.
typedef struct _filter_chain_d3d9 _filter_chain_d3d9;

/// Opaque struct for an OpenGL filter chain.
typedef struct _filter_chain_gl _filter_chain_gl;

/// Opaque struct for a Metal filter chain.
typedef struct _filter_chain_mtl _filter_chain_mtl;

/// Opaque struct for a Vulkan filter chain.
typedef struct _filter_chain_vk _filter_chain_vk;

/// The error type for librashader C API.
typedef struct _libra_error _libra_error;

/// Opaque struct for a shader preset.
typedef struct _shader_preset _shader_preset;

/// Opaque struct for a preset context.
typedef struct _preset_ctx _preset_ctx;

/// A handle to a librashader error object.
typedef struct _libra_error *libra_error_t;

/// A handle to a shader preset object.
typedef struct _shader_preset *libra_shader_preset_t;

/// A handle to a preset wildcard context object.
typedef struct _preset_ctx *libra_preset_ctx_t;

/// API version type alias.
typedef size_t LIBRASHADER_API_VERSION;

/// Options struct for loading shader presets.
///
/// Using this struct with `libra_preset_create_with_options` is the only way to
/// enable extended shader preset features.
typedef struct libra_preset_opt_t {
  /// The librashader API version.
  LIBRASHADER_API_VERSION version;
  /// Enables `_HAS_ORIGINALASPECT_UNIFORMS` behaviour.
  ///
  /// If this is true, then `frame_options.aspect_ratio` must be set for correct behaviour of shaders.
  ///
  /// This is only supported on API 2 and above, otherwise this has no effect.
  bool original_aspect_uniforms;
  /// Enables `_HAS_FRAMETIME_UNIFORMS` behaviour.
  ///
  /// If this is true, then `frame_options.frames_per_second` and `frame_options.frametime_delta`
  /// must be set for correct behaviour of shaders.
  ///
  /// This is only supported on API 2 and above, otherwise this has no effect.
  bool frametime_uniforms;
} libra_preset_opt_t;

/// A preset parameter.
typedef struct libra_preset_param_t {
  /// The name of the parameter
  const char *name;
  /// The description of the parameter.
  const char *description;
  /// The initial value the parameter is set to.
  float initial;
  /// The minimum value that the parameter can be set to.
  float minimum;
  /// The maximum value that the parameter can be set to.
  float maximum;
  /// The step by which this parameter can be incremented or decremented.
  float step;
} libra_preset_param_t;

/// A list of preset parameters.
typedef struct libra_preset_param_list_t {
  /// A pointer to the parameter
  const struct libra_preset_param_t *parameters;
  /// The number of parameters in the list. This field
  /// is readonly, and changing it will lead to undefined
  /// behaviour on free.
  uint64_t length;
} libra_preset_param_list_t;

/// Defines the output origin for a rendered frame.
typedef struct libra_viewport_t {
  /// The x offset in the viewport framebuffer to begin rendering from.
  float x;
  /// The y offset in the viewport framebuffer to begin rendering from.
  float y;
  /// The width extent of the viewport framebuffer to end rendering, relative to
  /// the origin specified by x.
  uint32_t width;
  /// The height extent of the viewport framebuffer to end rendering, relative to
  /// the origin specified by y.
  uint32_t height;
} libra_viewport_t;

#if (defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9))
/// Options for Direct3D 11 filter chain creation.
typedef struct filter_chain_d3d9_opt_t {
  /// The librashader API version.
  LIBRASHADER_API_VERSION version;
  /// Whether or not to explicitly disable mipmap
  /// generation regardless of shader preset settings.
  bool force_no_mipmaps;
  /// Disable the shader object cache. Shaders will be
  /// recompiled rather than loaded from the cache.
  bool disable_cache;
} filter_chain_d3d9_opt_t;
#endif

#if (defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9))
/// A handle to a Direct3D 11 filter chain.
typedef struct _filter_chain_d3d9 *libra_d3d9_filter_chain_t;
#endif

#if (defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9))
/// Options for each Direct3D 11 shader frame.
typedef struct frame_d3d9_opt_t {
  /// The librashader API version.
  LIBRASHADER_API_VERSION version;
  /// Whether or not to clear the history buffers.
  bool clear_history;
  /// The direction of rendering.
  /// -1 indicates that the frames are played in reverse order.
  int32_t frame_direction;
  /// The rotation of the output. 0 = 0deg, 1 = 90deg, 2 = 180deg, 3 = 270deg.
  uint32_t rotation;
  /// The total number of subframes ran. Default is 1.
  uint32_t total_subframes;
  /// The current sub frame. Default is 1.
  uint32_t current_subframe;
  /// The expected aspect ratio of the source image.
  ///
  /// This can differ from the actual aspect ratio of the source
  /// image.
  ///
  /// The default is 0, which will automatically
  /// infer the ratio from the source image.
  float aspect_ratio;
  /// The original frames per second of the source. Default is 1.
  float frames_per_second;
  /// Time in milliseconds between the current and previous frame. Default is 0.
  uint32_t frametime_delta;
} frame_d3d9_opt_t;
#endif

/// ABI version type alias.
typedef size_t LIBRASHADER_ABI_VERSION;

/// Function pointer definition for libra_abi_version
typedef LIBRASHADER_ABI_VERSION (*PFN_libra_instance_abi_version)(void);

/// Function pointer definition for libra_abi_version
typedef LIBRASHADER_API_VERSION (*PFN_libra_instance_api_version)(void);

/// Function pointer definition for
///libra_preset_create
typedef libra_error_t (*PFN_libra_preset_create)(const char *filename, libra_shader_preset_t *out);

/// Function pointer definition for
///libra_preset_free
typedef libra_error_t (*PFN_libra_preset_free)(libra_shader_preset_t *preset);

/// Function pointer definition for
///libra_preset_set_param
typedef libra_error_t (*PFN_libra_preset_set_param)(libra_shader_preset_t *preset,
                                                    const char *name,
                                                    float value);

/// Function pointer definition for
///libra_preset_get_param
typedef libra_error_t (*PFN_libra_preset_get_param)(const libra_shader_preset_t *preset,
                                                    const char *name,
                                                    float *value);

/// Function pointer definition for
///libra_preset_print
typedef libra_error_t (*PFN_libra_preset_print)(libra_shader_preset_t *preset);

/// Function pointer definition for
///libra_preset_get_runtime_params
typedef libra_error_t (*PFN_libra_preset_get_runtime_params)(const libra_shader_preset_t *preset,
                                                             struct libra_preset_param_list_t *out);

/// Function pointer definition for
///libra_preset_free_runtime_params
typedef libra_error_t (*PFN_libra_preset_free_runtime_params)(struct libra_preset_param_list_t preset);

/// Function pointer definition for
///libra_preset_create_with_context
typedef libra_error_t (*PFN_libra_preset_create_with_context)(const char *filename,
                                                              libra_preset_ctx_t *context,
                                                              libra_shader_preset_t *out);

/// Function pointer definition for
///libra_preset_create_with_options
typedef libra_error_t (*PFN_libra_preset_create_with_options)(const char *filename,
                                                              libra_preset_ctx_t *context,
                                                              struct libra_preset_opt_t *options,
                                                              libra_shader_preset_t *out);

/// Function pointer definition for
///libra_preset_ctx_create
typedef libra_error_t (*PFN_libra_preset_ctx_create)(libra_preset_ctx_t *out);

/// Function pointer definition for
///libra_preset_ctx_free
typedef libra_error_t (*PFN_libra_preset_ctx_free)(libra_preset_ctx_t *context);

/// Function pointer definition for
///libra_preset_ctx_set_core_name
typedef libra_error_t (*PFN_libra_preset_ctx_set_core_name)(libra_preset_ctx_t *context,
                                                            const char *name);

/// Function pointer definition for
///libra_preset_ctx_set_content_dir
typedef libra_error_t (*PFN_libra_preset_ctx_set_content_dir)(libra_preset_ctx_t *context,
                                                              const char *name);

/// Function pointer definition for
///libra_preset_ctx_set_param
typedef libra_error_t (*PFN_libra_preset_ctx_set_param)(libra_preset_ctx_t *context,
                                                        const char *name,
                                                        const char *value);

/// Function pointer definition for
///libra_preset_ctx_set_core_rotation
typedef libra_error_t (*PFN_libra_preset_ctx_set_core_rotation)(libra_preset_ctx_t *context,
                                                                uint32_t value);

/// Function pointer definition for
///libra_preset_ctx_set_user_rotation
typedef libra_error_t (*PFN_libra_preset_ctx_set_user_rotation)(libra_preset_ctx_t *context,
                                                                uint32_t value);

/// Function pointer definition for
///libra_preset_ctx_set_screen_orientation
typedef libra_error_t (*PFN_libra_preset_ctx_set_screen_orientation)(libra_preset_ctx_t *context,
                                                                     uint32_t value);

/// Function pointer definition for
///libra_preset_ctx_set_allow_rotation
typedef libra_error_t (*PFN_libra_preset_ctx_set_allow_rotation)(libra_preset_ctx_t *context,
                                                                 bool value);

/// Function pointer definition for
///libra_preset_ctx_set_view_aspect_orientation
typedef libra_error_t (*PFN_libra_preset_ctx_set_view_aspect_orientation)(libra_preset_ctx_t *context,
                                                                          LIBRA_PRESET_CTX_ORIENTATION value);

/// Function pointer definition for
///libra_preset_ctx_set_core_aspect_orientation
typedef libra_error_t (*PFN_libra_preset_ctx_set_core_aspect_orientation)(libra_preset_ctx_t *context,
                                                                          LIBRA_PRESET_CTX_ORIENTATION value);

/// Function pointer definition for
///libra_preset_ctx_set_runtime
typedef libra_error_t (*PFN_libra_preset_ctx_set_runtime)(libra_preset_ctx_t *context,
                                                          LIBRA_PRESET_CTX_RUNTIME value);

/// Function pointer definition for libra_error_errno
typedef LIBRA_ERRNO (*PFN_libra_error_errno)(libra_error_t error);

/// Function pointer definition for libra_error_print
typedef int32_t (*PFN_libra_error_print)(libra_error_t error);

/// Function pointer definition for libra_error_free
typedef int32_t (*PFN_libra_error_free)(libra_error_t *error);

/// Function pointer definition for libra_error_write
typedef int32_t (*PFN_libra_error_write)(libra_error_t error, char **out);

/// Function pointer definition for libra_error_free_string
typedef int32_t (*PFN_libra_error_free_string)(char **out);

#if (defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9))
/// Function pointer definition for
///libra_d3d9_filter_chain_create
typedef libra_error_t (*PFN_libra_d3d9_filter_chain_create)(libra_shader_preset_t *preset,
                                                            IDirect3DDevice9 * device,
                                                            const struct filter_chain_d3d9_opt_t *options,
                                                            libra_d3d9_filter_chain_t *out);
#endif

#if (defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9))
/// Function pointer definition for
///libra_d3d9_filter_chain_frame
typedef libra_error_t (*PFN_libra_d3d9_filter_chain_frame)(libra_d3d9_filter_chain_t *chain,
                                                           size_t frame_count,
                                                           IDirect3DTexture9 * image,
                                                           IDirect3DSurface9 * out,
                                                           const struct libra_viewport_t *viewport,
                                                           const float *mvp,
                                                           const struct frame_d3d9_opt_t *options);
#endif

#if (defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9))
/// Function pointer definition for
///libra_d3d9_filter_chain_set_param
typedef libra_error_t (*PFN_libra_d3d9_filter_chain_set_param)(libra_d3d9_filter_chain_t *chain,
                                                               const char *param_name,
                                                               float value);
#endif

#if (defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9))
/// Function pointer definition for
///libra_d3d9_filter_chain_get_param
typedef libra_error_t (*PFN_libra_d3d9_filter_chain_get_param)(const libra_d3d9_filter_chain_t *chain,
                                                               const char *param_name,
                                                               float *out);
#endif

#if (defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9))
/// Function pointer definition for
///libra_d3d9_filter_chain_set_active_pass_count
typedef libra_error_t (*PFN_libra_d3d9_filter_chain_set_active_pass_count)(libra_d3d9_filter_chain_t *chain,
                                                                           uint32_t value);
#endif

#if (defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9))
/// Function pointer definition for
///libra_d3d9_filter_chain_get_active_pass_count
typedef libra_error_t (*PFN_libra_d3d9_filter_chain_get_active_pass_count)(const libra_d3d9_filter_chain_t *chain,
                                                                           uint32_t *out);
#endif

#if (defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9))
/// Function pointer definition for
///libra_d3d9_filter_chain_free
typedef libra_error_t (*PFN_libra_d3d9_filter_chain_free)(libra_d3d9_filter_chain_t *chain);
#endif

/// The current version of the librashader API.
/// Pass this into `version` for config structs.
///
/// API versions are backwards compatible. It is valid to load
/// a librashader C API instance for all API versions less than
/// or equal to LIBRASHADER_CURRENT_VERSION, and subsequent API
/// versions must remain backwards compatible.
/// ## API Versions
/// - API version 0: 0.1.0
/// - API version 1: 0.2.0
///     - Added rotation, total_subframes, current_subframes to frame options
///     - Added preset context API
///     - Added Metal runtime API
/// - API version 2: 0.6.0
///     - Added original aspect uniforms
///     - Added frame time uniforms
#define LIBRASHADER_CURRENT_VERSION 2

/// The current version of the librashader ABI.
/// Used by the loader to check ABI compatibility.
///
/// ABI version 0 is reserved as a sentinel value.
///
/// ABI versions are not backwards compatible. It is not
/// valid to load a librashader C API instance for any ABI
/// version not equal to LIBRASHADER_CURRENT_ABI.
///
/// ## ABI Versions
/// - ABI version 0: null instance (unloaded)
/// - ABI version 1: 0.1.0
/// - ABI version 2: 0.5.0
///     - Reduced texture size information needed for some runtimes.
///     - Removed wrapper structs for Direct3D 11 SRV and RTV handles.
///     - Removed `gl_context_init`.
///     - Make viewport handling consistent across runtimes, which are now
///       span the output render target if omitted.
#define LIBRASHADER_CURRENT_ABI 2

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/// Get the error code corresponding to this error object.
///
/// ## Safety
///   - `error` must be valid and initialized.
LIBRA_ERRNO libra_error_errno(libra_error_t error);

/// Print the error message.
///
/// If `error` is null, this function does nothing and returns 1. Otherwise, this function returns 0.
/// ## Safety
///   - `error` must be a valid and initialized instance of `libra_error_t`.
int32_t libra_error_print(libra_error_t error);

/// Frees any internal state kept by the error.
///
/// If `error` is null, this function does nothing and returns 1. Otherwise, this function returns 0.
/// The resulting error object becomes null.
/// ## Safety
///   - `error` must be null or a pointer to a valid and initialized instance of `libra_error_t`.
int32_t libra_error_free(libra_error_t *error);

/// Writes the error message into `out`
///
/// If `error` is null, this function does nothing and returns 1. Otherwise, this function returns 0.
/// ## Safety
///   - `error` must be a valid and initialized instance of `libra_error_t`.
///   - `out` must be a non-null pointer. The resulting string must not be modified.
int32_t libra_error_write(libra_error_t error,
                          char **out);

/// Frees an error string previously allocated by `libra_error_write`.
///
/// After freeing, the pointer will be set to null.
/// ## Safety
///   - If `libra_error_write` is not null, it must point to a string previously returned by `libra_error_write`.
///     Attempting to free anything else, including strings or objects from other librashader functions, is immediate
///     Undefined Behaviour.
int32_t libra_error_free_string(char **out);

/// Load a preset.
///
/// This function is deprecated, and `libra_preset_create_with_options` should be used instead.
/// ## Safety
///  - `filename` must be either null or a valid, aligned pointer to a string path to the shader preset.
///  - `out` must be either null, or an aligned pointer to an uninitialized or invalid `libra_shader_preset_t`.
/// ## Returns
///  - If any parameters are null, `out` is unchanged, and this function returns `LIBRA_ERR_INVALID_PARAMETER`.
libra_error_t libra_preset_create(const char *filename,
                                  libra_shader_preset_t *out);

/// Load a preset with the given wildcard context.
///
/// The wildcard context is immediately invalidated and must be recreated after
/// the preset is created.
///
/// Path information variables `PRESET_DIR` and `PRESET` will automatically be filled in.
///
/// This function is deprecated, and `libra_preset_create_with_options` should be used instead.
/// ## Safety
///  - `filename` must be either null or a valid, aligned pointer to a string path to the shader preset.
///  - `context` must be either null or a valid, aligned pointer to a initialized `libra_preset_ctx_t`.
///  - `context` is  invalidated after this function returns.
///  - `out` must be either null, or an aligned pointer to an uninitialized or invalid `libra_shader_preset_t`.
/// ## Returns
///  - If any parameters are null, `out` is unchanged, and this function returns `LIBRA_ERR_INVALID_PARAMETER`.
libra_error_t libra_preset_create_with_context(const char *filename,
                                               libra_preset_ctx_t *context,
                                               libra_shader_preset_t *out);

/// Load a preset with optional options and an optional context.
///
/// Both `context` and `options` may be null.
///
/// If `context` is null, then a default context will be provided, and this function will not return `LIBRA_ERR_INVALID_PARAMETER`.
/// If `options` is null, then default options will be chosen.
///
/// If `context` is provided, it is immediately invalidated and must be recreated after
/// the preset is created.
///
/// ## Safety
///  - `filename` must be either null or a valid, aligned pointer to a string path to the shader preset.
///  - `context` must be either null or a valid, aligned pointer to an initialized `libra_preset_ctx_t`.
///  - `options` must be either null, or a valid, aligned pointer to a `libra_shader_opt_t`.
///    `LIBRASHADER_API_VERSION` should be set to `LIBRASHADER_CURRENT_VERSION`.
///  - `out` must be either null, or an aligned pointer to an uninitialized or invalid `libra_shader_preset_t`.
///
/// ## Returns
///  - If `out` or `filename` is null, `out` is unchanged, and this function returns `LIBRA_ERR_INVALID_PARAMETER`.
libra_error_t libra_preset_create_with_options(const char *filename,
                                               libra_preset_ctx_t *context,
                                               struct libra_preset_opt_t *options,
                                               libra_shader_preset_t *out);

/// Free the preset.
///
/// If `preset` is null, this function does nothing. The resulting value in `preset` then becomes
/// null.
///
/// ## Safety
/// - `preset` must be a valid and aligned pointer to a `libra_shader_preset_t`.
libra_error_t libra_preset_free(libra_shader_preset_t *preset);

/// Set the value of the parameter in the preset.
///
/// ## Safety
/// - `preset` must be null or a valid and aligned pointer to a `libra_shader_preset_t`.
/// - `name` must be null or a valid and aligned pointer to a string.
libra_error_t libra_preset_set_param(libra_shader_preset_t *preset, const char *name, float value);

/// Get the value of the parameter as set in the preset.
///
/// ## Safety
/// - `preset` must be null or a valid and aligned pointer to a shader preset.
/// - `name` must be null or a valid and aligned pointer to a string.
/// - `value` may be a pointer to a uninitialized `float`.
libra_error_t libra_preset_get_param(const libra_shader_preset_t *preset,
                                     const char *name,
                                     float *value);

/// Pretty print the shader preset.
///
/// ## Safety
/// - `preset` must be null or a valid and aligned pointer to a `libra_shader_preset_t`.
libra_error_t libra_preset_print(libra_shader_preset_t *preset);

/// Get a list of runtime parameters.
///
/// ## Safety
/// - `preset` must be null or a valid and aligned pointer to a `libra_shader_preset_t`.
/// - `out` must be an aligned pointer to a `libra_preset_parameter_list_t`.
/// - The output struct should be treated as immutable. Mutating any struct fields
///   in the returned struct may at best cause memory leaks, and at worse
///   cause undefined behaviour when later freed.
/// - It is safe to call `libra_preset_get_runtime_params` multiple times, however
///   the output struct must only be freed once per call.
libra_error_t libra_preset_get_runtime_params(const libra_shader_preset_t *preset,
                                              struct libra_preset_param_list_t *out);

/// Free the runtime parameters.
///
/// Unlike the other `free` functions provided by librashader,
/// `libra_preset_free_runtime_params` takes the struct directly.
/// The caller must take care to maintain the lifetime of any pointers
/// contained within the input `libra_preset_param_list_t`.
///
/// ## Safety
/// - Any pointers rooted at `parameters` becomes invalid after this function returns,
///   including any strings accessible via the input `libra_preset_param_list_t`.
///   The caller must ensure that there are no live pointers, aliased or unaliased,
///   to data accessible via the input `libra_preset_param_list_t`.
///
/// - Accessing any data pointed to via the input `libra_preset_param_list_t` after it
///   has been freed is a use-after-free and is immediate undefined behaviour.
///
/// - If any struct fields of the input `libra_preset_param_list_t` was modified from
///   their values given after `libra_preset_get_runtime_params`, this may result
///   in undefined behaviour.
libra_error_t libra_preset_free_runtime_params(struct libra_preset_param_list_t preset);

#if (defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9))
/// Create the filter chain given the shader preset.
///
/// The shader preset is immediately invalidated and must be recreated after
/// the filter chain is created.
///
/// ## Safety:
/// - `preset` must be either null, or valid and aligned.
/// - `options` must be either null, or valid and aligned.
/// - `device` must not be null.
/// - `out` must be aligned, but may be null, invalid, or uninitialized.
libra_error_t libra_d3d9_filter_chain_create(libra_shader_preset_t *preset,
                                             IDirect3DDevice9 * device,
                                             const struct filter_chain_d3d9_opt_t *options,
                                             libra_d3d9_filter_chain_t *out);
#endif

#if (defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9))
/// Draw a frame with the given parameters for the given filter chain.
///
/// ## Parameters
/// - `chain` is a handle to the filter chain.
/// - `frame_count` is the number of frames passed to the shader
/// - `image` is a pointer to a `IDirect3DTexture9` that will serve as the source image for the frame.
/// - `out` is a pointer to a `IDirect3DSurface9` that will serve as the render target for the frame.
///
/// - `viewport` is a pointer to a `libra_viewport_t` that specifies the area onto which scissor and viewport
///    will be applied to the render target. It may be null, in which case a default viewport spanning the
///    entire render target will be used.
/// - `mvp` is a pointer to an array of 16 `float` values to specify the model view projection matrix to
///    be passed to the shader.
/// - `options` is a pointer to options for the frame. Valid options are dependent on the `LIBRASHADER_API_VERSION`
///    passed in. It may be null, in which case default options for the filter chain are used.
///
/// ## Safety
/// - `chain` may be null, invalid, but not uninitialized. If `chain` is null or invalid, this
///    function will return an error.
/// - `viewport` may be null, or if it is not null, must be an aligned pointer to an instance of `libra_viewport_t`.
/// - `mvp` may be null, or if it is not null, must be an aligned pointer to 16 consecutive `float`
///    values for the model view projection matrix.
/// - `opt` may be null, or if it is not null, must be an aligned pointer to a valid `frame_d3d9_opt_t`
///    struct.
/// - `out` must not be null.
/// - `image` must not be null.
/// - You must ensure that only one thread has access to `chain` before you call this function. Only one
///   thread at a time may call this function.
libra_error_t libra_d3d9_filter_chain_frame(libra_d3d9_filter_chain_t *chain,
                                            size_t frame_count,
                                            IDirect3DTexture9 * image,
                                            IDirect3DSurface9 * out,
                                            const struct libra_viewport_t *viewport,
                                            const float *mvp,
                                            const struct frame_d3d9_opt_t *options);
#endif

#if (defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9))
/// Sets a parameter for the filter chain.
///
/// If the parameter does not exist, returns an error.
/// ## Safety
/// - `chain` must be either null or a valid and aligned pointer to an initialized `libra_d3d9_filter_chain_t`.
/// - `param_name` must be either null or a null terminated string.
libra_error_t libra_d3d9_filter_chain_set_param(libra_d3d9_filter_chain_t *chain,
                                                const char *param_name,
                                                float value);
#endif

#if (defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9))
/// Gets a parameter for the filter chain.
///
/// If the parameter does not exist, returns an error.
/// ## Safety
/// - `chain` must be either null or a valid and aligned pointer to an initialized `libra_d3d9_filter_chain_t`.
/// - `param_name` must be either null or a null terminated string.
libra_error_t libra_d3d9_filter_chain_get_param(const libra_d3d9_filter_chain_t *chain,
                                                const char *param_name,
                                                float *out);
#endif

#if (defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9))
/// Sets the number of active passes for this chain.
///
/// ## Safety
/// - `chain` must be either null or a valid and aligned pointer to an initialized `libra_d3d9_filter_chain_t`.
libra_error_t libra_d3d9_filter_chain_set_active_pass_count(libra_d3d9_filter_chain_t *chain,
                                                            uint32_t value);
#endif

#if (defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9))
/// Gets the number of active passes for this chain.
///
/// ## Safety
/// - `chain` must be either null or a valid and aligned pointer to an initialized `libra_d3d9_filter_chain_t`.
libra_error_t libra_d3d9_filter_chain_get_active_pass_count(const libra_d3d9_filter_chain_t *chain,
                                                            uint32_t *out);
#endif

#if (defined(_WIN32) && defined(LIBRA_RUNTIME_D3D9))
/// Free a d3d9 filter chain.
///
/// The resulting value in `chain` then becomes null.
/// ## Safety
/// - `chain` must be either null or a valid and aligned pointer to an initialized `libra_d3d9_filter_chain_t`.
libra_error_t libra_d3d9_filter_chain_free(libra_d3d9_filter_chain_t *chain);
#endif

/// Get the ABI version of the loaded instance.
LIBRASHADER_ABI_VERSION libra_instance_abi_version(void);

/// Get the API version of the loaded instance.
LIBRASHADER_API_VERSION libra_instance_api_version(void);

/// Create a wildcard context
///
/// The C API does not allow directly setting certain variables
///
/// - `PRESET_DIR` and `PRESET` are inferred on preset creation.
/// - `VID-DRV-SHADER-EXT` and `VID-DRV-PRESET-EXT` are always set to `slang` and `slangp` for librashader.
/// - `VID-FINAL-ROT` is automatically calculated as the sum of `VID-USER-ROT` and `CORE-REQ-ROT` if either are present.
///
/// These automatically inferred variables, as well as all other variables can be overridden with
/// `libra_preset_ctx_set_param`, but the expected string values must be provided.
/// See <https://github.com/libretro/RetroArch/pull/15023> for a list of expected string values.
///
/// No variables can be removed once added to the context, however subsequent calls to set the same
/// variable will overwrite the expected variable.
/// ## Safety
///  - `out` must be either null, or an aligned pointer to an uninitialized or invalid `libra_preset_ctx_t`.
/// ## Returns
///  - If any parameters are null, `out` is unchanged, and this function returns `LIBRA_ERR_INVALID_PARAMETER`.
libra_error_t libra_preset_ctx_create(libra_preset_ctx_t *out);

/// Free the wildcard context.
///
/// If `context` is null, this function does nothing. The resulting value in `context` then becomes
/// null.
///
/// ## Safety
/// - `context` must be a valid and aligned pointer to a `libra_preset_ctx_t`
libra_error_t libra_preset_ctx_free(libra_preset_ctx_t *context);

/// Set the core name (`CORE`) variable in the context
///
/// ## Safety
/// - `context` must be null or a valid and aligned pointer to a `libra_preset_ctx_t`.
/// - `name` must be null or a valid and aligned pointer to a string.
libra_error_t libra_preset_ctx_set_core_name(libra_preset_ctx_t *context, const char *name);

/// Set the content directory (`CONTENT-DIR`) variable in the context.
///
/// ## Safety
/// - `context` must be null or a valid and aligned pointer to a `libra_preset_ctx_t`.
/// - `name` must be null or a valid and aligned pointer to a string.
libra_error_t libra_preset_ctx_set_content_dir(libra_preset_ctx_t *context, const char *name);

/// Set a custom string variable in context.
///
/// If the path contains this variable when loading a preset, it will be replaced with the
/// provided contents.
///
/// ## Safety
/// - `context` must be null or a valid and aligned pointer to a `libra_preset_ctx_t`.
/// - `name` must be null or a valid and aligned pointer to a string.
/// - `value` must be null or a valid and aligned pointer to a string.
libra_error_t libra_preset_ctx_set_param(libra_preset_ctx_t *context,
                                         const char *name,
                                         const char *value);

/// Set the graphics runtime (`VID-DRV`) variable in the context.
///
/// Note that librashader only supports the following runtimes.
///
/// - Vulkan
/// - GLCore
/// - Direct3D11
/// - Direct3D12
/// - Metal
/// - Direct3D9 (HLSL)
///
/// This will also set the appropriate video driver extensions.
///
/// For librashader, `VID-DRV-SHADER-EXT` and `VID-DRV-PRESET-EXT` are always `slang` and `slangp`.
/// To override this, use `libra_preset_ctx_set_param`.
///
/// ## Safety
/// - `context` must be null or a valid and aligned pointer to a `libra_preset_ctx_t`.
/// - `name` must be null or a valid and aligned pointer to a string.
libra_error_t libra_preset_ctx_set_runtime(libra_preset_ctx_t *context,
                                           LIBRA_PRESET_CTX_RUNTIME value);

/// Set the core requested rotation (`CORE-REQ-ROT`) variable in the context.
///
/// Rotation is represented by quarter rotations around the unit circle.
/// For example. `0` = 0deg, `1` = 90deg, `2` = 180deg, `3` = 270deg, `4` = 0deg.
///
/// ## Safety
/// - `context` must be null or a valid and aligned pointer to a `libra_preset_ctx_t`.
libra_error_t libra_preset_ctx_set_core_rotation(libra_preset_ctx_t *context, uint32_t value);

/// Set the user rotation (`VID-USER-ROT`) variable in the context.
///
/// Rotation is represented by quarter rotations around the unit circle.
/// For example. `0` = 0deg, `1` = 90deg, `2` = 180deg, `3` = 270deg, `4` = 0deg.
///
/// ## Safety
/// - `context` must be null or a valid and aligned pointer to a `libra_preset_ctx_t`.
libra_error_t libra_preset_ctx_set_user_rotation(libra_preset_ctx_t *context, uint32_t value);

/// Set the screen orientation (`SCREEN-ORIENT`) variable in the context.
///
/// Orientation is represented by quarter rotations around the unit circle.
/// For example. `0` = 0deg, `1` = 90deg, `2` = 180deg, `3` = 270deg, `4` = 0deg.
///
/// ## Safety
/// - `context` must be null or a valid and aligned pointer to a `libra_preset_ctx_t`.
libra_error_t libra_preset_ctx_set_screen_orientation(libra_preset_ctx_t *context, uint32_t value);

/// Set whether or not to allow rotation (`VID-ALLOW-CORE-ROT`) variable in the context.
///
/// ## Safety
/// - `context` must be null or a valid and aligned pointer to a `libra_preset_ctx_t`.
libra_error_t libra_preset_ctx_set_allow_rotation(libra_preset_ctx_t *context, bool value);

/// Set the view aspect orientation (`VIEW-ASPECT-ORIENT`) variable in the context.
/// ## Safety
/// - `context` must be null or a valid and aligned pointer to a `libra_preset_ctx_t`.
libra_error_t libra_preset_ctx_set_view_aspect_orientation(libra_preset_ctx_t *context,
                                                           LIBRA_PRESET_CTX_ORIENTATION value);

/// Set the core aspect orientation (`CORE-ASPECT-ORIENT`) variable in the context.
/// ## Safety
/// - `context` must be null or a valid and aligned pointer to a `libra_preset_ctx_t`.
libra_error_t libra_preset_ctx_set_core_aspect_orientation(libra_preset_ctx_t *context,
                                                           LIBRA_PRESET_CTX_ORIENTATION value);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  /* __LIBRASHADER_H__ */
