
struct DirectionalLightData
{
    float4 color;
    float3 direction;
    float attenuation;
};

cbuffer PerFrameBuffer : register(b0)
{
    matrix worldMatrix;
    matrix viewMatrix;
    matrix wvpMatrix;
    float4 worldSpaceCameraPos;
    DirectionalLightData directionalLightData;
}