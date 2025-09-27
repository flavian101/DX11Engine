#include "common.hlsli"

StandardVertexOutput main(StandardVertexInput input)
{
    StandardVertexOutput output;
    
    // Transform position (always required)
    output.position = mul(float4(input.position, 1.0), WVP);
    output.worldPos = mul(float4(input.position, 1.0), Model);
    
    // Transform normal only if we have normal attribute
#if HAS_NORMAL_ATTRIBUTE
    output.normal = mul(input.normal, (float3x3)Model);
#endif
    
    // Pass through texture coordinates if available
#if HAS_TEXCOORDS_ATTRIBUTE
    output.texCoord = input.texCoord;
#endif
    
    // Transform tangent if available
#if HAS_TANGENT_ATTRIBUTE
    output.tangent = float4(mul(input.tangent.xyz, (float3x3)Model), input.tangent.w);
#endif

    // Pass through vertex color if available
#if HAS_VERTEX_COLOR_ATTRIBUTE
    output.color = input.color;
#endif

    // Calculate view direction and time (always available)
    output.viewDir = CameraPosition - output.worldPos.xyz;
    output.time = Time;
    
    return output;
}