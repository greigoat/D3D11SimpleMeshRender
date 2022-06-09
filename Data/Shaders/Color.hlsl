#include "shared.hlsli"

struct VSInput
{
    float3 position : POSITION;
    float3 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
};

PSInput VSMain(VSInput IN)
{
    PSInput OUT;

    float4 temp = float4( IN.position, 1);
    OUT.position = mul(temp, wvpMatrix);
    OUT.color = IN.color;
    
    return OUT;
}

float4 PSMain(PSInput IN) : SV_TARGET
{
    return float4(IN.color,1);
}