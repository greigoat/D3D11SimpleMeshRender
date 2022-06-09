// Includes
#include <vector>
#include <memory>
#include <system_error>
#include <string>
#include <algorithm>
#include <chrono>

#define WIN32_LEAN_AND_MEAN             
// ReSharper disable once IdentifierTypo
#define NOMINMAX
#include <Windows.h>
#include <windowsx.h>
#include <atlbase.h>
#include <commdlg.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include <fbxsdk.h>

// Defines
#if defined (DEBUG) || defined(_DEBUG)
#define DEBUG_LOG_FORMAT(format, ...) DebugLogFormat(format, __VA_ARGS__)
#else
#define DEBUG_LOG_FORMAT(format, ...)
#endif
#undef near
#undef far

#define IDM_FILE_OPEN 1
#define IDM_FILE_QUIT 2

// Structs
struct DirectionalLightData
{
    DirectX::XMFLOAT4 m_Color;
    DirectX::XMFLOAT3 m_Direction;
    float             m_Attenuation;
};

struct ModelVertex
{
    DirectX::XMFLOAT3 m_Position;
    DirectX::XMFLOAT3 m_Color;
    DirectX::XMFLOAT3 m_Normal;
};

struct GridVertex
{
    DirectX::XMFLOAT3 m_Position;
    DirectX::XMFLOAT3 m_Color;
};

struct FrameConstantBufferData
{
    DirectX::XMMATRIX    m_ModelMatrix;
    DirectX::XMMATRIX    m_ViewMatrix;
    DirectX::XMMATRIX    m_MvpMatrix;
    DirectX::XMFLOAT4    m_WorldSpaceCameraPos;
    DirectionalLightData m_DirectionalLightData;
};

// Globals
HWND         g_MainWindow;
const TCHAR* g_MainWindowTitleName = L"D3D11SimpleMeshRender";
const TCHAR* g_MainWindowClassName = L"D3D11SimpleMeshRenderClass";
int          g_MainWindowWidth = 1024; // 759;
int          g_MainWindowHeight = 768; // 291;

CComPtr<IDXGISwapChain>          g_SwapChain;
CComPtr<ID3D11Device>            g_Device;
CComPtr<ID3D11DeviceContext>     g_DeviceContext;
CComPtr<ID3D11Texture2D>         g_BackBuffer;
CComPtr<ID3D11RenderTargetView>  g_BackBufferView;
CComPtr<ID3D11Texture2D>         g_DepthStencilBuffer;
CComPtr<ID3D11DepthStencilState> g_DepthStencilState;
CComPtr<ID3D11DepthStencilView>  g_DepthStencilView;
CComPtr<ID3D11RasterizerState>   g_RasterizerState;

FbxSharedDestroyPtr<FbxManager> g_FbxManager;

CComPtr<ID3D11Buffer> g_FrameConstantBuffer;
DirectX::XMMATRIX     g_ProjectionMatrix;
DirectX::XMFLOAT4     g_ClearColor = {0.5f, 0.5f, 0.5f, 1};
DirectionalLightData  g_DirectionalLightData
{
    {1.0f, 1.0f, 1.0f, 1.0f}, // color
    {0, -0.5f, 0.5f}, // direction
    0.8f // attenuation
};

POINTF g_MousePosLastMoveEvent;

float             g_CameraFar = 1000.0f;
float             g_CameraNear = 0.3f;
float             g_CameraFov = 70.0f;
DirectX::XMVECTOR g_CameraPos = DirectX::XMVectorSet(0, 0, 0, 0);
float             g_CameraZoomZ = -2.5f;
float             g_CameraPitch = 20.0f;
float             g_CameraYaw = 0.0f;
float             g_CameraZoomMaxSpeed = 0.25f;
float             g_CameraZoomMaxScrollDelta = 120;
bool              g_CameraOrbitEnabled = false;
float             g_CameraOrbitMaxMouseDelta = 10;
float             g_CameraOrbitMaxSpeed = 5.0f;
bool              g_CameraPanEnabled = false;
float             g_CameraPanMaxMouseDelta = 200.0f;
float             g_CameraPanMaxSpeed = 2.0f;

int                         g_GridWidth = 32;
int                         g_GridLength = 32;
DirectX::XMFLOAT3           g_GridColor = {0.4f, 0.4f, 0.4f};
DirectX::XMFLOAT3           g_GridBoldColor = {0.3f, 0.3f, 0.3f};
const wchar_t*              g_GridShaderFileName = L"Data/Shaders/Color.hlsl";
int                         g_GridVertexCount = (g_GridWidth + g_GridLength + 2) * 2;
CComPtr<ID3D11Buffer>       g_GridVertexBuffer;
CComPtr<ID3D11Buffer>       g_GridIndexBuffer;
CComPtr<ID3D11VertexShader> g_GridVertexShader;
CComPtr<ID3D11PixelShader>  g_GridPixelShader;
CComPtr<ID3D11InputLayout>  g_GridVertexLayout;

std::string                 g_ModelFileName = "Data/Models/Suzanne.fbx";
const wchar_t*              g_ModelShaderFileName = L"Data/Shaders/BlinnPhong.hlsl";
int                         g_ModelVertexCount;
CComPtr<ID3D11VertexShader> g_ModelVertexShader;
CComPtr<ID3D11PixelShader>  g_ModelPixelShader;
CComPtr<ID3D11Buffer>       g_ModelVertexBuffer;
CComPtr<ID3D11Buffer>       g_ModelIndexBuffer;
CComPtr<ID3D11InputLayout>  g_ModelVertexLayout;

// Forward declarations
HRESULT CreateMainWindow(HINSTANCE hInstance);
LRESULT CALLBACK MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

HRESULT CreateD3DDevice();
HRESULT CreateSwapChain();
HRESULT CreateFrameConstantBuffer();

void InitFbxSdk();

HRESULT CreateGrid();
HRESULT CreateModelShadersAndLayout();
HRESULT CreateModel();
void MessageBoxModelLoadError(HWND hWnd, HRESULT hr);

void ProcessFrame();


void DebugLogFormat(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    char        buffer[512];
    std::string fmt = format;
    fmt += "\n";

    _vsnprintf_s(buffer, 511, fmt.c_str(), args);

    OutputDebugStringA(buffer);

    va_end(args);
}

template <typename T>
T Clamp(T val, T min, T max) { return std::min(std::max(val, min), max); }

std::string GetErrorMessage(HRESULT hr) { return std::system_category().message(hr); }

bool DirectoryExists(const char* directory)
{
    char fullDir[4096];
    GetFullPathNameA(directory, 4096, fullDir, nullptr);
    
    DWORD attr = GetFileAttributesA(directory);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return false;

    if (attr & FILE_ATTRIBUTE_DIRECTORY)
        return true;

    return false;
}


int APIENTRY wWinMain(_In_ HINSTANCE     hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR        lpCmdLine,
                      _In_ int           nShowCmd)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    HRESULT hr = CreateD3DDevice();
    if (FAILED(hr))
        return hr;

    hr = CreateFrameConstantBuffer();
    if (FAILED(hr))
        return hr;
    
    hr = CreateGrid();
    if (FAILED(hr))
        return hr;

    hr = CreateModelShadersAndLayout();
    if (FAILED(hr))
        return hr;

    hr = CreateMainWindow(hInstance);
    if (FAILED(hr))
        return hr;

    hr = CreateSwapChain();
    if (FAILED(hr))
        return hr;

    InitFbxSdk();

    ShowWindow(g_MainWindow, nShowCmd);
    UpdateWindow(g_MainWindow);

    if (DirectoryExists("Data/Models/"))
    {
        hr = CreateModel();
        if (FAILED(hr))
        {
            MessageBoxModelLoadError(g_MainWindow, hr);
        }
    }

    MSG msg;
    while (true)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT)
                break;
        }
        else { ProcessFrame(); }
    }

    g_SwapChain->SetFullscreenState(false, nullptr);

    return static_cast<int>(msg.wParam);
}

HRESULT CreateMainWindow(HINSTANCE hInstance)
{
    HRESULT hr = S_OK;
    
    WNDCLASSEXW wcx = {};
    wcx.cbSize = sizeof(WNDCLASSEX);
    wcx.style = CS_HREDRAW | CS_VREDRAW;
    wcx.lpfnWndProc = MsgProc;
    wcx.hInstance = hInstance;
    wcx.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcx.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
    wcx.lpszClassName = g_MainWindowClassName;

    if (!RegisterClassExW(&wcx))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        DEBUG_LOG_FORMAT("Failed register main window class. %s", GetErrorMessage(hr).c_str());
    }
    
    RECT rc = {0, 0, g_MainWindowWidth, g_MainWindowHeight};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, false);

    HMENU menuFile = CreateMenu();
    AppendMenu(menuFile, MF_STRING, IDM_FILE_OPEN, L"&Open");
    AppendMenu(menuFile, MF_SEPARATOR, 0, nullptr);
    AppendMenu(menuFile, MF_STRING, IDM_FILE_QUIT, L"&Quit");

    HMENU menuBar = CreateMenu();
    AppendMenu(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(menuFile), L"&File");
    
    g_MainWindow = CreateWindowEx(
        0,
        g_MainWindowClassName,
        g_MainWindowTitleName,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        nullptr,
        menuBar,
        hInstance,
        nullptr);

    if (!g_MainWindow)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        DEBUG_LOG_FORMAT("Failed to create main window. %s", GetErrorMessage(hr).c_str());
        return hr;
    }
    
    return hr;
}

HRESULT CreateD3DDevice()
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
        DEBUG_LOG_FORMAT("Failed to create d3d11 device. %s", GetErrorMessage(hr).c_str());
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
        DEBUG_LOG_FORMAT("Failed to query dxgi device interface. %s", GetErrorMessage(hr).c_str());
        return hr;
    }

    CComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to get dxgi device adapter. %s", GetErrorMessage(hr).c_str());
        return hr;
    }

    CComPtr<IDXGIFactory> factory;
    hr = adapter->GetParent(IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to get dxgi adapter parent. %s", GetErrorMessage(hr).c_str());
        return hr;
    }

    RECT rc;
    GetClientRect(g_MainWindow, &rc);
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
    desc.OutputWindow = g_MainWindow;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    hr = factory->CreateSwapChain(g_Device, &desc, &g_SwapChain);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create swap chain. %s", GetErrorMessage(hr).c_str());
        return hr;
    }

    return hr;
}

HRESULT SetUpBackBuffer()
{
    g_BackBuffer.Release();
    g_BackBufferView.Release();
    g_DepthStencilBuffer.Release();
    g_DepthStencilState.Release();
    g_DepthStencilView.Release();
    g_RasterizerState.Release();

    HRESULT hr = g_SwapChain->GetBuffer(0, IID_PPV_ARGS(&g_BackBuffer));
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Couldn't obtain back buffer. %s", GetErrorMessage(hr).c_str());
        return hr;
    }

    hr = g_Device->CreateRenderTargetView(g_BackBuffer, nullptr, &g_BackBufferView);

    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Couldn't create back buffer view. %s", GetErrorMessage(hr).c_str());
        return hr;
    }

    D3D11_TEXTURE2D_DESC backBufferDesc;
    g_BackBuffer->GetDesc(&backBufferDesc);

    D3D11_TEXTURE2D_DESC depthStencilBufferDesc;
    depthStencilBufferDesc.Width = backBufferDesc.Width;
    depthStencilBufferDesc.Height = backBufferDesc.Height;
    depthStencilBufferDesc.MipLevels = 1;
    depthStencilBufferDesc.ArraySize = 1;
    depthStencilBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; //  24 bits for the depth, 8 bits for the stencil
    depthStencilBufferDesc.SampleDesc.Count = 1;
    depthStencilBufferDesc.SampleDesc.Quality = 0;
    depthStencilBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    depthStencilBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthStencilBufferDesc.CPUAccessFlags = 0;
    depthStencilBufferDesc.MiscFlags = 0;

    hr = g_Device->CreateTexture2D(&depthStencilBufferDesc, nullptr, &g_DepthStencilBuffer);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Couldn't create depth stencil buffer. %s", GetErrorMessage(hr).c_str());
        return hr;
    }

    D3D11_DEPTH_STENCIL_DESC depthStencilDesc;

    depthStencilDesc.DepthEnable = true;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

    depthStencilDesc.StencilEnable = true;
    depthStencilDesc.StencilReadMask = 0xFF;
    depthStencilDesc.StencilWriteMask = 0xFF;

    depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
    depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
    depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    g_Device->CreateDepthStencilState(&depthStencilDesc, &g_DepthStencilState);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Couldn't create depth stencil state. %s", GetErrorMessage(hr).c_str());
        return hr;
    }

    g_Device->CreateDepthStencilView(g_DepthStencilBuffer, nullptr, &g_DepthStencilView);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Couldn't create depth stencil view. %s", GetErrorMessage(hr).c_str());
        return hr;
    }

    D3D11_RASTERIZER_DESC desc;
    desc.AntialiasedLineEnable = false;
    desc.CullMode = D3D11_CULL_BACK;
    desc.DepthBias = 0;
    desc.DepthBiasClamp = 0.0f;
    desc.FillMode = D3D11_FILL_SOLID;
    desc.FrontCounterClockwise = false;
    desc.MultisampleEnable = false;
    desc.ScissorEnable = FALSE;
    desc.SlopeScaledDepthBias = 0.0f;

    hr = g_Device->CreateRasterizerState(&desc, &g_RasterizerState);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Couldn't create rasterizer state. %s", GetErrorMessage(hr).c_str());
        return hr;
    }

    return hr;
}

HRESULT CreateFrameConstantBuffer()
{
    HRESULT hr = S_OK;
    
    CD3D11_BUFFER_DESC perFrameBufferDesc = {};
    perFrameBufferDesc.ByteWidth = sizeof(FrameConstantBufferData);
    perFrameBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    hr = g_Device->CreateBuffer(&perFrameBufferDesc, nullptr, &g_FrameConstantBuffer);

    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create frame constant buffer. %s", GetErrorMessage(hr).c_str());
        return hr;
    }

    return hr;
}

HRESULT CompileShaders(
    const std::wstring& fileName,
    ID3D11VertexShader** outVS,
    ID3D11PixelShader** outPS,
    ID3DBlob** outVSCode,
    ID3DBlob** outPSCode)
{
    HRESULT           hr = S_OK;
    CComPtr<ID3DBlob> errors;

    UINT shaderCompilerFlags = 0;
#if (defined DEBUG || defined _DEBUG)
    shaderCompilerFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    hr = D3DCompileFromFile(fileName.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0",
                            shaderCompilerFlags, 0,
                            outVSCode, &errors.p);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed compile vertex shader. %s %s", GetErrorMessage(hr).c_str(),
                         errors.p ? static_cast<char*>(errors->GetBufferPointer()) : "");
        return hr;
    }

    hr = g_Device->CreateVertexShader((*outVSCode)->GetBufferPointer(), (*outVSCode)->GetBufferSize(), nullptr, outVS);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create vertex shader. %s", GetErrorMessage(hr).c_str());
        return hr;
    }

    hr = D3DCompileFromFile(fileName.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0",
                            shaderCompilerFlags, 0,
                            outPSCode, &errors.p);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed compile pixel shader. %s %s", GetErrorMessage(hr).c_str(),
                         errors.p ? static_cast<char*>(errors->GetBufferPointer()) : "");
        return hr;
    }

    hr = g_Device->CreatePixelShader((*outPSCode)->GetBufferPointer(), (*outPSCode)->GetBufferSize(), nullptr, outPS);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create pixel shader. %s", GetErrorMessage(hr).c_str());
        return hr;
    }

    return hr;
}

template<typename TVertex>
HRESULT CreateVertexBuffer(std::vector<TVertex> vertices, ID3D11Buffer** outBuffer)
{
    HRESULT hr = S_OK;
    
    D3D11_BUFFER_DESC vertexBufferDesc{};
    vertexBufferDesc.ByteWidth = vertices.size() * sizeof(TVertex);
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    const D3D11_SUBRESOURCE_DATA vertexData{vertices.data()};

    hr = g_Device->CreateBuffer(&vertexBufferDesc, &vertexData, outBuffer);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create vertex buffer. %s", GetErrorMessage(hr).c_str());
        return hr;
    }

    return hr;
}

HRESULT CreateIndexBuffer(std::vector<uint32_t> indices, ID3D11Buffer** outBuffer)
{
    HRESULT hr = S_OK;
    
    D3D11_BUFFER_DESC indexBufferDesc{};
    indexBufferDesc.ByteWidth = indices.size() * sizeof(uint32_t);
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    const D3D11_SUBRESOURCE_DATA indexData{indices.data()};

    hr = g_Device->CreateBuffer(&indexBufferDesc, &indexData, outBuffer);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create index buffer. %s", GetErrorMessage(hr).c_str());
        return hr;
    }

    return hr;
}

HRESULT CreateVertexLayoutUsingReflection( ID3DBlob* vsCode, ID3D11InputLayout** outVertexLayout )
{
    HRESULT hr = S_OK;
    
    // Reflect shader info
    CComPtr<ID3D11ShaderReflection> vertexShaderReflection;
    hr = D3DReflect(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), IID_PPV_ARGS(&vertexShaderReflection));
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Call to D3DReflect failed. %s", GetErrorMessage(hr).c_str());
        return hr;
    }

    // Get shader info
    D3D11_SHADER_DESC shaderDesc;
    vertexShaderReflection->GetDesc(&shaderDesc);

    // Read input layout description from shader info
    std::vector<D3D11_INPUT_ELEMENT_DESC> inputLayoutDesc;
    for (uint32_t i = 0; i < shaderDesc.InputParameters; i++)
    {
        D3D11_SIGNATURE_PARAMETER_DESC paramDesc;
        vertexShaderReflection->GetInputParameterDesc(i, &paramDesc);

        // fill out input element desc
        D3D11_INPUT_ELEMENT_DESC elementDesc;
        elementDesc.SemanticName = paramDesc.SemanticName;
        elementDesc.SemanticIndex = paramDesc.SemanticIndex;
        elementDesc.InputSlot = 0;
        elementDesc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        elementDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        elementDesc.InstanceDataStepRate = 0;

        // determine DXGI format
        if (paramDesc.Mask == 1)
        {
            if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) elementDesc.Format = DXGI_FORMAT_R32_UINT;
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
                elementDesc.Format =
                    DXGI_FORMAT_R32_SINT;
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
                elementDesc.Format =
                    DXGI_FORMAT_R32_FLOAT;
        }
        else if (paramDesc.Mask <= 3)
        {
            if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) elementDesc.Format = DXGI_FORMAT_R32G32_UINT;
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
                elementDesc.Format =
                    DXGI_FORMAT_R32G32_SINT;
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
                elementDesc.Format =
                    DXGI_FORMAT_R32G32_FLOAT;
        }
        else if (paramDesc.Mask <= 7)
        {
            if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
                elementDesc.Format =
                    DXGI_FORMAT_R32G32B32_UINT;
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
                elementDesc.Format =
                    DXGI_FORMAT_R32G32B32_SINT;
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
                elementDesc.Format =
                    DXGI_FORMAT_R32G32B32_FLOAT;
        }
        else if (paramDesc.Mask <= 15)
        {
            if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
                elementDesc.Format =
                    DXGI_FORMAT_R32G32B32A32_UINT;
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
                elementDesc.Format =
                    DXGI_FORMAT_R32G32B32A32_SINT;
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
                elementDesc.Format =
                    DXGI_FORMAT_R32G32B32A32_FLOAT;
        }

        //save element desc
        inputLayoutDesc.push_back(elementDesc);
    }

    // Try to create Input Layout
    hr = g_Device->CreateInputLayout(&inputLayoutDesc[0], inputLayoutDesc.size(), vsCode->GetBufferPointer(),
                                           vsCode->GetBufferSize(), outVertexLayout);

    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create vertex layout. %s", GetErrorMessage(hr).c_str());
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
    D3D11_TEXTURE2D_DESC backBufferDesc;
    g_BackBuffer->GetDesc(&backBufferDesc);

    const float aspectRatio = static_cast<float>(backBufferDesc.Width) / static_cast<float>(backBufferDesc.Height);
    const float zRange = g_CameraFar - g_CameraNear;
    const float tanHalfFov = tanf(DirectX::XMConvertToRadians(g_CameraFov * 0.5f));
    
    DirectX::XMFLOAT4X4 projectionMatrix = {};
    projectionMatrix.m[0][0] = 1 / (tanHalfFov * aspectRatio);
    projectionMatrix.m[1][1] = 1 / tanHalfFov;
    projectionMatrix.m[2][2] = g_CameraFar / zRange;
    projectionMatrix.m[3][2] = -g_CameraFar * g_CameraNear / zRange;
    projectionMatrix.m[2][3] = 1;

    g_ProjectionMatrix = DirectX::XMLoadFloat4x4(&projectionMatrix);
}

HRESULT CreateGrid()
{
    CComPtr<ID3DBlob> vsc, psc;
    
    HRESULT hr = CompileShaders(g_GridShaderFileName, &g_GridVertexShader, &g_GridPixelShader, &vsc, &psc);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create grid shaders.");
        return hr;
    }

    const int halfGridLength = g_GridLength / 2;
    const int halfGridWidth = g_GridWidth / 2;
    std::vector<GridVertex> vertices(g_GridVertexCount);
    std::vector<uint32_t>   indices(g_GridVertexCount);

    int index = 0;
    for (int z = -halfGridLength; z < halfGridLength + 1; z++)
    {
        DirectX::XMFLOAT3 color = z % 5 == 0 ? g_GridBoldColor : g_GridColor;
        
        vertices[index].m_Position = {  static_cast<float>(-halfGridWidth), 0, static_cast<float>(z) };
        vertices[index].m_Color = color;
        indices[index] = index;
        index++;
        
        vertices[index].m_Position = {  static_cast<float>(halfGridWidth), 0, static_cast<float>(z) };
        vertices[index].m_Color = color;
        indices[index] = index;
        index++;
    }
    
    for (int x = -halfGridWidth; x < halfGridWidth + 1; x++)
    {
        DirectX::XMFLOAT3 color = x % 5 == 0 ? g_GridBoldColor : g_GridColor;
        
        vertices[index].m_Position = {  static_cast<float>(x), 0, static_cast<float>(-halfGridLength) };
        vertices[index].m_Color = color;
        indices[index] = index;
        index++;
        
        vertices[index].m_Position = {  static_cast<float>(x), 0, static_cast<float>(halfGridLength) };
        vertices[index].m_Color = color;
        indices[index] = index;
        
        index++;
    }

    hr = CreateVertexBuffer(vertices, &g_GridVertexBuffer);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create grid vertex buffer.");
        return hr;
    }

    hr = CreateIndexBuffer(indices, &g_GridIndexBuffer);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create grid index buffer.");
        return hr;
    }

    hr = CreateVertexLayoutUsingReflection(vsc, &g_GridVertexLayout);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create grid vertex layout.");
        return hr;
    }
    
    return hr;
}

DirectX::XMFLOAT3 GetFbxMeshPolygonVertexColor(FbxMesh* mesh, int vertexId, int controlPointId)
{
    const FbxGeometryElementVertexColor* colorElement = mesh->GetElementVertexColor();
    if (!colorElement)
        return {1, 1, 1};

    FbxColor color;
    switch (colorElement->GetMappingMode())
    {
    case FbxGeometryElement::eByControlPoint:
        {
            switch (colorElement->GetReferenceMode())
            {
            case FbxGeometryElement::eDirect:
                {
                    color = colorElement->GetDirectArray().GetAt(controlPointId);
                    break;
                }
            case FbxGeometryElement::eIndexToDirect:
                {
                    int id = colorElement->GetIndexArray().GetAt(controlPointId);
                    color = colorElement->GetDirectArray().GetAt(id);
                    break;
                }
            default: { break; }
            }
            break;
        }
    case FbxGeometryElement::eByPolygonVertex:
        {
            switch (colorElement->GetReferenceMode())
            {
            case FbxGeometryElement::eDirect:
                {
                    color = colorElement->GetDirectArray().GetAt(vertexId);
                    break;
                }
            case FbxGeometryElement::eIndexToDirect:
                {
                    int id = colorElement->GetIndexArray().GetAt(vertexId);
                    color = colorElement->GetDirectArray().GetAt(id);
                    break;
                }
            default: { break; }
            }
            break;
        }
    default: { break; }
    }

    return {
        static_cast<float>(color.mRed),
        static_cast<float>(color.mGreen),
        static_cast<float>(color.mBlue)
    };
}

void InitFbxSdk()
{
    g_FbxManager = FbxSharedDestroyPtr<FbxManager>(FbxManager::Create());
}

HRESULT CreateModelShadersAndLayout()
{
    HRESULT           hr = S_OK;
    CComPtr<ID3DBlob> vsCode, psCode;
    hr = CompileShaders(g_ModelShaderFileName, &g_ModelVertexShader, &g_ModelPixelShader, &vsCode, &psCode);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to compile shaders for the model.");
        return hr;
    }

    hr = CreateVertexLayoutUsingReflection(vsCode, &g_ModelVertexLayout);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create model vertex layout. ");
        return hr;
    }

    return hr;
}

HRESULT CreateModel()
{
    HRESULT hr = S_OK;
    
    FbxSharedDestroyPtr<FbxIOSettings> fbxIOSettings(FbxIOSettings::Create(g_FbxManager, IOSROOT));
    FbxSharedDestroyPtr<FbxImporter>   fbxImporter(FbxImporter::Create(g_FbxManager, g_ModelFileName.c_str()));

    if (!fbxImporter->Initialize(g_ModelFileName.c_str(), -1, fbxIOSettings))
    {
        DEBUG_LOG_FORMAT("Call to FbxImporter::Initialize() failed. %s", fbxImporter->GetStatus().GetErrorString());
        return E_FAIL;
    }

    FbxSharedDestroyPtr<FbxScene> fbxScene(FbxScene::Create(g_FbxManager, "SampleModelScene"));
    if (!fbxImporter->Import(fbxScene))
    {
        DEBUG_LOG_FORMAT("Could not import scene. %s", fbxImporter->GetStatus().GetErrorString());
        return E_FAIL;
    }
    fbxImporter.Destroy();

    if (fbxScene->GetNodeCount() == 0)
    {
        DEBUG_LOG_FORMAT("No content is present in the fbx file.");
        return E_FAIL;
    }

    // Doesn't seem to work for me. I'm converting manually instead
    //FbxAxisSystem::DirectX.ConvertScene(fbxScene);

    // Triangulate the meshes
    FbxGeometryConverter geometryConverter(g_FbxManager);
    geometryConverter.Triangulate(fbxScene, true);
    geometryConverter.RemoveBadPolygonsFromMeshes(fbxScene);
    
    const FbxSystemUnit& mUnit = FbxSystemUnit::m;

    constexpr FbxSystemUnit::ConversionOptions conversionOptions = {
        false, /* mConvertRrsNodes */
        true, /* mConvertAllLimits */
        true, /* mConvertClusters */
        true, /* mConvertLightIntensity */
        true, /* mConvertPhotometricLProperties */
        true /* mConvertCameraClipPlanes */
    };

    mUnit.ConvertScene(fbxScene, conversionOptions);

    FbxNode* meshNode = nullptr;
    FbxMesh* mesh = nullptr;
    for (int i = 0; i < fbxScene->GetNodeCount(); i++)
    {
        FbxNode*          n = fbxScene->GetNode(i);
        FbxNodeAttribute* attr = n->GetNodeAttribute();
        if (!attr)
            continue;

        FbxNodeAttribute::EType attrType = attr->GetAttributeType();

        if (attrType == FbxNodeAttribute::EType::eMesh)
        {
            mesh = static_cast<FbxMesh*>(attr);
            meshNode = n;
            break;
        }
    }

    if (!mesh)
    {
        DEBUG_LOG_FORMAT("Could not find any meshes in fbx file.");
        return E_FAIL;
    }

    std::vector<ModelVertex> vertices;
    std::vector<uint32_t>   indices;

    FbxVector4* controlPoints = mesh->GetControlPoints();
    int         polyCount = mesh->GetPolygonCount();

    FbxMatrix fbxTransform = meshNode->EvaluateGlobalTransform();
    
    // Convert from right handed to left handed. Flip z axis, rotate by 180 to face the z axis
    FbxMatrix m(FbxVector4(), FbxVector4(0, 180, 0), FbxVector4(1, 1, -1));
    fbxTransform *= m;

    int vertexCounter = 0;
    for (int j = 0; j < polyCount; j++)
    {
        int iNumVertices = mesh->GetPolygonSize(j);
        assert(iNumVertices == 3);

        indices.push_back(j * 3);
        indices.push_back(j * 3 + 2);
        indices.push_back(j * 3 + 1);

        for (int k = 0; k < iNumVertices; k++)
        {
            int iControlPointIndex = mesh->GetPolygonVertex(j, k);

            FbxVector4 pos = controlPoints[iControlPointIndex];
            pos = fbxTransform.MultNormalize(pos);
            double* p = pos.mData;

            ModelVertex vertex;
            vertex.m_Position =
            {
                static_cast<float>(p[0]),
                static_cast<float>(p[1]),
                static_cast<float>(p[2])
            };

            FbxVector4 fbxNormal;
            mesh->GetPolygonVertexNormal(j, k, fbxNormal);

            // convert to normals to left handed
            fbxNormal = m.MultNormalize(fbxNormal);

            vertex.m_Color = GetFbxMeshPolygonVertexColor(mesh, vertexCounter, iControlPointIndex);

            vertex.m_Normal = DirectX::XMFLOAT3(
                static_cast<float>(fbxNormal.mData[0]),
                static_cast<float>(fbxNormal.mData[1]),
                static_cast<float>(fbxNormal.mData[2]));

            vertices.push_back(vertex);

            vertexCounter++;
        }
    }

    g_ModelVertexCount = vertices.size();

    g_ModelIndexBuffer.Release();
    g_ModelVertexBuffer.Release();
    
    hr = CreateVertexBuffer(vertices, &g_ModelVertexBuffer);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create model vertex buffer. ");
        return hr;
    }

    hr = CreateIndexBuffer(indices, &g_ModelIndexBuffer);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create model index buffer. ");
        return hr;
    }

    return hr;
}

void ProcessFrame()
{
    // Uncomment in case we need delta time
    /*static auto timePrevFrame = std::chrono::high_resolution_clock::now();
    const auto  timeThisFrame = std::chrono::high_resolution_clock::now();
    const float deltaTime = std::chrono::duration<float>(timeThisFrame - timePrevFrame).count();
    timePrevFrame = timeThisFrame;*/

    ID3D11DeviceContext* ctx = g_DeviceContext;
    
    // Rotate model to face the camera
    const DirectX::XMMATRIX worldMatrix = DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(180));
    
    const DirectX::XMMATRIX cameraRotationMatrix =
        DirectX::XMMatrixRotationRollPitchYaw(
            DirectX::XMConvertToRadians(g_CameraPitch),
            DirectX::XMConvertToRadians(g_CameraYaw),
             0);
    
    DirectX::XMMATRIX cameraMatrix = DirectX::XMMatrixTranslation(0, 0, g_CameraZoomZ) * cameraRotationMatrix *
        DirectX::XMMatrixTranslationFromVector(g_CameraPos);

    DirectX::XMVECTOR worldSpaceCameraPos = DirectX::XMVector4Transform(DirectX::XMVectorSet(0, 0, 0, 1), cameraMatrix);

    cameraMatrix = DirectX::XMMatrixInverse(nullptr, cameraMatrix);

    DirectX::XMVECTOR cameraDir = DirectX::XMVector4Normalize(DirectX::XMVectorNegate(worldSpaceCameraPos));
    DirectX::XMStoreFloat3(&g_DirectionalLightData.m_Direction, cameraDir);

    FrameConstantBufferData frameCbd = {};
    frameCbd.m_ModelMatrix = XMMatrixTranspose(worldMatrix);
    frameCbd.m_ViewMatrix = XMMatrixTranspose(cameraMatrix);
    frameCbd.m_MvpMatrix = XMMatrixTranspose(worldMatrix * cameraMatrix * g_ProjectionMatrix);
    frameCbd.m_DirectionalLightData = g_DirectionalLightData;
    XMStoreFloat4(&frameCbd.m_WorldSpaceCameraPos, worldSpaceCameraPos);

    ctx->UpdateSubresource(g_FrameConstantBuffer, 0, nullptr, &frameCbd, 0, 0);
    
    ctx->OMSetRenderTargets(1, &g_BackBufferView.p, g_DepthStencilView);

    ctx->OMSetDepthStencilState(g_DepthStencilState, 1);
    
    ctx->RSSetState(g_RasterizerState);

    ctx->ClearRenderTargetView(g_BackBufferView, reinterpret_cast<FLOAT*>(&g_ClearColor));
    
    ctx->ClearDepthStencilView(g_DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

    // Render model
    constexpr UINT modelBufferStride = sizeof(ModelVertex);
    constexpr UINT modelBufferOffset = 0;
    ctx->IASetVertexBuffers(0, 1, &g_ModelVertexBuffer.p, &modelBufferStride, &modelBufferOffset);

    ctx->VSSetShader(g_ModelVertexShader, nullptr, 0);

    ctx->VSSetConstantBuffers(0, 1, &g_FrameConstantBuffer.p);
    ctx->PSSetConstantBuffers(0, 1, &g_FrameConstantBuffer.p);

    ctx->PSSetShader(g_ModelPixelShader, nullptr, 0);
    ctx->IASetIndexBuffer(g_ModelIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(g_ModelVertexLayout);
    ctx->DrawIndexed(g_ModelVertexCount, 0, 0);

    // Render grid
    frameCbd.m_ModelMatrix = DirectX::XMMatrixIdentity();
    frameCbd.m_MvpMatrix = XMMatrixTranspose(cameraMatrix * g_ProjectionMatrix);

    ctx->UpdateSubresource(g_FrameConstantBuffer, 0, nullptr, &frameCbd, 0, 0);
    
    constexpr UINT gridBufferStride = sizeof(GridVertex);
    constexpr UINT gridBufferOffset = 0;
    ctx->IASetVertexBuffers(0, 1, &g_GridVertexBuffer.p, &gridBufferStride, &gridBufferOffset);
    
    ctx->IASetIndexBuffer(g_GridIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    ctx->VSSetShader(g_GridVertexShader, nullptr, 0);
    
    ctx->PSSetShader(g_GridPixelShader, nullptr, 0);

    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    ctx->IASetInputLayout(g_GridVertexLayout);

    ctx->DrawIndexed(g_GridVertexCount, 0, 0);

    // present the frame
    g_SwapChain->Present(0, 0);
}

void MessageBoxModelLoadError(HWND hWnd, HRESULT hr)
{
    MessageBoxA(hWnd,  (std::string( "Could not open the model. File corrupt or invalid. \n\n" )
    + GetErrorMessage(hr)).c_str(), "Error", MB_OK|MB_ICONERROR);
}

LRESULT CALLBACK MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            const int menuId = LOWORD(wParam);

            switch (menuId)
            {
            case IDM_FILE_OPEN:
                {
                    OPENFILENAMEA ofn{};
                    char szFile[260];       // buffer for file name

                    char initialDir[4096];
                    GetFullPathNameA("Data/Models/", 4096, initialDir, nullptr);
                    
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lpstrTitle = "Select Model";
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hWnd;
                    ofn.lpstrFile = szFile;
                    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
                    // use the contents of szFile to initialize itself.
                    ofn.lpstrFile[0] = '\0';
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = "FBX\0*.FBX\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = nullptr;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = initialDir;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                    // Display the Open dialog box. 

                    if (GetOpenFileNameA(&ofn))
                    {
                        g_ModelFileName = ofn.lpstrFile;
                        HRESULT hr = CreateModel();
                        if (FAILED(hr))
                        {
                            MessageBoxModelLoadError(hWnd, hr);
                        }
                    }
                }
                break;
            case IDM_FILE_QUIT:
                SendMessage(hWnd, WM_CLOSE, 0, 0);
                break;
            default:
                break;
            }
        }
        break;
        
    case WM_SIZE:
        {
            g_DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
            g_DeviceContext->Flush();

            g_BackBuffer.Release();
            g_BackBufferView.Release();

            g_SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
            HRESULT hr = SetUpBackBuffer();

            if (FAILED(hr))
            {
                DEBUG_LOG_FORMAT("Failed to recreate back buffer during resize event. %s", GetErrorMessage(hr).c_str());
                PostQuitMessage(hr);
                return 0;
            }

            SetUpViewport();
            CalculateProjectionMatrix();
            ProcessFrame();
        }
        break;
        
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_MOUSEWHEEL:
        {
            float delta = GET_WHEEL_DELTA_WPARAM(wParam);

            float zoomDeltaFactor = Clamp(delta * (1.0f / g_CameraZoomMaxScrollDelta), -1.0f, 1.0f);
            g_CameraZoomZ += zoomDeltaFactor * g_CameraZoomMaxSpeed;
            g_CameraZoomZ = std::min(0.0f, g_CameraZoomZ);
        }
        break;
    
    case WM_LBUTTONDOWN:
        {
            if (!g_CameraOrbitEnabled)
            {
                SetCapture(hWnd);
                g_CameraOrbitEnabled = true;
            }
        }
        break;
    case WM_LBUTTONUP:
        {
            if (g_CameraOrbitEnabled)
            {
                ReleaseCapture();
                g_CameraOrbitEnabled = false;
            }
        }
        break;
    case WM_RBUTTONDOWN:
        {
            if (!g_CameraPanEnabled)
            {
                SetCapture(hWnd);
                g_CameraPanEnabled = true;
            }
        }
        break;
    case WM_RBUTTONUP:
        {
            if (g_CameraPanEnabled)
            {
                ReleaseCapture();
                g_CameraPanEnabled = false;
            }
        }
        break;

    case WM_MOUSEMOVE:
        {
            auto px = static_cast<float>(GET_X_LPARAM(lParam));
            auto py = static_cast<float>(GET_Y_LPARAM(lParam));

            float mouseDeltaX = px - g_MousePosLastMoveEvent.x;
            float mouseDeltaY = py - g_MousePosLastMoveEvent.y;

            if (g_CameraOrbitEnabled)
            {
                float orbitDeltaFactorX = Clamp(mouseDeltaX * (1 / g_CameraOrbitMaxMouseDelta), -1.0f, 1.0f);
                float orbitDeltaFactorY = Clamp(mouseDeltaY * (1 / g_CameraOrbitMaxMouseDelta), -1.0f, 1.0f);

                g_CameraYaw += orbitDeltaFactorX * g_CameraOrbitMaxSpeed;
                g_CameraPitch += orbitDeltaFactorY * g_CameraOrbitMaxSpeed;
            }

            if (g_CameraPanEnabled)
            {
                float deltaFactorX = Clamp(mouseDeltaX * (1 / g_CameraPanMaxMouseDelta), -1.0f, 1.0f);
                float deltaFactorY = Clamp(mouseDeltaY * (1 / g_CameraPanMaxMouseDelta), -1.0f, 1.0f);
                
                const DirectX::XMMATRIX cameraRotationMatrix =
                    DirectX::XMMatrixRotationRollPitchYaw(
                        DirectX::XMConvertToRadians(g_CameraPitch),
                        DirectX::XMConvertToRadians(g_CameraYaw),
                         0);

                DirectX::XMVECTOR panVector = DirectX::XMVector3Transform(
                    DirectX::XMVectorSet(deltaFactorX, deltaFactorY, 0, 0), cameraRotationMatrix);
                
                g_CameraPos = DirectX::XMVectorAdd(g_CameraPos, panVector);
            }

            g_MousePosLastMoveEvent.x = px;
            g_MousePosLastMoveEvent.y = py;
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}