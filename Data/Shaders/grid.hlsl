#include "shared.hlsli"

struct VSInput
{
    float3 position : POSITION;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 fogParams : FOGPARAMS;
};

PSInput VSMain(VSInput IN)
{
    PSInput OUT;

    float4 temp = float4( IN.position, 1);
    OUT.position = mul(temp, wvpMatrix);

    float4 worldPos = mul(temp, worldMatrix);
    float distToCam = distance(worldPos, worldSpaceCameraPos);
    OUT.fogParams.x = saturate((distToCam - fogRange.x) / (fogRange.y - fogRange.x));
    
    return OUT;
}

float4 PSMain(PSInput IN) : SV_TARGET
{
    float4 col = float4(0.35f,0.35f,0.35f,1);
    
    col.rgb = lerp(col.rgb, fogColor.rgb, IN.fogParams.x);

    return col;
}