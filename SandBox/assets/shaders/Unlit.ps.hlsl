#include "common.hlsli"

float4 main(StandardVertexOutput input) : SV_Target
{
    float4 baseColor = diffuseColor;
    
    // Sample diffuse texture if available
#if HAS_DIFFUSE_TEXTURE
    float4 texColor = SampleDiffuseTexture(input.texCoord);
    baseColor *= texColor;
#endif
    
    // Modulate with vertex colors if available
#if HAS_VERTEX_COLOR_ATTRIBUTE
    baseColor *= input.color;
#endif
    
    // Add emissive contribution
#if HAS_EMISSIVE_MAP || ENABLE_EMISSIVE
    float3 emissiveSample = SampleEmissiveMap(input.texCoord);
    baseColor.rgb += emissiveColor.rgb * emissiveSample;
#endif
    
    // Apply material alpha
    baseColor.a *= alpha;
    
    return baseColor;
}