cbuffer MaterialBufferData : register(b1)
{
    float4 diffuseColor;
    float metallic;
    float roughness;
    float alpha;
    float2 textureScale;
    float2 textureOffset;
    uint flags;
    float3 padding;
};

struct PS_input
{
    float4 pos : SV_Position;
    float2 texCoords : TEXCOORD;
};

Texture2D diffuseTexture : register(t0);
SamplerState textureSampler : register(s0);

float4 main( PS_input input) : SV_Target
{
    float4 baseColor = diffuseColor;
    
    if(flags & 1)
    {
        float2 scaledUV = input.texCoords * textureScale + textureOffset;
        baseColor = diffuseTexture.Sample(textureSampler, scaledUV);
        
        //modulate with material color
        baseColor *= diffuseColor;
    }
    baseColor.a *= alpha;
    
    return baseColor;
}