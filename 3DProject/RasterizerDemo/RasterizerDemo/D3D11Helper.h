#pragma once
#include <d3d11.h>
#include <dxgi.h>

// Skapa device + context + swapchain
bool CreateInterfaces(ID3D11Device*& device,
    ID3D11DeviceContext*& immediateContext,
    IDXGISwapChain*& swapChain,
    UINT width,
    UINT height,
    HWND window);

// Skapa RTV
bool CreateRenderTargetView(ID3D11Device* device,
    IDXGISwapChain* swapChain,
    ID3D11RenderTargetView*& rtv);

// Valfri helper om du vill skapa andra depth-resurser manuellt
bool CreateDepthStencil(ID3D11Device* device,
    UINT width,
    UINT height,
    ID3D11Texture2D*& dsTexture,
    ID3D11DepthStencilView*& dsView);

// Viewport
void SetViewport(D3D11_VIEWPORT& viewport,
    UINT width,
    UINT height);

// “Huvud”-setup som du anropar i main
bool SetupD3D11(UINT width,
    UINT height,
    HWND window,
    ID3D11Device*& device,
    ID3D11DeviceContext*& immediateContext,
    IDXGISwapChain*& swapChain,
    ID3D11RenderTargetView*& rtv,
    D3D11_VIEWPORT& viewport);
