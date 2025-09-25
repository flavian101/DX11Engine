#include "common.hlsli"

struct SkyboxVertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct SkyboxVertexOutput
{
    float4 position : SV_POSITION;
    float3 texCoord : TEXCOORD;
};

SkyboxVertexOutput main(SkyboxVertexInput input)
{
    SkyboxVertexOutput output;
    
    // Transform vertex to clip space
    output.position = mul(float4(input.position, 1.0f), WVP);
    
    // Use local position as texture coordinates for cubemap sampling
    output.texCoord = input.position;
    
    // Ensure skybox is always rendered at far plane
    output.position.z = output.position.w;
    
    return output;
}