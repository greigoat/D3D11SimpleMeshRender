// C RunTime Header Files
#include <cstdlib>
#include <tchar.h>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#define HRF_RETURN(hr) if (FAILED(hr)) return hr

#include <memory>
#include <string>
#include <Windows.h>

#include <atlbase.h>
#include <comdef.h>

#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "d3d11.lib")

struct Color4f
{
    float r, g, b, a;
};

struct Vector3f
{
    float x, y, z;
};

struct VertexData
{
    Vector3f position;
    Color4f  color;
};

// Globals
HWND          g_SampleWindow;
const TCHAR*  g_SampleWindowName = L"D3D11SimpleMeshRender";
const TCHAR*  g_SampleWindowClassName = L"D3D11SimpleMeshRenderClass";
constexpr int g_SampleWindowWidth = 1024;
constexpr int g_SampleWindowHeight = 768;

CComPtr<ID3D11Buffer>           g_SampleGeometryVertexBuffer;
CComPtr<IDXGISwapChain>         g_SwapChain;
CComPtr<ID3D11Device>           g_Device;
CComPtr<ID3D11DeviceContext>    g_DeviceContext;
CComPtr<ID3D11RenderTargetView> g_Backbuffer;
CComPtr<ID3D11VertexShader>     g_VS;
CComPtr<ID3D11PixelShader>      g_PS;

Color4f g_ClearColor = {0, 0, 1, 1};

VertexData g_SampleGeometryVertices[] =
{
    {
        {0.0f, 0.5f, 0.0f},
        {1.0f, 0.0f, 0.0f, 1.0f}
    },

    {
        {0.45f, -0.5, 0.0f},
        {0.0f, 1.0f, 0.0f, 1.0f}
    },

    {
        {-0.45f, -0.5f, 0.0f},
        {0.0f, 0.0f, 1.0f, 1.0f}
    }
};

HRESULT SetupD3D11()
{
    UINT creationFlags = 0;

#if defined(_DEBUG)
    // If the project is in a debug build, enable the debug layer.
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 1;
    swapChainDesc.OutputWindow = g_SampleWindow;
    swapChainDesc.BufferDesc.Width = g_SampleWindowWidth;
    swapChainDesc.BufferDesc.Height = g_SampleWindowHeight;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 32 bit rgba
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SampleDesc.Count = 4;
    swapChainDesc.Windowed = true;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    HRF_RETURN(D3D11CreateDeviceAndSwapChain(nullptr, // default gpu
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &swapChainDesc,
        &g_SwapChain,
        &g_Device,
        nullptr,
        &g_DeviceContext));

    CComPtr<ID3D11Texture2D> backbuffer;
    HRF_RETURN(g_SwapChain->GetBuffer(0, IID_PPV_ARGS(&backbuffer)));
    HRF_RETURN(g_Device->CreateRenderTargetView(backbuffer, nullptr, &g_Backbuffer));
    g_DeviceContext->OMSetRenderTargets(1, &g_Backbuffer.p, nullptr);

    // setup vp
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = g_SampleWindowWidth;
    viewport.Height = g_SampleWindowHeight;

    g_DeviceContext->RSSetViewports(1, &viewport);

    // compile sample shader
    CComPtr<ID3DBlob> vsc, psc;

    HRF_RETURN(
        D3DCompileFromFile(L"Data/Shaders/shader.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vsc, nullptr));
    HRF_RETURN(
        D3DCompileFromFile(L"Data/Shaders/shader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &psc, nullptr));

    HRF_RETURN(g_Device->CreateVertexShader(vsc->GetBufferPointer(), vsc->GetBufferSize(), nullptr, &g_VS));
    HRF_RETURN(g_Device->CreatePixelShader(psc->GetBufferPointer(), psc->GetBufferSize(), nullptr, &g_PS));

    g_DeviceContext->VSSetShader(g_VS, nullptr, 0);
    g_DeviceContext->PSSetShader(g_PS, nullptr, 0);

    // Create gpu vertex buffer for our geometry
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC; // write access access by CPU and GPU
    bd.ByteWidth = ARRAYSIZE(g_SampleGeometryVertices) * sizeof(VertexData);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; // use as a vertex buffer
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // allow CPU to write in buffer

    g_Device->CreateBuffer(&bd, nullptr, &g_SampleGeometryVertexBuffer);

    // map buffer to our data
    D3D11_MAPPED_SUBRESOURCE ms;
    g_DeviceContext->Map(g_SampleGeometryVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
    memcpy(ms.pData, g_SampleGeometryVertices, sizeof(g_SampleGeometryVertices)); // copy the data
    g_DeviceContext->Unmap(g_SampleGeometryVertexBuffer, NULL); // unmap           

    // Create input layout 
    D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, sizeof(Vector3f), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    CComPtr<ID3D11InputLayout> vertexLayout;
    g_Device->CreateInputLayout(vertexLayoutDesc, ARRAYSIZE(vertexLayoutDesc),
                                vsc->GetBufferPointer(), vsc->GetBufferSize(), &vertexLayout);

    g_DeviceContext->IASetInputLayout(vertexLayout);

    return S_OK;
}

void AppLoop()
{
    const float clearColor[4] = {g_ClearColor.r, g_ClearColor.g, g_ClearColor.b, g_ClearColor.a};
    g_DeviceContext->ClearRenderTargetView(g_Backbuffer, clearColor);

    constexpr UINT stride = sizeof(VertexData);
    constexpr UINT offset = 0;
    g_DeviceContext->IASetVertexBuffers(0, 1, &g_SampleGeometryVertexBuffer.p, &stride, &offset);
    g_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    g_DeviceContext->Draw(ARRAYSIZE(g_SampleGeometryVertices), 0);

    g_SwapChain->Present(0, 0);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void CreateSampleWindow(HINSTANCE hInstance)
{
    WNDCLASSEXW wcx = {};
    wcx.cbSize = sizeof(WNDCLASSEX);
    wcx.style = CS_HREDRAW | CS_VREDRAW;
    wcx.lpfnWndProc = WndProc;
    wcx.hInstance = hInstance;
    wcx.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcx.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
    wcx.lpszClassName = g_SampleWindowClassName;

    RegisterClassExW(&wcx);

    g_SampleWindow = CreateWindowEx(
        0,
        g_SampleWindowClassName,
        g_SampleWindowName,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        g_SampleWindowWidth,
        g_SampleWindowHeight,
        nullptr,
        nullptr,
        hInstance,
        nullptr);
}

int APIENTRY wWinMain(_In_ HINSTANCE     hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR        lpCmdLine,
                      _In_ int           nShowCmd)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    CreateSampleWindow(hInstance);

    if (!g_SampleWindow)
        return 0;

    HRESULT hr = SetupD3D11();
    if (FAILED(hr))
    {
        const _com_error e(hr);
        OutputDebugString(e.ErrorMessage());
        return 0;
    }

    ShowWindow(g_SampleWindow, nShowCmd);
    UpdateWindow(g_SampleWindow);

    MSG msg;
    // Main message loop:
    while (true)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT)
                break;
        }
        else { AppLoop(); }
    }

    g_SwapChain->SetFullscreenState(false, nullptr);

    return static_cast<int>(msg.wParam);
}
