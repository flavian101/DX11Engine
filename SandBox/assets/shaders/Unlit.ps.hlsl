#include "common.hlsli"

float4 main(StandardVertexOutput input) : SV_Target
{
    float4 baseColor = diffuseColor;
    
    // Sample diffuse texture if available
#if HAS_DIFFUSE_TEXTURE && HAS_TEXCOORDS_ATTRIBUTE
    float4 texColor = SampleDiffuseTexture(input.texCoord);
    baseColor *= texColor;
#endif
    
    // Modulate with vertex colors if available
#if HAS_VERTEX_COLOR_ATTRIBUTE
    baseColor *= input.color;
#endif
    
    // Add emissive contribution if available
#if (HAS_EMISSIVE_MAP && HAS_TEXCOORDS_ATTRIBUTE) || ENABLE_EMISSIVE
    float3 emissiveSample = float3(0.0, 0.0, 0.0);
    
#if HAS_EMISSIVE_MAP && HAS_TEXCOORDS_ATTRIBUTE
    emissiveSample = SampleEmissiveMap(input.texCoord);
#endif
    
    baseColor.rgb += emissiveColor.rgb * emissiveSample;
#endif
    
    // Apply material alpha
    baseColor.a *= alpha;
    
#if ENABLE_ALPHA_TEST
    clip(baseColor.a - 0.5);
#endif
    
    return baseColor;
}