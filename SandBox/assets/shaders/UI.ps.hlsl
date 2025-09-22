#include "common.hlsli"

float4 main(UIVertexOutput input) : SV_Target
{
    float4 finalColor = diffuseColor;
    
    // Sample diffuse texture if available
    if (flags & HAS_DIFFUSE_TEXTURE)
    {
        float4 texColor = diffuseTexture.Sample(standardSampler, input.texCoord);
        finalColor *= texColor;
    }
    
    // Apply material alpha
    finalColor.a *= alpha;
    
    // UI elements don't need complex lighting, just return the color
    return saturate(finalColor);
}