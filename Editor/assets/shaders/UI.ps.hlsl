#include "common.hlsli"

float4 main(UIVertexOutput input) : SV_Target
{
    float4 finalColor = diffuseColor;
    
    // Sample diffuse texture if available and we have texture coordinates
#if HAS_DIFFUSE_TEXTURE && HAS_TEXCOORDS_ATTRIBUTE
    float4 texColor = diffuseTexture.Sample(standardSampler, input.texCoord);
    finalColor *= texColor;
#endif
    
    // UI elements can have vertex colors too
#if HAS_VERTEX_COLOR_ATTRIBUTE
    finalColor *= input.color;
#endif
    
    // Apply material alpha
    finalColor.a *= alpha;
    
    // Alpha test for UI elements (useful for text rendering)
#if ENABLE_ALPHA_TEST
    clip(finalColor.a - 0.1); // Lower threshold for UI
#endif
    
    // UI elements don't need complex lighting, just return the color
    return saturate(finalColor);
}