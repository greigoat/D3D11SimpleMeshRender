#include "shared.hlsli"

struct VSInput
{
    float3 position : POSITION;
};

struct PSInput
{
    float4 position : SV_POSITION;
};

PSInput VSMain(VSInput IN)
{
    PSInput OUT;

    float4 temp = float4( IN.position, 1);
    OUT.position = mul(temp, wvpMatrix);
    
    return OUT;
}

float4 PSMain(PSInput IN) : SV_TARGET
{
    return float4(0.35f,0.35f,0.35f,1);
}