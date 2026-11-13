#include "common.hlsli"


UIVertexOutput main(UIVertexInput input)
{
    UIVertexOutput output;
    
    // Transform using UI projection matrix (position always required)
    output.position = mul(float4(input.position, 1.0), uiProjection);
    
    // Pass through texture coordinates if available
#if HAS_TEXCOORDS_ATTRIBUTE
    // Apply texture scaling and offset
    output.texCoord = input.texCoord * textureScale + textureOffset;
#endif

    // Pass through vertex color if available
#if HAS_VERTEX_COLOR_ATTRIBUTE
    output.color = input.color;
#endif
    
    return output;
}