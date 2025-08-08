cbuffer MaterialBufferData : register(b1)
{
    float4 diffuseColor;
    float4 specularColor;
    float4 emissiveColor;
    float shininess;
    float metallic;
    float roughness;
    float alpha;
    float2 textureScale;
    float2 textureOffset;
    uint flags;
    float padding;
};

struct PS_input
{
    float4 Pos : SV_POSITION;
    float3 texCoords : TEXCOORD;
};

TextureCube environmentTexture : register(t4);
SamplerState textureSampler : register(s0);

float4 main(PS_input input) : SV_Target
{
    // Sample the cubemap using the 3D texture coordinates
    float4 skyColor = environmentTexture.Sample(textureSampler, input.texCoords);
    
    // Modulate with material color (useful for tinting the sky)
    skyColor *= diffuseColor;
    
    // Add any emissive contribution (useful for glowing skies)
    skyColor.rgb += emissiveColor.rgb;
    
    return float4(skyColor.rgb, 1.0f); // Skybox is always opaque
}