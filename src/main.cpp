// stl headers
#include <memory>
#include <system_error>
#include <string>
#include <vector>
#include <cmath>

// Win32 headers
// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN             
// ReSharper disable once IdentifierTypo
#define NOMINMAX
#include <Windows.h>
#include <atlbase.h>

// D3D11 headers
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "d3d11.lib")


void DebugLogFormat(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    char szBuffer[512];
    _vsnprintf_s(szBuffer, 511, format, args);

    OutputDebugStringA(szBuffer);

    va_end(args);
}

#if defined (DEBUG) || defined(_DEBUG)
#define DEBUG_LOG_FORMAT(format, ...) DebugLogFormat(format, __VA_ARGS__)
#else
#define DEBUG_LOG_FORMAT(format, ...)
#endif

struct VertexData
{
    DirectX::XMFLOAT3 m_Position;
    DirectX::XMFLOAT3 m_Color;
};


struct PerFrameConstantBufferData
{
    DirectX::XMMATRIX m_MvpMatrix;
};

// Create cube geometry.
constexpr VertexData g_GeomVerts[] =
{
    {DirectX::XMFLOAT3(-0.5f,-0.5f,-0.5f), DirectX::XMFLOAT3(  0,   0,   0),},
    {DirectX::XMFLOAT3(-0.5f,-0.5f, 0.5f), DirectX::XMFLOAT3(  0,   0,   1),},
    {DirectX::XMFLOAT3(-0.5f, 0.5f,-0.5f), DirectX::XMFLOAT3(  0,   1,   0),},
    {DirectX::XMFLOAT3(-0.5f, 0.5f, 0.5f), DirectX::XMFLOAT3(  0,   1,   1),},

    {DirectX::XMFLOAT3( 0.5f,-0.5f,-0.5f), DirectX::XMFLOAT3(  1,   0,   0),},
    {DirectX::XMFLOAT3( 0.5f,-0.5f, 0.5f), DirectX::XMFLOAT3(  1,   0,   1),},
    {DirectX::XMFLOAT3( 0.5f, 0.5f,-0.5f), DirectX::XMFLOAT3(  1,   1,   0),},
    {DirectX::XMFLOAT3( 0.5f, 0.5f, 0.5f), DirectX::XMFLOAT3(  1,   1,   1),},
};
    
constexpr uint16_t g_GeomIndices [] = 
{
    0,2,1, // -x
    1,2,3,

    4,5,6, // +x
    5,7,6,

    0,1,5, // -y
    0,5,4,

    2,6,7, // +y
    2,7,3,

    0,4,6, // -z
    0,6,2,

    1,3,7, // +z
    1,7,5,
};

// Globals
HWND          g_SampleWindow;
const TCHAR*  g_SampleWindowName = L"D3D11SimpleMeshRender";
const TCHAR*  g_SampleWindowClassName = L"D3D11SimpleMeshRenderClass";
constexpr int g_SampleWindowWidth = 1024;
constexpr int g_SampleWindowHeight = 768;

CComPtr<IDXGISwapChain>         g_SwapChain;
CComPtr<ID3D11Device>           g_Device;
CComPtr<ID3D11DeviceContext>    g_DeviceContext;
CComPtr<ID3D11Texture2D>        g_BackBuffer;
CComPtr<ID3D11RenderTargetView> g_BackBufferView;

CComPtr<ID3D11VertexShader> g_VS;
CComPtr<ID3D11PixelShader>  g_PS;
CComPtr<ID3D11Buffer>       g_SampleGeometryVertexBuffer;
CComPtr<ID3D11Buffer>       g_SampleGeometryIndexBuffer;
CComPtr<ID3D11InputLayout>  g_VertexLayout;
CComPtr<ID3D11Buffer>       g_PerFrameConstantBuffer;
//DirectX::XMMATRIX           g_ViewMatrix;
DirectX::XMMATRIX           g_ProjectionMatrix;

DirectX::XMFLOAT4 g_ClearColor = {0, 0, 1, 1};

std::string GetHResultMessage(HRESULT hr)
{
    return std::system_category().message(hr);
}

HRESULT CreateDevice()
{
    HRESULT hr = S_OK;

    UINT deviceFlags = 0;

#if defined(DEBUG) || defined(_DEBUG)
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        deviceFlags,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &g_Device,
        nullptr,
        &g_DeviceContext
    );

    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create d3d11 device. %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    return hr;
}

HRESULT CreateSwapChain()
{
    HRESULT hr = S_OK;

    CComPtr<IDXGIDevice> dxgiDevice;
    hr = g_Device.QueryInterface(&dxgiDevice);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to query dxgi device interface. %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    CComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to get dxgi device adapter. %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    CComPtr<IDXGIFactory> factory;
    hr = adapter->GetParent(IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to get dxgi adapter parent. %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    RECT rc;
    GetClientRect(g_SampleWindow, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;


    DXGI_SWAP_CHAIN_DESC desc{};
    desc.Windowed = true;
    desc.BufferCount = 2;
    desc.BufferDesc.Width = width;
    desc.BufferDesc.Height = height;
    desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.OutputWindow = g_SampleWindow;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    hr = factory->CreateSwapChain(g_Device, &desc, &g_SwapChain);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create swap chain. %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    return hr;
}

HRESULT SetUpBackBuffer()
{
    g_BackBuffer.Release();
    g_BackBufferView.Release();
    
    HRESULT hr = g_SwapChain->GetBuffer(0, IID_PPV_ARGS(&g_BackBuffer));
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Couldn't obtain back buffer", GetHResultMessage(hr).c_str());
        return hr;
    }
    
    hr = g_Device->CreateRenderTargetView(g_BackBuffer, nullptr, &g_BackBufferView);

    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Couldn't create back buffer view", GetHResultMessage(hr).c_str());
        return hr;
    }

    return hr;
}

void SetUpViewport()
{
    D3D11_TEXTURE2D_DESC backBufferDesc;
    g_BackBuffer->GetDesc(&backBufferDesc);
    
    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(backBufferDesc.Width);
    viewport.Height = static_cast<float>(backBufferDesc.Height);

    g_DeviceContext->RSSetViewports(1, &viewport);
}

void CalculateProjectionMatrix()
{
    
    D3D11_TEXTURE2D_DESC desc;
    g_BackBuffer->GetDesc(&desc);
    float aspectRatio = static_cast<float>(desc.Width) / static_cast<float>( desc.Height );

    g_ProjectionMatrix = DirectX::XMMatrixPerspectiveFovRH(
        2.0f * std::atan(std::tan(DirectX::XMConvertToRadians(70) * 0.5f) / aspectRatio),
        aspectRatio, 0.01f, 100.0f);
                
    //g_ProjectionMatrix = DirectX::XMMatrixIdentity();
}

/*void CreateViewMatrix()
{
    /*
    DirectX::XMVECTOR eye = DirectX::XMVectorSet(0.0f, 0.7f, 1.5f, 0.f);
    DirectX::XMVECTOR at  = DirectX::XMVectorSet(0.0f,-0.1f, 0.0f, 0.f);
    DirectX::XMVECTOR up  = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.f);

    DirectX::XMMATRIX view = XMMatrixTranspose(DirectX::XMMatrixLookAtRH(eye,at,up));
    #1#
    
    g_ViewMatrix = DirectX::XMMatrixIdentity();
}*/

HRESULT InitSample()
{
    // load and compile shaders
    CComPtr<ID3DBlob> vsc, psc;
    HRESULT           hr = S_OK;

    hr = D3DCompileFromFile(L"Data/Shaders/shader.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vsc, nullptr);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed compile vertex shader.", GetHResultMessage(hr).c_str());
        return hr;
    }

    hr = D3DCompileFromFile(L"Data/Shaders/shader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &psc, nullptr);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed compile pixel shader.", GetHResultMessage(hr).c_str());
        return hr;
    }

    hr = g_Device->CreateVertexShader(vsc->GetBufferPointer(), vsc->GetBufferSize(), nullptr, &g_VS);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to vreate vertex shader.", GetHResultMessage(hr).c_str());
        return hr;
    }

    hr = g_Device->CreatePixelShader(psc->GetBufferPointer(), psc->GetBufferSize(), nullptr, &g_PS);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create pixel shader.", GetHResultMessage(hr).c_str());
        return hr;
    }

    CD3D11_BUFFER_DESC perFrameBufferDesc ={};
    perFrameBufferDesc.ByteWidth = sizeof(PerFrameConstantBufferData);
    perFrameBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    
    hr = g_Device->CreateBuffer(&perFrameBufferDesc,nullptr, &g_PerFrameConstantBuffer);

    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create per frame constant buffer.", GetHResultMessage(hr).c_str());
        return hr;
    }
    
    // Create geom vertex buffer
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC; // write access access by CPU and GPU
    bd.ByteWidth = ARRAYSIZE(g_GeomVerts) * sizeof(VertexData);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; // use as a vertex buffer
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // allow CPU to write in buffer

    hr = g_Device->CreateBuffer(&bd, nullptr, &g_SampleGeometryVertexBuffer);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create vertex buffer.", GetHResultMessage(hr).c_str());
        return hr;
    }

    // store verts in the vertex buffer
    D3D11_MAPPED_SUBRESOURCE ms;
    hr = g_DeviceContext->Map(g_SampleGeometryVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to map vertex buffer.", GetHResultMessage(hr).c_str());
        return hr;
    }

    memcpy(ms.pData, g_GeomVerts, sizeof(g_GeomVerts)); // copy the data
    g_DeviceContext->Unmap(g_SampleGeometryVertexBuffer, NULL); // unmap           


    // Create index buffer
    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DYNAMIC; // write access access by CPU and GPU
    ibd.ByteWidth = ARRAYSIZE(g_GeomIndices) * sizeof(uint16_t);
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER; // use as a vertex buffer
    ibd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // allow CPU to write in buffer

    hr = g_Device->CreateBuffer(&ibd, nullptr, &g_SampleGeometryIndexBuffer);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create index buffer.", GetHResultMessage(hr).c_str());
        return hr;
    }

    // store indices in the buffer
    D3D11_MAPPED_SUBRESOURCE indicesRes;
    hr = g_DeviceContext->Map(g_SampleGeometryIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, NULL, &indicesRes);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to map index buffer.", GetHResultMessage(hr).c_str());
        return hr;
    }

    memcpy(indicesRes.pData, g_GeomIndices, sizeof(g_GeomIndices)); // copy the data
    g_DeviceContext->Unmap(g_SampleGeometryIndexBuffer, NULL); // unmap           
    
    // Create vertex layout. This is somewhat unrelated to geometry
    D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(DirectX::XMFLOAT3), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    
    hr = g_Device->CreateInputLayout(vertexLayoutDesc, ARRAYSIZE(vertexLayoutDesc),
                                     vsc->GetBufferPointer(), vsc->GetBufferSize(), &g_VertexLayout);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create vertex layout.", GetHResultMessage(hr).c_str());
        return hr;
    }

    return hr;
}

void TickSample()
{
    ID3D11DeviceContext* ctx = g_DeviceContext;

    static DirectX::XMVECTOR eye = DirectX::XMVectorSet(0.0f, 0.7f, 3.5f, 0.f);
    const DirectX::XMVECTOR at  = DirectX::XMVectorSet(0.0f,0, 0.0f, 0.f);
    const DirectX::XMVECTOR up  = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.f);
    
    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtRH(eye,at,up);
    
    PerFrameConstantBufferData cb0 = 
    {
        XMMatrixTranspose( DirectX::XMMatrixTranslation(0, 0, 0) * view *  g_ProjectionMatrix ),
    };
    
    ctx->UpdateSubresource( g_PerFrameConstantBuffer, 0, nullptr, &cb0, 0, 0 );
    
    ctx->ClearRenderTargetView(g_BackBufferView, reinterpret_cast<FLOAT*>(&g_ClearColor));

    ctx->OMSetRenderTargets(1, &g_BackBufferView.p, nullptr);

    constexpr UINT vertexBufferStride = sizeof(VertexData);
    constexpr UINT vertexBufferOffset = 0;
    ctx->IASetVertexBuffers(0, 1, &g_SampleGeometryVertexBuffer.p, &vertexBufferStride, &vertexBufferOffset);
    
    ctx->VSSetShader(g_VS, nullptr, 0);
    
    ctx->VSSetConstantBuffers(0, 1, &g_PerFrameConstantBuffer.p);
    
    ctx->PSSetShader(g_PS, nullptr, 0);

    ctx->IASetIndexBuffer(g_SampleGeometryIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->IASetInputLayout(g_VertexLayout);

    ctx->DrawIndexed(ARRAYSIZE(g_GeomIndices), 0, 0);
    
    g_SwapChain->Present(0, 0);
}

LRESULT CALLBACK MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        {
            //const UINT width = LOWORD(lParam);
            //const UINT height = HIWORD(lParam);

            g_DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
            g_DeviceContext->Flush();
            
            g_BackBuffer.Release();
            g_BackBufferView.Release();
            
            g_SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
            HRESULT hr = SetUpBackBuffer();
            
            CalculateProjectionMatrix();

            if (FAILED(hr))
            {
                DEBUG_LOG_FORMAT("Failed to set up back buffer during resize event. ", GetHResultMessage(hr).c_str());
            }
            else
            {
                SetUpViewport();
            }
        }
        break;
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
    wcx.lpfnWndProc = MsgProc;
    wcx.hInstance = hInstance;
    wcx.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcx.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
    wcx.lpszClassName = g_SampleWindowClassName;

    RegisterClassExW(&wcx);

    RECT rc = {0, 0, g_SampleWindowWidth, g_SampleWindowHeight};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, false);

    g_SampleWindow = CreateWindowEx(
        0,
        g_SampleWindowClassName,
        g_SampleWindowName,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
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

    HRESULT hr = CreateDevice();
    if (FAILED(hr))
        return hr;

    InitSample();

    CreateSampleWindow(hInstance);

    if (!g_SampleWindow)
        return 0;

    hr = CreateSwapChain();
    if (FAILED(hr))
        return hr;

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
        else { TickSample(); }
    }

    g_SwapChain->SetFullscreenState(false, nullptr);

    return static_cast<int>(msg.wParam);
}
