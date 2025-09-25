#include "common.hlsli"

float4 main(UIVertexOutput input) : SV_Target
{
    float4 finalColor = diffuseColor;
    
    // Sample diffuse texture if available
#if HAS_DIFFUSE_TEXTURE
        float4 texColor = diffuseTexture.Sample(standardSampler, input.texCoord);
        finalColor *= texColor;
#endif
    
    // UI elements can have vertex colors too
#if HAS_VERTEX_COLOR_ATTRIBUTE
    finalColor *= input.color;
#endif
    
    // Apply material alpha
    finalColor.a *= alpha;
    
    // UI elements don't need complex lighting, just return the color
    return saturate(finalColor);
}