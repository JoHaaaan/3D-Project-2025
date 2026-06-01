#include "D3D11Helper.h"
#include <iostream>

bool CreateInterfaces(ID3D11Device*& device,
    ID3D11DeviceContext*& immediateContext,
    IDXGISwapChain*& swapChain,
    UINT width,
    UINT height,
    HWND window)
{
    UINT flags = 0;
#if _DEBUG
    flags = D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferDesc.Width = width;
    swapChainDesc.BufferDesc.Height = height;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;

    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 1;
    swapChainDesc.OutputWindow = window;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapChainDesc.Flags = 0;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        featureLevels,
        1,
        D3D11_SDK_VERSION,
        &swapChainDesc,
        &swapChain,
        &device,
        nullptr,
        &immediateContext);

    return !FAILED(hr);
}

bool CreateRenderTargetView(ID3D11Device* device,
    IDXGISwapChain* swapChain,
    ID3D11RenderTargetView*& rtv)
{
    ID3D11Texture2D* backBuffer = nullptr;
    if (FAILED(swapChain->GetBuffer(
        0,
        __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(&backBuffer))))
    {
        std::cerr << "Failed to get back buffer!" << std::endl;
        return false;
    }

    HRESULT hr = device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
    backBuffer->Release();

    if (FAILED(hr))
    {
        std::cerr << "Failed to create render target view!" << std::endl;
        return false;
    }

    return true;
}



void SetViewport(D3D11_VIEWPORT& viewport,
    UINT width,
    UINT height)
{
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
}

bool SetupD3D11(UINT width,
    UINT height,
    HWND window,
    ID3D11Device*& device,
    ID3D11DeviceContext*& immediateContext,
    IDXGISwapChain*& swapChain,
    ID3D11RenderTargetView*& rtv,
    D3D11_VIEWPORT& viewport)
{
    if (!CreateInterfaces(device, immediateContext, swapChain, width, height, window))
    {
        std::cerr << "Error creating interfaces!" << std::endl;
        return false;
    }

    if (!CreateRenderTargetView(device, swapChain, rtv))
    {
        std::cerr << "Error creating rtv!" << std::endl;
        return false;
    }

    SetViewport(viewport, width, height);

    return true;
}
