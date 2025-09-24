#include "common.hlsli"

StandardVertexOutput main(StandardVertexInput input)
{
    StandardVertexOutput output;
    output.position = mul(float4(input.position, 1.0), WVP);
    output.texCoord = input.texCoord;
    return output;
}