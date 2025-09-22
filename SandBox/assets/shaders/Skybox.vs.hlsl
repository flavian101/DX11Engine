#include "common.hlsli"

struct SkyboxVertexOutput
{
    float4 position : SV_POSITION;
    float3 texCoord : TEXCOORD;
};

SkyboxVertexOutput main(float3 Pos : POSITION, float3 normal : NORMAL, float2 inTexCoord : TEXCOORD)
{
    SkyboxVertexOutput output;
    
    // Transform vertex to clip space
    output.position = mul(float4(Pos, 1.0f), WVP);
    
    // Use local position as texture coordinates for cubemap sampling
    output.texCoord = Pos;
    
    // Ensure skybox is always rendered at far plane
    output.position.z = output.position.w;
    
    return output;
}