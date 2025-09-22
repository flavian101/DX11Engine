#include "common.hlsli"

struct SkyboxPixelInput
{
    float4 position : SV_POSITION;
    float3 texCoord : TEXCOORD;
};

float4 main(SkyboxPixelInput input) : SV_Target
{
    // Sample the cubemap using the 3D texture coordinates
    float4 skyColor = environmentTexture.Sample(standardSampler, input.texCoord);
    
    // Modulate with material color (useful for tinting)
    skyColor *= diffuseColor;
    
    // Add emissive contribution (useful for glowing skies)
    skyColor.rgb += emissiveColor.rgb;
    
    // Apply exposure and gamma for HDR skyboxes
    skyColor.rgb = ToneMapExposure(skyColor.rgb, exposure * iblIntensity);
    skyColor.rgb = ApplyGamma(skyColor.rgb, gamma);
    
    return float4(skyColor.rgb, 1.0); // Skybox is always opaque
}