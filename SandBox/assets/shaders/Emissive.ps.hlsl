#include "common.hlsli"

float4 main(StandardVertexOutput input) : SV_Target
{
    // Sample base texture
    float4 diffuseSample = SampleDiffuseTexture(input.texCoord);
    float3 emissiveSample = SampleEmissiveMap(input.texCoord);
    
    // Base color
    float3 baseColor = diffuseColor.rgb * diffuseSample.rgb;
    
    // Enhanced emissive effects
    float3 finalEmissive = emissiveColor.rgb * emissiveSample;
    
    // Time-based animation using world position for deterministic effects
    float timeValue = input.worldPos.x + input.worldPos.y + input.worldPos.z + input.time;
    
    // Pulsing effect controlled by shininess
    if (flags & 0x100) // Custom flag for pulse enable
    {
        float pulse = sin(timeValue * shininess * 0.1) * 0.5 + 0.5;
        finalEmissive *= (0.5 + pulse * 0.5);
    }
    
    // Distance-based effects
    if (flags & 0x200) // Custom flag for distance effects
    {
        float distance = length(input.worldPos.xyz - CameraPosition);
        float falloff = 1.0 / (1.0 + distance * 0.01);
        finalEmissive *= falloff;
    }
    
    // Rim lighting effect
    float3 V = normalize(input.viewDir);
    float3 N = normalize(input.normal);
    float rimDot = 1.0 - saturate(dot(V, N));
    float rimPower = lerp(1.0, 8.0, roughness);
    float rimEffect = pow(rimDot, rimPower);
    float3 rimContribution = specularColor.rgb * rimEffect * metallic;
    
    // Combine effects
    float3 finalColor = baseColor * finalEmissive + rimContribution;
    
    // Intensity boost using metallic value
    finalColor *= (1.0 + metallic);
    
    // Simple tone mapping for emissive materials
    finalColor = ToneMapReinhard(finalColor);
    
    return float4(finalColor, diffuseSample.a * alpha);
}