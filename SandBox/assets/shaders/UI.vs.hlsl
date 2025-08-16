
cbuffer MaterialBuffer : register(b1)
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
    float3 materialPadding;
};
cbuffer UIConstantBuffer : register(b5)
{
    matrix projection;
    float screenWidth;
    float screenHeight;
    float time;
    float padding;
};
struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};


struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    // Transform position using the WVP matrix (which includes UI projection)
    output.position = mul(float4(input.position, 1.0f), projection);
    
    // Pass through texture coordinates with tiling and offset
    output.texCoord = input.texCoord * textureScale + textureOffset;
      
    return output;
}
