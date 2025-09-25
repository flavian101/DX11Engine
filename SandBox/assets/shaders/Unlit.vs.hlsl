#include "common.hlsli"

StandardVertexOutput main(StandardVertexInput input)
{
    StandardVertexOutput output;
    
    // Use the standard vertex shader but with minimal processing
    output.position = mul(float4(input.position, 1.0), WVP);
    output.worldPos = mul(float4(input.position, 1.0), Model);
    output.normal = mul(input.normal, (float3x3) Model);
    output.texCoord = input.texCoord;
    
#if HAS_TANGENT_ATTRIBUTE
    output.tangent = float4(mul(input.tangent.xyz, (float3x3)Model), input.tangent.w);
#endif

#if HAS_VERTEX_COLOR_ATTRIBUTE
    output.color = input.color;
#endif

    // For unlit materials, we don't need view direction but initialize it anyway
    output.viewDir = CameraPosition - output.worldPos.xyz;
    output.time = Time;
    
    return output;
}