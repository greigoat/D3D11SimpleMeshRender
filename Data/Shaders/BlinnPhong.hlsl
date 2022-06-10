#include "shared.hlsli"

static const float specularGlossiness = 50.0f;
static const float specularPower = 0.25f;

struct VSInput
{
    float3 position : POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 viewDir : TEXCOORD2;
    float3 color : COLOR;
    float3 normal : NORMAL;
};

PSInput VSMain(VSInput IN)
{
    PSInput OUT;

    float4 temp = float4( IN.position, 1);
    OUT.position = mul(temp, wvpMatrix);
    
    float3 worldPosition = mul(temp, worldMatrix).xyz;
    OUT.viewDir = normalize(worldSpaceCameraPos.xyz - worldPosition);
    
    OUT.normal = mul(IN.normal, worldMatrix);
    OUT.color = IN.color;
    
    return OUT;
}

float4 PSMain(PSInput IN) : SV_TARGET
{
    float3 diffuseColor = IN.color;
    float3 normalDirection = normalize(IN.normal); 
    float3 lightDirection = -directionalLightData.direction;
    
    float NDotL = saturate(dot(normalDirection,lightDirection));
    //NDotL = pow(NDotL * 0.5 + 0.5,2.0); // uncomment for half-lambert
    
    float3 viewDirection = IN.viewDir;
    float3 halfDirection = normalize(viewDirection+lightDirection);
    
    float NDotV = max(0, dot( normalDirection, halfDirection ));
    
    float3 specularity = pow(NDotV, specularGlossiness) * specularPower;
    float lightingModel = NDotL * diffuseColor + specularity;

    float attenuation= directionalLightData.attenuation;
    float3 attenuationColor = attenuation * directionalLightData.color.rgb;
    float3 finalDiffuse = float4(lightingModel * attenuationColor,1);
    
    return float4 (finalDiffuse, 1);
}