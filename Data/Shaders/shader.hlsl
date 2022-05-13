
struct DirectionalLightData
{
    float4 color;
    float3 direction;
    float attenuation;
};

cbuffer PerFrameBuffer : register(b0)
{
    matrix modelMatrix;
    matrix mvpMatrix;
    DirectionalLightData directionalLightData;
}

struct VSInput
{
    float3 position : POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
};

PSInput VSMain(VSInput IN)
{
    PSInput OUT;

    float4 temp = float4( IN.position, 1);
    OUT.position = mul(temp, mvpMatrix);

    //float4 tempNormal = float4(IN.normal, 0);
   // tempNormal = mul(tempNormal, modelMatrix);
    OUT.normal = mul(IN.normal, modelMatrix);
    OUT.color = IN.color;
    
    return OUT;
}

float4 PSMain(PSInput IN) : SV_TARGET
{
    float3 surfaceColor = IN.color;
    float3 normal = normalize(IN.normal); 
    float3 lightDirection = -normalize(directionalLightData.direction);
    float NDotL = max(0.0,dot(normal,lightDirection));
    float halfLambertDiffuse = pow(NDotL * 0.5 + 0.5,2.0) * surfaceColor;
    float3 finalColor = halfLambertDiffuse * directionalLightData.attenuation * directionalLightData.color;

    return float4 (finalColor, 1);
}