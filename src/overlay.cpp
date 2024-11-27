#include "overlay.h"
#include "dxgiswapchain.h"
#include "d3d9device.h"
#include "../imgui/imgui.h"
#include "../imgui/examples/imgui_impl_dx9.h"
#include "../imgui/examples/imgui_impl_win32.h"

#include "globals.h"

#define TEXT_DURATION 2.5

namespace {
    cs_wrapper gui_cs;
}

class Overlay::Impl {
    friend class Overlay;

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
    MyID3D9Device* pDevice = NULL;
    MyIDXGISwapChain* pSwapChain = NULL;
    IDirect3DSurface9* rtv = NULL;
    ImGuiContext* imgui_context = NULL;
    ImGuiIO* io = NULL;
    UINT64 time = 0;
    UINT64 ticks_per_second = 0;
    ImVec2 display_size = {};

    void create_render_target() {
        if (!rtv) {
            IDirect3DSurface9* pBackBuffer = NULL;
            pSwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
            if (pBackBuffer) {
                rtv = pBackBuffer; // No need to create render target view separately in DX9
            }
        }
        ImGui_ImplDX9_CreateDeviceObjects();
    }

    void cleanup_render_target() {
        ImGui_ImplDX9_InvalidateDeviceObjects();
        if (rtv) {
            rtv->Release();
            rtv = NULL;
        }
    }

    void set_display_size(ImVec2 size) {
        display_size = size;
    }

    void set_display(
        D3DPRESENT_PARAMETERS* pSwapChainDesc,
        MyIDXGISwapChain* pSwapChain,
        MyID3D9Device* pDevice  // Changed from MyIDirect3DDevice9 to MyID3D9Device
    ) {
        reset_display();
        if (!(pSwapChainDesc && pSwapChain && pDevice)) return;

        hwnd = pSwapChainDesc->hDeviceWindow;
        this->pDevice = pDevice;
        this->pSwapChain = pSwapChain;

        pDevice->AddRef();
        pSwapChain->AddRef();

        imgui_context = ImGui::CreateContext();
        io = &ImGui::GetIO();
        io->IniFilename = NULL;
        ImGui_ImplDX9_Init(pDevice->get_inner()); // Ensure get_inner() is defined in MyID3D9Device
        ImGui::StyleColorsClassic();
        ImGuiStyle* style = &ImGui::GetStyle();
        style->WindowBorderSize = 0;
        set_display_size(ImVec2(
            pSwapChainDesc->BackBufferWidth,
            pSwapChainDesc->BackBufferHeight
        ));

        create_render_target();
    }

    void reset_display() {
        cleanup_render_target();

        set_display_size({});
        ImGui_ImplDX9_Shutdown();
        io = NULL;
        if (imgui_context) {
            ImGui::DestroyContext(imgui_context);
            imgui_context = NULL;
        }

        if (pDevice) {
            pDevice->set_overlay(NULL);
            pDevice->Release();
            pDevice = NULL;
        }
        if (pSwapChain) {
            pSwapChain->set_overlay(NULL);
            pSwapChain->Release();
            pSwapChain = NULL;
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
        reset_texts_timings();
        cleanup_render_target();
        HRESULT ret = pSwapChain->GetBuffer(
            0,
            IID_IDirect3DSurface9,
            (void**)&rtv
        ); // Ensure the correct method is used
        if (ret == S_OK) {
            set_display_size(ImVec2(width, height));
            create_render_target();
        }
        return ret;
    }

    void present(
        UINT SyncInterval,
        UINT Flags
    ) {
        if (!(texts.size() && rtv && gui_cs.try_begin_cs())) {
            time = 0;
            return;
        }

        ImGui::SetCurrentContext(imgui_context);
        ImGui_ImplDX9_NewFrame();

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

        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(0, 0));
        ImGui::Begin(
            "Overlay",
            NULL,
            ImGuiWindowFlags_NoTitleBar
        );
        while (texts.size() && texts.front().time && time - texts.front().time > ticks_per_second * TEXT_DURATION) {
            texts.pop_front();
        }
        for (Text& text : texts) {
            if (!text.time) text.time = time;
            ImGui::TextUnformatted(text.text.c_str());
        }
        ImGui::End();
        ImGui::Render();
        pDevice->get_inner()->SetRenderTarget(0, rtv);
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

        gui_cs.end_cs();
    }
};

Overlay::Overlay() : impl(new Impl()) {}

Overlay::~Overlay() {
    delete impl;
}

void Overlay::set_display(
    D3DPRESENT_PARAMETERS* pSwapChainDesc,
    MyIDXGISwapChain* pSwapChain,
    MyID3D9Device* pDevice
) {
    impl->begin_text();
    impl->set_display(pSwapChainDesc, pSwapChain, pDevice);
    impl->end_text();
}

HRESULT Overlay::present(
    UINT SyncInterval,
    UINT Flags
) {
    impl->begin_text();
    impl->present(SyncInterval, Flags);
    impl->end_text();
    return impl->pSwapChain->get_inner()->Present(NULL, NULL, NULL, NULL, Flags);
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
