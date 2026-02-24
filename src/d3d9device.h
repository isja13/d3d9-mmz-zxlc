#ifndef D3D9DEVICE_H
#define D3D9DEVICE_H

#include <d3d9.h>
#include "main.h"
#include "unknown.h"
#include "unknown_impl.h"
#include "d3d9texture1d.h"
#include "d3d9texture2d.h"
#include "d3d9pixelshader.h"
#include "d3d9vertexshader.h"
#include "d3d9samplerstate.h"
#include "d3d9depthstencilstate.h"
#include "custom_query_type.h"
#include "conf.h"
#include "overlay.h"

#define MAX_FVF_DECL_SIZE 64
#define D3D9_SIMULTANEOUS_RENDER_TARGET_COUNT 4

struct D3D9_BLEND_DESC {
    struct RenderTargetBlendDesc {
        BOOL BlendEnable;
        D3DBLEND SrcBlend;
        D3DBLEND DestBlend;
        D3DBLENDOP BlendOp;
        D3DBLEND SrcBlendAlpha;
        D3DBLEND DestBlendAlpha;
        D3DBLENDOP BlendOpAlpha;
        UINT8 RenderTargetWriteMask;
    };
    RenderTargetBlendDesc RenderTarget[D3D9_SIMULTANEOUS_RENDER_TARGET_COUNT];
};

struct D3D9_RASTERIZER_DESC {
    D3DFILLMODE FillMode;
    D3DCULL CullMode;
    float DepthBias;
    float SlopeScaledDepthBias;
    BOOL ScissorEnable;
    BOOL MultisampleEnable;
    BOOL AntialiasedLineEnable;
};

class MyID3D9Device : public IDirect3DDevice9 {
    template<class T> friend struct LogItem;
    class Impl;
    Impl* impl;

public:

    Impl* get_impl() const { return impl; }

    MyID3D9Device(IDirect3DDevice9** inner, UINT width, UINT height);
    MyID3D9Device(IDirect3DDevice9* pOriginal);
    virtual ~MyID3D9Device();

    IUNKNOWN_DECL(IDirect3DDevice9)

        IDirect3DDevice9* get_inner() const;
    

    void set_overlay(Overlay* overlay);
    void set_config(Config* config);

    void resize_buffers(UINT width, UINT height);
    void resize_orig_buffers(UINT width, UINT height);

    void on_pre_reset();
    void on_post_reset(D3DPRESENT_PARAMETERS* pp);

    HRESULT STDMETHODCALLTYPE VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, IDirect3DVertexBuffer9* const* ppConstantBuffers);
    HRESULT STDMETHODCALLTYPE SetDepthStencilState(IDirect3DStateBlock9* pDepthStencilState);
    HRESULT STDMETHODCALLTYPE DrawPrimitiveAuto();
    HRESULT STDMETHODCALLTYPE SetRasterizerState(IDirect3DStateBlock9* pRasterizerState);
    HRESULT STDMETHODCALLTYPE SetViewports(UINT NumViewports, const D3DVIEWPORT9* pViewports);
    HRESULT STDMETHODCALLTYPE SetScissorRects(UINT NumRects, const RECT* pRects);
    HRESULT STDMETHODCALLTYPE UpdateSubresource(IDirect3DResource9* pDstResource, UINT DstSubresource, const D3DLOCKED_RECT* pDstBox, const VOID* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);
    void STDMETHODCALLTYPE GenerateMips(IDirect3DBaseTexture9* pTexture);
    void STDMETHODCALLTYPE PSGetShaderResources(UINT StartSlot, UINT NumViews, IDirect3DBaseTexture9** ppShaderResourceViews);
    void STDMETHODCALLTYPE PSGetShader(IDirect3DPixelShader9** ppPixelShader);
    void STDMETHODCALLTYPE VSGetShader(IDirect3DVertexShader9** ppVertexShader);
    void STDMETHODCALLTYPE PSGetSamplers(UINT StartSlot, UINT NumSamplers, DWORD* ppSamplers);
    void STDMETHODCALLTYPE IAGetInputLayout(IDirect3DVertexDeclaration9** ppInputLayout);
    void STDMETHODCALLTYPE IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers, IDirect3DVertexBuffer9** ppVertexBuffers, UINT* pStrides, UINT* pOffsets);
    void STDMETHODCALLTYPE IAGetIndexBuffer(IDirect3DIndexBuffer9** pIndexBuffer, D3DFORMAT* Format, UINT* Offset);
    void STDMETHODCALLTYPE OMGetRenderTargets(UINT NumViews, IDirect3DSurface9** ppRenderTargetViews, IDirect3DSurface9** ppDepthStencilView);
    void STDMETHODCALLTYPE RSGetViewports(UINT* NumViewports, D3DVIEWPORT9* pViewports);
    void STDMETHODCALLTYPE RSGetScissorRects(UINT* NumRects, RECT* pRects);
    void STDMETHODCALLTYPE Flush();

    HRESULT STDMETHODCALLTYPE SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture);
    HRESULT STDMETHODCALLTYPE SetPixelShader(IDirect3DPixelShader9* pPixelShader);
    HRESULT STDMETHODCALLTYPE SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value);
    HRESULT STDMETHODCALLTYPE SetVertexShader(IDirect3DVertexShader9* pVertexShader);
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount);
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantF(UINT StartRegister, const float* pConstantData, UINT Vector4fCount);
    HRESULT STDMETHODCALLTYPE SetFVF(DWORD FVF);
    HRESULT STDMETHODCALLTYPE SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride);
    HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DIndexBuffer9* pIndexData);
    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount);
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantF(UINT StartRegister, const float* pConstantData, UINT Vector4fCount);
    HRESULT STDMETHODCALLTYPE SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget);
    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE State, DWORD Value);
    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle);


    HRESULT STDMETHODCALLTYPE CreateTexture1D(UINT Width, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle);
    HRESULT STDMETHODCALLTYPE CreateTexture2D(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle = nullptr);
    HRESULT STDMETHODCALLTYPE CreateShaderResourceView(IDirect3DTexture9* pTexture, IDirect3DTexture9** ppSRView);
    HRESULT STDMETHODCALLTYPE CreateRenderTargetView(IDirect3DSurface9* pSurface, const D3DSURFACE_DESC* pDesc, IDirect3DSurface9** ppRTView);
    HRESULT STDMETHODCALLTYPE CreateDepthStencilView(IDirect3DSurface9* pSurface, const D3DSURFACE_DESC* pDesc, IDirect3DSurface9** ppDepthStencilView);
    HRESULT STDMETHODCALLTYPE CreateInputLayout(const D3DVERTEXELEMENT9* pInputElementDescs, UINT NumElements, const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, IDirect3DVertexDeclaration9** ppVertexDeclaration);
    HRESULT STDMETHODCALLTYPE CreateVertexShader(const void* pShaderBytecode, SIZE_T BytecodeLength, IDirect3DVertexShader9** ppVertexShader);
    HRESULT STDMETHODCALLTYPE CreateGeometryShader(const void* pShaderBytecode, SIZE_T BytecodeLength, void** ppGeometryShader);
    HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(const void* pShaderBytecode, SIZE_T BytecodeLength, const void* pSODeclaration, UINT NumEntries, UINT OutputStreamStride, void** ppGeometryShader);
    HRESULT STDMETHODCALLTYPE CreateBlendState(const D3D9_BLEND_DESC* pBlendStateDesc, IDirect3DStateBlock9** ppBlendState);
    HRESULT STDMETHODCALLTYPE CreateDepthStencilState(const D3D9_DEPTH_STENCIL_DESC* pDepthStencilDesc, IDirect3DStateBlock9** ppDepthStencilState);
    HRESULT STDMETHODCALLTYPE CreateRasterizerState(const D3D9_RASTERIZER_DESC* pRasterizerDesc, IDirect3DStateBlock9** ppRasterizerState);
    HRESULT STDMETHODCALLTYPE CreateSamplerState(const D3DSAMPLER_DESC* pSamplerDesc, IDirect3DStateBlock9** ppSamplerState);
    HRESULT STDMETHODCALLTYPE CreateQuery_Custom(const CustomQueryDesc* pQueryDesc, IDirect3DQuery9** ppQuery);
    HRESULT STDMETHODCALLTYPE CreatePredicate(const CustomQueryDesc* pPredicateDesc, IDirect3DQuery9** ppPredicate);
    HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(D3DFORMAT Format, DWORD SampleCount, DWORD* pNumQualityLevels);

    HRESULT STDMETHODCALLTYPE TestCooperativeLevel();
    UINT STDMETHODCALLTYPE GetAvailableTextureMem();
    HRESULT STDMETHODCALLTYPE EvictManagedResources();
    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D9** ppD3D9);
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS9* pCaps);
    HRESULT STDMETHODCALLTYPE GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode);
    HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters);
    HRESULT STDMETHODCALLTYPE SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap);
    void STDMETHODCALLTYPE SetCursorPosition(int X, int Y, DWORD Flags);
    BOOL STDMETHODCALLTYPE ShowCursor(BOOL bShow);
    HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** pSwapChain);
    HRESULT STDMETHODCALLTYPE GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain);
    UINT STDMETHODCALLTYPE GetNumberOfSwapChains();
    HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS* pPresentationParameters);
    HRESULT STDMETHODCALLTYPE Present(const RECT* src_rect, const RECT* dst_rect, HWND dst_window_override, const RGNDATA* dirty_region);
    HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer);
    HRESULT STDMETHODCALLTYPE GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus);
    HRESULT STDMETHODCALLTYPE SetDialogBoxMode(BOOL bEnableDialogs);
    void STDMETHODCALLTYPE SetGammaRamp(UINT iSwapChain, DWORD flags, const D3DGAMMARAMP* ramp);
    void STDMETHODCALLTYPE GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp);
    HRESULT STDMETHODCALLTYPE CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle);
    HRESULT STDMETHODCALLTYPE CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle);
    HRESULT STDMETHODCALLTYPE CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle);
    HRESULT STDMETHODCALLTYPE CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle);
    HRESULT STDMETHODCALLTYPE CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle);
    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle);
    HRESULT STDMETHODCALLTYPE UpdateSurface(IDirect3DSurface9* src_surface, const RECT* src_rect, IDirect3DSurface9* dest_surface, const POINT* dest_point);
    HRESULT STDMETHODCALLTYPE UpdateTexture(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture);
    HRESULT STDMETHODCALLTYPE GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface);
    HRESULT STDMETHODCALLTYPE GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface);
    HRESULT STDMETHODCALLTYPE StretchRect(IDirect3DSurface9* src_surface, const RECT* src_rect, IDirect3DSurface9* dest_surface, const RECT* dest_rect, D3DTEXTUREFILTERTYPE filter);
    HRESULT STDMETHODCALLTYPE ColorFill(IDirect3DSurface9* surface, const RECT* rect, D3DCOLOR color);
    HRESULT STDMETHODCALLTYPE CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle);
    HRESULT STDMETHODCALLTYPE GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget);
    HRESULT STDMETHODCALLTYPE SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil);
    HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface);
    HRESULT STDMETHODCALLTYPE BeginScene();
    HRESULT STDMETHODCALLTYPE EndScene();
    HRESULT STDMETHODCALLTYPE Clear(DWORD rect_count, const D3DRECT* rects, DWORD flags, D3DCOLOR color, float z, DWORD stencil);
    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix);
    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix);
    HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix);
    HRESULT STDMETHODCALLTYPE SetViewport(const D3DVIEWPORT9* viewport);
    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT9* pViewport);
    HRESULT STDMETHODCALLTYPE SetMaterial(const D3DMATERIAL9* material);
    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL9* pMaterial);
    HRESULT STDMETHODCALLTYPE SetLight(DWORD index, const D3DLIGHT9* light);
    HRESULT STDMETHODCALLTYPE GetLight(DWORD Index, D3DLIGHT9* pLight);
    HRESULT STDMETHODCALLTYPE LightEnable(DWORD Index, BOOL Enable);
    HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD Index, BOOL* pEnable);
    HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD index, const float* plane);
    HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD Index, float* pPlane);
    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue);
    HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB);
    HRESULT STDMETHODCALLTYPE BeginStateBlock();
    HRESULT STDMETHODCALLTYPE EndStateBlock(IDirect3DStateBlock9** ppSB);
    HRESULT STDMETHODCALLTYPE SetClipStatus(const D3DCLIPSTATUS9* clip_status);
    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS9* pClipStatus);
    HRESULT STDMETHODCALLTYPE GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture);
    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue);
    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value);
    HRESULT STDMETHODCALLTYPE GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue);
    HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD* pNumPasses);
    HRESULT STDMETHODCALLTYPE SetPaletteEntries(UINT palette_idx, const PALETTEENTRY* entries);
    HRESULT STDMETHODCALLTYPE GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries);
    HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette(UINT PaletteNumber);
    HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette(UINT* PaletteNumber);
    HRESULT STDMETHODCALLTYPE SetScissorRect(const RECT* rect);
    HRESULT STDMETHODCALLTYPE GetScissorRect(RECT* pRect);
    HRESULT STDMETHODCALLTYPE SetSoftwareVertexProcessing(BOOL bSoftware);
    BOOL STDMETHODCALLTYPE GetSoftwareVertexProcessing();
    HRESULT STDMETHODCALLTYPE SetNPatchMode(float nSegments);
    float STDMETHODCALLTYPE GetNPatchMode();
    HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(D3DPRIMITIVETYPE primitive_type, UINT primitive_count, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride);
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE primitive_type, UINT min_vertex_idx, UINT num_vertices, UINT primitive_count, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride);
    HRESULT STDMETHODCALLTYPE ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags);
    HRESULT STDMETHODCALLTYPE CreateVertexDeclaration(const D3DVERTEXELEMENT9* elements, IDirect3DVertexDeclaration9** ppDecl);
    HRESULT STDMETHODCALLTYPE SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl);
    HRESULT STDMETHODCALLTYPE GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl);
    HRESULT STDMETHODCALLTYPE GetFVF(DWORD* pFVF);
    HRESULT STDMETHODCALLTYPE CreateVertexShader(const DWORD* byte_code, IDirect3DVertexShader9** shader);
    HRESULT STDMETHODCALLTYPE GetVertexShader(IDirect3DVertexShader9** ppShader);
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount);
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantI(UINT reg_idx, const int* data, UINT count);
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount);
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantB(UINT reg_idx, const BOOL* data, UINT count);
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount);
    HRESULT STDMETHODCALLTYPE GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* OffsetInBytes, UINT* pStride);
    HRESULT STDMETHODCALLTYPE SetStreamSourceFreq(UINT StreamNumber, UINT Divider);
    HRESULT STDMETHODCALLTYPE GetStreamSourceFreq(UINT StreamNumber, UINT* Divider);
    HRESULT STDMETHODCALLTYPE GetIndices(IDirect3DIndexBuffer9** ppIndexData);
    HRESULT STDMETHODCALLTYPE CreatePixelShader(const DWORD* byte_code, IDirect3DPixelShader9** shader);
    HRESULT STDMETHODCALLTYPE GetPixelShader(IDirect3DPixelShader9** ppShader);
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount);
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantI(UINT reg_idx, const int* data, UINT count);
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount);
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantB(UINT reg_idx, const BOOL* data, UINT count);
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount);
    HRESULT STDMETHODCALLTYPE DrawRectPatch(UINT handle, const float* segment_count, const D3DRECTPATCH_INFO* patch_info);
    HRESULT STDMETHODCALLTYPE DrawTriPatch(UINT handle, const float* segment_count, const D3DTRIPATCH_INFO* patch_info);
    HRESULT STDMETHODCALLTYPE DeletePatch(UINT Handle);
    HRESULT STDMETHODCALLTYPE CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery);

    IDirect3DDevice9* m_pD3DDevice; // Existing member
    IDirect3D9* m_pD3D; // New member for the IDirect3D9 interface
};

#endif // D3D9DEVICE_H
