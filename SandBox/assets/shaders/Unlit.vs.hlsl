#include "common.hlsli"

struct UnlitVertexOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

UnlitVertexOutput main(StandardVertexInput input)
{
    UnlitVertexOutput output;
    output.position = mul(float4(input.position, 1.0), WVP);
    output.texCoord = input.texCoord;
    return output;
}