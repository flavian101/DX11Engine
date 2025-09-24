#include "common.hlsli"

struct UnlitPixelInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

float4 main(UnlitPixelInput input) : SV_Target
{
    float4 baseColor = diffuseColor;
    
    if (flags & HAS_DIFFUSE_TEXTURE_FLAG)
    {
        float2 scaledUV = input.texCoord * textureScale + textureOffset;
        float4 texColor = diffuseTexture.Sample(standardSampler, scaledUV);
        baseColor *= texColor;
    }
    
    // Add emissive contribution
    float3 emissiveSample = SampleEmissiveMap(input.texCoord);
    baseColor.rgb += emissiveColor.rgb * emissiveSample;
    
    baseColor.a *= alpha;
    
    return baseColor;
}