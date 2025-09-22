#include "common.hlsli"

UIVertexOutput main(UIVertexInput input)
{
    UIVertexOutput output;
    
    // Transform using UI projection matrix
    output.position = mul(float4(input.position, 1.0), uiProjection);
    
    // Apply texture scaling and offset
    output.texCoord = input.texCoord * textureScale + textureOffset;
    
    return output;
}
