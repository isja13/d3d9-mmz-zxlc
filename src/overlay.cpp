#include "overlay.h"
#include "d3d9device.h"
#include "../imgui/imgui.h"
#include "../imgui/examples/imgui_impl_dx9.h"
#include "../imgui/examples/imgui_impl_win32.h"


#include "globals.h"

#include <windows.h>
#undef DBG
#define DBG(msg) do { OutputDebugStringA("[ZeroMod] "); OutputDebugStringA(msg); OutputDebugStringA("\n"); } while(0)


#define TEXT_DURATION 5.0

#include <iostream>
#include <deque>

namespace {
    cs_wrapper gui_cs;
}

class Overlay::Impl {
    friend class Overlay;

    bool imgui_inited = false;

    struct Text {
        std::string text;
        UINT64 time = 0;
    };
    std::deque<Text> texts;

    void push_text_base(std::string&& s) {
        std::cerr << s << std::endl;
        texts.emplace_back(Text{ std::move(s) });
    }
    cs_wrapper texts_cs;

    void begin_text() {
        texts_cs.begin_cs();
    }

    void end_text() {
        texts_cs.end_cs();
    }

    void reset_texts_timings() {
        for (Text& text : texts) text.time = 0;
    }

    HWND hwnd = NULL;
    MyID3D9Device* pDevice = nullptr;
   // IDirect3DSurface9* rtv = nullptr;      // our backbuffer surface
 //.   IDirect3DSurface9* old_rtv = nullptr;  // to restore after drawing

    ImGuiContext* imgui_context = NULL;
    ImGuiIO* io = NULL;
    UINT64 time = 0;
    UINT64 ticks_per_second = 0;
    ImVec2 display_size = {};

    void create_render_target() {
      //  if (!rtv && pDevice) {
        //    IDirect3DSurface9* bb = nullptr;
            // Safer than “swapchain-ish” calls: device is canonical in D3D9.
       //     HRESULT hr = pDevice->get_inner()->GetBackBuffer(
       //         0, 0, D3DBACKBUFFER_TYPE_MONO, &bb
      //      );
     //       DBG(hr == S_OK ? "Overlay: GetBackBuffer OK" : "Overlay: GetBackBuffer FAILED");
     //       if (SUCCEEDED(hr) && bb) {
      //          rtv = bb; // bb already AddRef'd
     //       }
      //  }
        DBG("create_render_target: before CreateDeviceObjects");
        ImGui_ImplDX9_CreateDeviceObjects();
        DBG("create_render_target: after CreateDeviceObjects");
        texts.emplace_back(Text{ "Overlay alive" });
    }

    void cleanup_render_target() {
        if (imgui_context) {  // or if (io) or if (pDevice)
            ImGui_ImplDX9_InvalidateDeviceObjects();
        }
    }

    void set_display_size(ImVec2 size) {
        display_size = size;
    }

    void set_display(D3DPRESENT_PARAMETERS* pp, MyID3D9Device* dev)
    {
        DBG("Overlay::Impl::set_display ENTER");

        reset_display();
        DBG("Overlay::Impl::set_display after reset_display");

        if (!(pp && dev)) {
            DBG("Overlay::Impl::set_display missing args");
            return;
        }

        hwnd = pp->hDeviceWindow;
        {
            char b[256];
            _snprintf(b, sizeof(b), "[ZeroMod] Overlay hwnd=%p IsWindow=%d\n",
                (void*)hwnd, hwnd ? (int)IsWindow(hwnd) : 0);
            OutputDebugStringA(b);
        }

        if (!hwnd || !IsWindow(hwnd)) {
            DBG("Overlay::Impl::set_display hwnd invalid");
            return;
        }
        {
            char b[256];
            _snprintf(b, sizeof(b),
                "[ZeroMod] OVR about to AddRef dev=%p inner=%p\n",
                (void*)dev,
                dev ? (void*)dev->get_inner() : nullptr
            );
            OutputDebugStringA(b);
        }

        pDevice = dev;
        OutputDebugStringA("[ZeroMod] OVR calling AddRef\n");
        pDevice->AddRef();
        OutputDebugStringA("[ZeroMod] OVR AddRef returned\n");
        DBG("Overlay::Impl::set_display after AddRef");

        OutputDebugStringA("[ZeroMod] OVR step: before CreateContext\n");
        imgui_context = ImGui::CreateContext();
        OutputDebugStringA("[ZeroMod] OVR step: after CreateContext\n");

        OutputDebugStringA("[ZeroMod] OVR step: before GetIO\n");
        io = &ImGui::GetIO();
        OutputDebugStringA("[ZeroMod] OVR step: after GetIO\n");

        io->IniFilename = NULL;

        OutputDebugStringA("[ZeroMod] OVR step: before Win32_Init\n");
        ImGui_ImplWin32_Init(hwnd);
        OutputDebugStringA("[ZeroMod] OVR step: after Win32_Init\n");

        ImGui::SetCurrentContext(imgui_context);

        IDirect3DDevice9* inner = pDevice ? pDevice->get_inner() : nullptr;

        char bb[256];
        _snprintf(bb, sizeof(bb), "[ZeroMod] OVR inner=%p\n", (void*)inner);
        OutputDebugStringA(bb);

        if (!inner) {
            OutputDebugStringA("[ZeroMod] OVR ERROR: inner is NULL\n");
            return;
        }

        // Hard sanity calls: if this crashes, your inner pointer is not valid.
        inner->AddRef();
        inner->Release();

        D3DDEVICE_CREATION_PARAMETERS cp = {};
        HRESULT hrCP = inner->GetCreationParameters(&cp);
        _snprintf(bb, sizeof(bb), "[ZeroMod] OVR GetCreationParameters hr=0x%08lX focus=%p\n",
            (unsigned long)hrCP, (void*)cp.hFocusWindow);
        OutputDebugStringA(bb);


        OutputDebugStringA("[ZeroMod] OVR step: before DX9_Init\n");
        ImGui_ImplDX9_Init(pDevice->get_inner());
        imgui_inited = true;
        OutputDebugStringA("[ZeroMod] OVR step: after DX9_Init\n");

        ImGui::StyleColorsClassic();
        DBG("Overlay::Impl::set_display after StyleColorsClassic");

        ImGuiStyle* style = &ImGui::GetStyle();
        style->WindowBorderSize = 0;
        DBG("Overlay::Impl::set_display after style");

        set_display_size(ImVec2(
            (float)pp->BackBufferWidth,
            (float)pp->BackBufferHeight
        ));
        DBG("Overlay::Impl::set_display after set_display_size");

        DBG("Overlay::Impl::set_display before create_render_target");
        create_render_target();
        DBG("Overlay::Impl::set_display EXIT OK");
    }


    void reset_display() {
        cleanup_render_target();

        set_display_size({});

        if (imgui_context) {
            ImGui::SetCurrentContext(imgui_context);
            ImGui_ImplDX9_Shutdown();
            ImGui_ImplWin32_Shutdown();
        }
        imgui_inited = false;

        io = NULL;

        if (imgui_context) {
            ImGui::DestroyContext(imgui_context);
            imgui_context = NULL;
        }

        if (pDevice) {
            // optional: pDevice->set_overlay(NULL); only if you actually use that link
            pDevice->Release();
            pDevice = nullptr;
        }

        hwnd = NULL;
    }

    Impl() {
        QueryPerformanceFrequency((LARGE_INTEGER*)&ticks_per_second);
    }

    ~Impl() {
        reset_display();
    }

    HRESULT resize_buffers(
        UINT buffer_count,
        UINT width,
        UINT height,
        D3DFORMAT format,
        UINT flags
    ) {
        (void)buffer_count; (void)format; (void)flags;

        DBG("Overlay::resize_buffers ENTER");

        reset_texts_timings();
        cleanup_render_target();

        set_display_size(ImVec2((float)width, (float)height));

        // reacquire backbuffer via device
        create_render_target();

        return S_OK;
    }


    void present(
        UINT SyncInterval,
        UINT Flags
    ) {
        if (!imgui_context || !io || !pDevice) return;

     //   if (!(texts.size() && gui_cs.try_begin_cs())) {
    //        time = 0;
      //      return;
       //
       // }

        if (!imgui_context || !io || !pDevice) return;
        if (!gui_cs.try_begin_cs()) { time = 0; return; }

        ImGui::SetCurrentContext(imgui_context);
        ImGui_ImplWin32_NewFrame();
        ImGui_ImplDX9_NewFrame();
        ImGui::NewFrame();


        if (!time || hwnd != GetForegroundWindow()) {
            reset_texts_timings();
            QueryPerformanceCounter((LARGE_INTEGER*)&time);
            io->DeltaTime = 1.0f / 60.0f;
        }
        else {
            UINT64 current_time = 0; // Initialize current_time
            QueryPerformanceCounter((LARGE_INTEGER*)&current_time);
            io->DeltaTime = (float)(current_time - time) / ticks_per_second;
            time = current_time;
        }
        io->DisplaySize = display_size;

      //  ImGui::NewFrame();
// prune first
        while (texts.size() && texts.front().time &&
            time - texts.front().time > ticks_per_second * TEXT_DURATION) {
            texts.pop_front();
        }

        bool began = false;

        if (!texts.empty()) {
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.35f);

            ImGui::Begin(
                "Overlay",
                NULL,
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoInputs
            );
            began = true;

            for (Text& text : texts) {
                if (!text.time) text.time = time;
                ImGui::TextUnformatted(text.text.c_str());
            }
        }

        if (began) {
            ImGui::End();
        }


        static int once = 0;
        if (once++ == 0) OutputDebugStringA("[ZeroMod] Overlay::present() is running\n");

        ImGui::Render();
        // Save current RT
 //       old_rtv = nullptr;
  //      pDevice->get_inner()->GetRenderTarget(0, &old_rtv);

        // Draw to backbuffer
//        pDevice->get_inner()->SetRenderTarget(0, rtv);
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

        // Restore
   //     if (old_rtv) {
   //         pDevice->get_inner()->SetRenderTarget(0, old_rtv);
    //        old_rtv->Release();
    //        old_rtv = nullptr;
     //   }


        gui_cs.end_cs();
    }
};

Overlay::Overlay() : impl(new Impl()) {}

Overlay::~Overlay() {
    delete impl;
}

void Overlay::set_display(D3DPRESENT_PARAMETERS* pp, MyID3D9Device* dev) {

    if (!pp || !dev) { DBG("Overlay::set_display missing args"); return; }
    if (!pp->hDeviceWindow) { DBG("Overlay::set_display: NULL hDeviceWindow"); return; }

    impl->begin_text();
    impl->set_display(pp, dev);
    impl->end_text();
}
void Overlay::set_display(
    D3DPRESENT_PARAMETERS* pp,
    MyIDXGISwapChain* sc,
    MyID3D9Device* dev
) {
    (void)sc; // ignored on purpose
    set_display(pp, dev);
}

HRESULT Overlay::present(UINT SyncInterval, UINT Flags) {
    (void)SyncInterval; (void)Flags;
    impl->begin_text();
    impl->present(SyncInterval, Flags);
    impl->end_text();
    return S_OK;
}


HRESULT Overlay::resize_buffers(
    UINT buffer_count,
    UINT width,
    UINT height,
    D3DFORMAT format,
    UINT flags
) {
    impl->begin_text();
    HRESULT ret = impl->resize_buffers(buffer_count, width, height, format, flags);
    impl->end_text();
    return ret;
}

void Overlay::push_text_base(std::string&& s) {
    impl->begin_text();
    impl->push_text_base(std::move(s));
    impl->end_text();
}

void Overlay::set_log_message(const std::string& message) {
    impl->begin_text();
    impl->push_text_base(std::string(message)); // Ensure correct type
    impl->end_text();
}

//OverlayPtr default_overlay;
