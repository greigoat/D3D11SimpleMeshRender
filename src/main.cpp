// stl headers
#include <memory>
#include <system_error>
#include <string>
#include <cmath>
#include <algorithm>
#include <fbxsdk.h>

// Win32 headers
// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN             
// ReSharper disable once IdentifierTypo
#define NOMINMAX
#include <Windows.h>

#include <atlbase.h>

// D3D11 headers
#include <chrono>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <vector>

#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "d3d11.lib")

// Defines
#if defined (DEBUG) || defined(_DEBUG)
#define DEBUG_LOG_FORMAT(format, ...) DebugLogFormat(format, __VA_ARGS__)
#else
#define DEBUG_LOG_FORMAT(format, ...)
#endif
#undef near
#undef far

// Structs
struct DirectionalLightData
{
    DirectX::XMFLOAT4 m_Color;
    DirectX::XMFLOAT3 m_Direction;
    float             m_Attenuation;
};

struct VertexData
{
    DirectX::XMFLOAT3 m_Position;
    DirectX::XMFLOAT3 m_Color;
    DirectX::XMFLOAT3 m_Normal;
};


struct FrameConstantBufferData
{
    DirectX::XMMATRIX    m_ModelMatrix;
    DirectX::XMMATRIX    m_ViewMatrix;
    DirectX::XMMATRIX    m_MvpMatrix;
    DirectX::XMFLOAT4 m_WorldSpaceCameraPos; 
    DirectionalLightData m_DirectionalLightData;
};

// Typedefs


// Globals
HWND          g_SampleWindow;
const TCHAR*  g_SampleWindowName = L"D3D11SimpleMeshRender";
const TCHAR*  g_SampleWindowClassName = L"D3D11SimpleMeshRenderClass";
constexpr int g_SampleWindowWidth = 759;//1024;
constexpr int g_SampleWindowHeight = 291;//768;

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
constexpr const char*           g_ModelFileName = "Data/Models/Suzanne.fbx";

int g_GridWidth = 32;
int g_GridLength = 32;
CComPtr<ID3D11Buffer> g_GridVertexBuffer;
CComPtr<ID3D11Buffer> g_GridIndexBuffer;
CComPtr<ID3D11VertexShader> g_GridVertexShader;
CComPtr<ID3D11PixelShader> g_GridPixelShader;
CComPtr<ID3D11InputLayout>  g_GridVertexLayout;
//CComPtr<ID3D11PixelShader> g_GridPixelShader;

CComPtr<ID3D11VertexShader> g_VS;
CComPtr<ID3D11PixelShader>  g_PS;
CComPtr<ID3D11Buffer>       g_SampleGeometryVertexBuffer;
CComPtr<ID3D11Buffer>       g_SampleGeometryIndexBuffer;
CComPtr<ID3D11InputLayout>  g_VertexLayout;
CComPtr<ID3D11Buffer>       g_PerFrameConstantBuffer;
DirectX::XMMATRIX           g_ProjectionMatrix;
DirectX::XMFLOAT4           g_ClearColor = {0.5f, 0.5f, 0.5f, 1};
constexpr DirectX::XMFLOAT3 g_GeomVertexColor = {1, 1, 1};

DirectionalLightData g_DirectionalLightData
{
    {1.0f, 1.0f, 1.0f, 1.0f}, // color
    {0, -0.5f, 0.5f}, // direction
    0.8f // attenuation
};

// Create cube geometry.
constexpr VertexData g_GeomVerts[] =
{
    {DirectX::XMFLOAT3(-1, 0, 0), g_GeomVertexColor, {0, 1, 0}},
    {DirectX::XMFLOAT3( 0, 1, 0), g_GeomVertexColor, {0, 1, 0}},
    {DirectX::XMFLOAT3(1, 0, 0), g_GeomVertexColor, {0, 1, 0}},
};

constexpr uint32_t g_GeomIndices[] =
{
    0, 1, 2, 0, 2, 3
};

void DebugLogFormat(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    char szBuffer[512];
    _vsnprintf_s(szBuffer, 511, format, args);

    OutputDebugStringA(szBuffer);

    va_end(args);
}

std::string GetHResultMessage(HRESULT hr) { return std::system_category().message(hr); }

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
    g_DepthStencilBuffer.Release();
    g_DepthStencilState.Release();
    g_DepthStencilView.Release();
    g_RasterizerState.Release();

    HRESULT hr = g_SwapChain->GetBuffer(0, IID_PPV_ARGS(&g_BackBuffer));
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Couldn't obtain back buffer %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    hr = g_Device->CreateRenderTargetView(g_BackBuffer, nullptr, &g_BackBufferView);

    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Couldn't create back buffer view %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    D3D11_TEXTURE2D_DESC backBufferSurfaceDesc;
    g_BackBuffer->GetDesc(&backBufferSurfaceDesc);

    D3D11_TEXTURE2D_DESC descDepth;
    descDepth.Width = backBufferSurfaceDesc.Width;
    descDepth.Height = backBufferSurfaceDesc.Height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; //  24 bits for the depth, and 8 bits for the stencil
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    // this texture will be bound to the OM stage as a depth/stencil buffer
    descDepth.CPUAccessFlags = 0;
    descDepth.MiscFlags = 0;

    hr = g_Device->CreateTexture2D(&descDepth, nullptr, &g_DepthStencilBuffer);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Couldn't create depth stencil buffer %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    // Setup depth stencil
    /*D3D11_DEPTH_STENCIL_DESC dsDesc;

    // Depth test parameters
    dsDesc.DepthEnable = true;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;

    // Stencil test parameters
    dsDesc.StencilEnable = true;
    dsDesc.StencilReadMask = 0xFF;
    dsDesc.StencilWriteMask = 0xFF;

    // Create depth stencil state
    g_Device->CreateDepthStencilState(&dsDesc, &g_DepthStencilState);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Couldn't create depth stencil state %s", GetHResultMessage(hr).c_str());
        return hr;
    }*/

    g_Device->CreateDepthStencilView(g_DepthStencilBuffer, nullptr, &g_DepthStencilView);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Couldn't create depth stencil view %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    D3D11_RASTERIZER_DESC desc;
    desc.AntialiasedLineEnable = TRUE;
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
        DEBUG_LOG_FORMAT("Couldn't create rasterizer state %s", GetHResultMessage(hr).c_str());
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
    float aspectRatio = static_cast<float>(desc.Width) / static_cast<float>(desc.Height);
    
    DirectX::XMFLOAT4X4 m{};
    float zFar = 1000.0f;
    float zNear = 0.3f;
    float fov = 70.0f;
    float zRange = zFar - zNear;
    float degToRad = 0.01745329f;
    float tanHalfFov = tanf(fov * 0.5f * degToRad); 

    float ar = aspectRatio;

    m.m[0][0] = 1 / ( tanHalfFov * ar );
    m.m[1][1] = 1 / tanHalfFov;
    m.m[2][2] = zFar / (zFar - zNear);
    m.m[3][2] = -zFar * zNear / (zFar - zNear);
    m.m[2][3] = 1;
    
    g_ProjectionMatrix = DirectX::XMLoadFloat4x4(&m);
    
    /*g_ProjectionMatrix = DirectX::XMMatrixPerspectiveFovLH(
        2.0f * std::atan(std::tan(DirectX::XMConvertToRadians(70) * 0.5f) / aspectRatio),
        aspectRatio,
        0.3f,
        1000.0f);

    DirectX::XMFLOAT4X4 f;
    DirectX::XMStoreFloat4x4(&f, g_ProjectionMatrix);*/

    //g_ProjectionMatrix = DirectX::XMMatrixIdentity();
}

HRESULT CreateGrid()
{
    CComPtr<ID3DBlob> vsc, psc;
    HRESULT           hr = S_OK;

    UINT shaderCompilerFlags = 0;
#if (defined DEBUG || defined _DEBUG)
    shaderCompilerFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    CComPtr<ID3DBlob> errors;
        
    hr = D3DCompileFromFile(L"Data/Shaders/grid.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", shaderCompilerFlags, 0,
                        &vsc, &errors.p);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed compile vertex shader. %s %s", GetHResultMessage(hr).c_str(), static_cast<char*>(errors->GetBufferPointer()));
        return hr;
    }

    hr = g_Device->CreateVertexShader(vsc->GetBufferPointer(), vsc->GetBufferSize(), nullptr, &g_GridVertexShader);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create vertex shader. %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    hr = D3DCompileFromFile(L"Data/Shaders/grid.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", shaderCompilerFlags, 0,
                        &psc, &errors.p);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed compile pixel shader. %s %s", GetHResultMessage(hr).c_str(), static_cast<char*>(errors->GetBufferPointer()));
        return hr;
    }

    hr = g_Device->CreatePixelShader(psc->GetBufferPointer(), psc->GetBufferSize(), nullptr, &g_GridPixelShader);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create pixel shader. %s", GetHResultMessage(hr).c_str());
        return hr;
    }
    
    std::vector<DirectX::XMFLOAT3> vertices(g_GridWidth * g_GridLength * 8);
    std::vector<uint32_t> indices(vertices.size());

    int index = 0;
    
    // Load the vertex array and index array with data.
    for(int j=-g_GridLength/2; j<((g_GridLength/2)-1); j++)
    {
        for(int i=-g_GridWidth/2; i<(g_GridWidth/2-1); i++)
        {
            // Line 1 - Upper left.
            float positionX = (float)i;
            float positionZ = (float)(j + 1);

            vertices[index] = DirectX::XMFLOAT3(positionX, 0.0f, positionZ);
            indices[index] = index;
            index++;

            // Line 1 - Upper right.
            positionX = (float)(i + 1);
            positionZ = (float)(j + 1);

            vertices[index] = DirectX::XMFLOAT3(positionX, 0.0f, positionZ);
            indices[index] = index;
            index++;

            // Line 2 - Upper right
            positionX = (float)(i + 1);
            positionZ = (float)(j + 1);

            vertices[index] = DirectX::XMFLOAT3(positionX, 0.0f, positionZ);
            indices[index] = index;
            index++;

            // Line 2 - Bottom right.
            positionX = (float)(i + 1);
            positionZ = (float)j;

            vertices[index] = DirectX::XMFLOAT3(positionX, 0.0f, positionZ);
            //vertices[index].color = color;
            indices[index] = index;
            index++;

            // Line 3 - Bottom right.
            positionX = (float)(i + 1);
            positionZ = (float)j;

            vertices[index] = DirectX::XMFLOAT3(positionX, 0.0f, positionZ);
            //vertices[index].color = color;
            indices[index] = index;
            index++;

            // Line 3 - Bottom left.
            positionX = (float)i;
            positionZ = (float)j;

            vertices[index] = DirectX::XMFLOAT3(positionX, 0.0f, positionZ);
            //vertices[index].color = color;
            indices[index] = index;
            index++;

            // Line 4 - Bottom left.
            positionX = (float)i;
            positionZ = (float)j;

            vertices[index] = DirectX::XMFLOAT3(positionX, 0.0f, positionZ);
            indices[index] = index;
            index++;

            // Line 4 - Upper left.
            positionX = (float)i;
            positionZ = (float)(j + 1);

            vertices[index] = DirectX::XMFLOAT3(positionX, 0.0f, positionZ);
            indices[index] = index;
            index++;
        }
    }
    
    D3D11_BUFFER_DESC vertexBufferDesc{};
    vertexBufferDesc.ByteWidth = vertices.size() * sizeof(DirectX::XMFLOAT3);
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA vertexData { vertices.data() };

    hr = g_Device->CreateBuffer(&vertexBufferDesc, &vertexData, &g_GridVertexBuffer);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create vertex buffer. %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    // Create index buffer
    D3D11_BUFFER_DESC indexBufferDesc{};
    indexBufferDesc.ByteWidth = indices.size() * sizeof(uint32_t);
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexData { indices.data() };

    hr = g_Device->CreateBuffer(&indexBufferDesc, &indexData, &g_GridIndexBuffer);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create index buffer. %s", GetHResultMessage(hr).c_str());
        return hr;
    }
    
    D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    hr = g_Device->CreateInputLayout(vertexLayoutDesc, ARRAYSIZE(vertexLayoutDesc),
                                     vsc->GetBufferPointer(), vsc->GetBufferSize(), &g_GridVertexLayout);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create vertex layout. %s", GetHResultMessage(hr).c_str());
        return hr;
    }
    
    return hr;
}

DirectX::XMFLOAT3 GetPolygonVertexColor(FbxMesh* mesh, int vertexId, int controlPointId)
{
    const FbxGeometryElementVertexColor* colorElement = mesh->GetElementVertexColor();
    if (!colorElement)
        return {1,1,1};

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
            default:
                {
                    break;
                }
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
            default:
                {
                    break;
                }
            }
            break;
        }
    default:
        {
            break;
        }
    }

    return {
        static_cast<float>(color.mRed),
        static_cast<float>(color.mGreen),
        static_cast<float>(color.mBlue)};
}

int g_indexCount;  
HRESULT InitSample()
{
    g_FbxManager = FbxSharedDestroyPtr<FbxManager>(FbxManager::Create());
    FbxSharedDestroyPtr<FbxIOSettings> fbxIOSettings(FbxIOSettings::Create(g_FbxManager, IOSROOT));
    FbxSharedDestroyPtr<FbxImporter>   fbxImporter(FbxImporter::Create(g_FbxManager, g_ModelFileName));

    if (!fbxImporter->Initialize(g_ModelFileName, -1, fbxIOSettings))
    {
        DEBUG_LOG_FORMAT("Call to FbxImporter::Initialize() failed. %s \n", fbxImporter->GetStatus().GetErrorString());
        return E_FAIL;
    }

    FbxSharedDestroyPtr<FbxScene> fbxScene(FbxScene::Create(g_FbxManager, "SampleModelScene"));
    if (!fbxImporter->Import(fbxScene))
    {
        DEBUG_LOG_FORMAT("Could not import scene. %s \n", fbxImporter->GetStatus().GetErrorString());
        return E_FAIL;
    }
    fbxImporter.Destroy();

    if (fbxScene->GetNodeCount() == 0)
    {
        DEBUG_LOG_FORMAT("No content is present in the fbx file. \n");
        return E_FAIL;
    }

    //FbxAxisSystem::DirectX.ConvertScene(fbxScene);

    // Triangulate the meshes
    FbxGeometryConverter geometryConverter( g_FbxManager );
    geometryConverter.Triangulate( fbxScene, true );
    geometryConverter.RemoveBadPolygonsFromMeshes(fbxScene);
    
    const FbxSystemUnit& sysUnit = fbxScene->GetGlobalSettings().GetSystemUnit();
    const FbxSystemUnit& mUnit = FbxSystemUnit::m;

    const FbxSystemUnit::ConversionOptions lConversionOptions = {
        false, /* mConvertRrsNodes */
        true, /* mConvertAllLimits */
        true, /* mConvertClusters */
        true, /* mConvertLightIntensity */
        true, /* mConvertPhotometricLProperties */
        true  /* mConvertCameraClipPlanes */
      };
    
    mUnit.ConvertScene(fbxScene, lConversionOptions);
    
    FbxNode* meshNode = nullptr;
    FbxMesh* mesh = nullptr;
    for (int i = 0; i < fbxScene->GetNodeCount(); i++)
    {
        FbxNode* n = fbxScene->GetNode(i);
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
        DEBUG_LOG_FORMAT("Could not find any meshes in fbx file. \n");
        return E_FAIL;
    }
    
    std::vector<VertexData> vertices;
    std::vector<int> indices;
    
    FbxVector4* controlPoints = mesh->GetControlPoints();
    int polyCount = mesh->GetPolygonCount();
    
    FbxMatrix fbxTransform = meshNode->EvaluateGlobalTransform();
    
    //fbxTransform.SetRow(3, FbxVector4(0,0,0,1));
    
    // Convert from right handed to left handed. Flip z axis, rotate by 180 to face the z axis
    FbxMatrix m(FbxVector4(), FbxVector4(0,180,0), FbxVector4(1,1,-1));
    fbxTransform *= m;

    int vertexCounter = 0;
    for (int j = 0; j < polyCount; j++)
    {
        int iNumVertices = mesh->GetPolygonSize(j);
        assert( iNumVertices == 3 );

        indices.push_back(j * 3);
        indices.push_back(j * 3 + 2);
        indices.push_back(j * 3 + 1);
        
        for (int k = 0; k < iNumVertices; k++)
        {
            int iControlPointIndex = mesh->GetPolygonVertex(j, k);

            // 0,1,2, 3,4,5, 6,7,8
            // 0,2,1, 3,5,4, 6,8,7

            /*if ((indicesIndex % 2) == 0)
                indicesIndex+=1;*/
            /*if (indicesIndex > 0 (indicesIndex % 3) == 0)
                indicesIndex-=1;*/
            
            //indices.push_back(indicesIndex++);

            FbxVector4 pos = controlPoints[iControlPointIndex];
            //pos.mData[3] = 1;
            pos = fbxTransform.MultNormalize(pos);
            
            //pos = fbxTransform.MultR(pos);
            //pos = fbxTransform.MultS(pos);
            
            //pos *= 0.5f;
            
            double* p = pos.mData;
            
            VertexData vertex;
            vertex.m_Position =
                {
                static_cast<float>(p[0]),
                static_cast<float>(p[1]),
                static_cast<float>(p[2])
                };

            FbxVector4 fbxNormal;
            mesh->GetPolygonVertexNormal(j, k, fbxNormal);
            
            //fbxNormal = fbxTransform.MultNormalize(fbxNormal);
            fbxNormal = m.MultNormalize(fbxNormal);
            
            vertex.m_Color = GetPolygonVertexColor(mesh, vertexCounter, iControlPointIndex); // g_GeomVertexColor;
            
            vertex.m_Normal =  DirectX::XMFLOAT3(
                static_cast<float>(fbxNormal.mData[0]),
                static_cast<float>(fbxNormal.mData[1]),
                static_cast<float>(fbxNormal.mData[2]));
            
            vertices.push_back(vertex);

            vertexCounter++;
        }
    }

    /*indices =
    {
        0, 2, 1, = 0+0, 0+2, 0+1
        3, 5, 4, = 3+0, 3+2, 3+1
        
        6, 8, 7, = 6+0, 6+2, 6+1
        9, 11, 10,

        12, 14, 13,
        15, 17, 16,
    };*/
    
    //indices.assign(mesh->GetPolygonVertices(), mesh->GetPolygonVertices() + mesh->GetPolygonVertexCount());
    /*vertices =
    {
        {DirectX::XMFLOAT3(-1, -1, -1), g_GeomVertexColor, {-1, -1, -1}},
        {DirectX::XMFLOAT3(-1, -1, 1), g_GeomVertexColor, {-1, -1, 1}},
        {DirectX::XMFLOAT3(-1, 1, -1), g_GeomVertexColor, {-1, 1, -1}},

        {DirectX::XMFLOAT3(-1, -1, 1), g_GeomVertexColor, {-1, -1, 1}},
        {DirectX::XMFLOAT3(-1, 1, 1), g_GeomVertexColor, {-1, 1, 1}},
        {DirectX::XMFLOAT3(-1, 1, -1), g_GeomVertexColor, {-1, 1, -1}},

        {DirectX::XMFLOAT3(-1, -1, 1), g_GeomVertexColor, {-1, -1, 1}}, //6g
        {DirectX::XMFLOAT3(1, 1, 1), g_GeomVertexColor, {1, 1, 1}}, //7
        {DirectX::XMFLOAT3(1, -1, 1), g_GeomVertexColor, {1, -1, 1}}, //8

        {DirectX::XMFLOAT3(1, 1, 1), g_GeomVertexColor, {1, 1, 1}},
{DirectX::XMFLOAT3(-1, 1, 1), g_GeomVertexColor, {-1, 1, 1}},
{DirectX::XMFLOAT3(-1, -1, 1), g_GeomVertexColor, {-1, -1, 1}},
        
    };*/
    int vertexCount = vertices.size();
    
    /*int icount = mesh->GetPolygonVertexCount();
    std::vector<int> test;
    test.assign(mesh->GetPolygonVertices(), mesh->GetPolygonVertices() + icount);*/

    //const uint32_t* indices = const_cast<uint32_t*>(g_GeomIndices);
    int indexCount = indices.size();// ARRAYSIZE(g_GeomIndices); //mesh->GetPolygonVertexCount();
    g_indexCount = indexCount;
    
    /*int vertexCount  = ARRAYSIZE(g_GeomVerts);
    std::vector<VertexData> vertices(g_GeomVerts, std::end(g_GeomVerts));*/

    //int indexCount = ARRAYSIZE(g_GeomIndices);
    //constexpr const uint32_t* indices = const_cast<uint32_t*>(g_GeomIndices);
    
    // load and compile shaders
    CComPtr<ID3DBlob> vsc, psc;
    HRESULT           hr = S_OK;

    CComPtr<ID3DBlob> errors;

    UINT shaderCompilerFlags = 0;
#if (defined DEBUG || defined _DEBUG)
    shaderCompilerFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    hr = D3DCompileFromFile(L"Data/Shaders/shader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", shaderCompilerFlags, 0,
                            &vsc, &errors.p);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed compile vertex shader. %s %s", (GetHResultMessage(hr)).c_str(), static_cast<char*>(errors->GetBufferPointer()));
        return hr;
    }

    hr = D3DCompileFromFile(L"Data/Shaders/shader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", shaderCompilerFlags, 0,
                            &psc, &errors.p);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed compile pixel shader. %s %s", GetHResultMessage(hr).c_str(), static_cast<char*>(errors->GetBufferPointer()));
        return hr;
    }

    hr = g_Device->CreateVertexShader(vsc->GetBufferPointer(), vsc->GetBufferSize(), nullptr, &g_VS);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create vertex shader. %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    hr = g_Device->CreatePixelShader(psc->GetBufferPointer(), psc->GetBufferSize(), nullptr, &g_PS);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create pixel shader. %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    CD3D11_BUFFER_DESC perFrameBufferDesc = {};
    perFrameBufferDesc.ByteWidth = sizeof(FrameConstantBufferData);
    perFrameBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    hr = g_Device->CreateBuffer(&perFrameBufferDesc, nullptr, &g_PerFrameConstantBuffer);

    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create per frame constant buffer. %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    // Create geom vertex buffer
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC; // write access access by CPU and GPU
    bd.ByteWidth = vertexCount * sizeof(VertexData);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; // use as a vertex buffer
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // allow CPU to write in buffer

    hr = g_Device->CreateBuffer(&bd, nullptr, &g_SampleGeometryVertexBuffer);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create vertex buffer. %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    // store verts in the vertex buffer
    D3D11_MAPPED_SUBRESOURCE vertexBufferSubresource;
    hr = g_DeviceContext->Map(g_SampleGeometryVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, NULL, &vertexBufferSubresource);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to map vertex buffer. %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    //std::memcpy(vertexBufferSubresource.pData, vertices.data(), vertexCount * sizeof(VertexData));
    memcpy(vertexBufferSubresource.pData, vertices.data(), vertexCount * sizeof(VertexData)); // copy the data
    g_DeviceContext->Unmap(g_SampleGeometryVertexBuffer, NULL); // unmap           


    // Create index buffer
    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DYNAMIC; // write access access by CPU and GPU
    ibd.ByteWidth = indexCount * sizeof(uint32_t);
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER; // use as a vertex buffer
    ibd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // allow CPU to write in buffer

    hr = g_Device->CreateBuffer(&ibd, nullptr, &g_SampleGeometryIndexBuffer);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create index buffer. %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    // store indices in the buffer
    D3D11_MAPPED_SUBRESOURCE indicesRes;
    hr = g_DeviceContext->Map(g_SampleGeometryIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, NULL, &indicesRes);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to map index buffer. %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    memcpy(indicesRes.pData, indices.data(), indexCount * sizeof(uint32_t)); // copy the data
    g_DeviceContext->Unmap(g_SampleGeometryIndexBuffer, NULL); // unmap           

    // Create vertex layout. This is somewhat unrelated to geometry
    D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    hr = g_Device->CreateInputLayout(vertexLayoutDesc, ARRAYSIZE(vertexLayoutDesc),
                                     vsc->GetBufferPointer(), vsc->GetBufferSize(), &g_VertexLayout);
    if (FAILED(hr))
    {
        DEBUG_LOG_FORMAT("Failed to create vertex layout. %s", GetHResultMessage(hr).c_str());
        return hr;
    }

    return hr;
}

void TickSample()
{
    static auto timePrevFrame = std::chrono::high_resolution_clock::now();
    const auto  timeThisFrame = std::chrono::high_resolution_clock::now();
    const float deltaTime = std::chrono::duration<float>(timeThisFrame - timePrevFrame).count();
    timePrevFrame = timeThisFrame;

    static float           spinAngle = 0;
    static constexpr float spinSpeed = 10.0f;
    spinAngle += deltaTime * spinSpeed;
    if (spinAngle > 360.0f) { spinAngle = 0; }

    //spinAngle = 0;
    // Rotate model to face the camera
    DirectX::XMMATRIX modelMatrix = DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(180 /*+ spinAngle*/));
    //modelMatrix *= DirectX::XMMatrixTranslation(0,0,7);
    
    
    //g_ProjectionMatrix = DirectX::XMMatrixIdentity();
    //viewMatrix = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixIdentity();
    //viewMatrix *= DirectX::XMMatrixTranslation(0, 0, -3.5);
    //viewMatrix = DirectX::XMMatrixInverse(nullptr, viewMatrix);

    viewMatrix =
        DirectX::XMMatrixTranslation(0,0,-2.5f) *
            DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(1,0,0,0), DirectX::XMConvertToRadians(20)) *
            DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0,1,0,0), DirectX::XMConvertToRadians(spinAngle));


    DirectX::XMVECTOR vWorldSpaceCameraPos = DirectX::XMVector4Transform(DirectX::XMVectorSet(0,0,0,1), viewMatrix);
    
    viewMatrix = DirectX::XMMatrixInverse(nullptr, viewMatrix);
    //viewMatrix = DirectX::XMMatrixLookAtLH(DirectX::XMVectorSet(0,0,-3.5f,0), DirectX::XMVectorSet(0,0,0, 0), DirectX::XMVectorSet(0,1,0,0));

    /*DirectX::XMFLOAT4X4 f;
    DirectX::XMStoreFloat4x4(&f, viewMatrix);*/

    DirectX::XMVECTOR viewDir = DirectX::XMVector4Normalize(DirectX::XMVectorNegate(vWorldSpaceCameraPos));
    DirectX::XMStoreFloat3(&g_DirectionalLightData.m_Direction, viewDir);
    
    FrameConstantBufferData cb0 = {};
    cb0.m_ModelMatrix = XMMatrixTranspose(modelMatrix);
    cb0.m_ViewMatrix = XMMatrixTranspose(viewMatrix);
    cb0.m_MvpMatrix = XMMatrixTranspose(modelMatrix * viewMatrix * g_ProjectionMatrix);
    cb0.m_DirectionalLightData = g_DirectionalLightData,
    XMStoreFloat4(&cb0.m_WorldSpaceCameraPos, vWorldSpaceCameraPos);  

    ID3D11DeviceContext* ctx = g_DeviceContext;

    ctx->UpdateSubresource(g_PerFrameConstantBuffer, 0, nullptr, &cb0, 0, 0);

    ctx->OMSetRenderTargets(1, &g_BackBufferView.p, g_DepthStencilView);

    //ctx->OMSetDepthStencilState(g_DepthStencilState, 1);
    //ctx->RSSetState(g_RasterizerState);
    
    ctx->ClearRenderTargetView(g_BackBufferView, reinterpret_cast<FLOAT*>(&g_ClearColor));

    ctx->ClearDepthStencilView(g_DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
    
    constexpr UINT vertexBufferStride = sizeof(VertexData);
    constexpr UINT vertexBufferOffset = 0;
    ctx->IASetVertexBuffers(0, 1, &g_SampleGeometryVertexBuffer.p, &vertexBufferStride, &vertexBufferOffset);

    ctx->VSSetShader(g_VS, nullptr, 0);

    ctx->VSSetConstantBuffers(0, 1, &g_PerFrameConstantBuffer.p);
    ctx->PSSetConstantBuffers(0, 1, &g_PerFrameConstantBuffer.p);
    
    ctx->PSSetShader(g_PS, nullptr, 0);
    ctx->IASetIndexBuffer(g_SampleGeometryIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(g_VertexLayout);
    ctx->DrawIndexed(g_indexCount, 0, 0);

    // draw grid

    cb0.m_ModelMatrix = DirectX::XMMatrixIdentity();
    cb0.m_MvpMatrix = XMMatrixTranspose(viewMatrix * g_ProjectionMatrix);

    ctx->UpdateSubresource(g_PerFrameConstantBuffer, 0, nullptr, &cb0, 0, 0);
    
    
    constexpr UINT gridBufferStride = sizeof(DirectX::XMFLOAT3);
    constexpr UINT gridBufferOffset = 0;

    ctx->IASetVertexBuffers(0, 1, &g_GridVertexBuffer.p, &gridBufferStride, &gridBufferOffset);
    ctx->IASetIndexBuffer(g_GridIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    
    ctx->VSSetShader(g_GridVertexShader, nullptr, 0);
    ctx->PSSetShader(g_GridPixelShader, nullptr, 0);

    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    ctx->IASetInputLayout(g_GridVertexLayout);
    
    ctx->DrawIndexed(g_GridWidth * g_GridLength * 8, 0, 0);
    
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
                DEBUG_LOG_FORMAT("Failed to set up back buffer during resize event. %s", GetHResultMessage(hr).c_str());
            }
            else { SetUpViewport(); }
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

    hr = CreateGrid();
    if (FAILED(hr))
        return hr;
    
    hr = InitSample();
    if (FAILED(hr))
        return hr;

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
