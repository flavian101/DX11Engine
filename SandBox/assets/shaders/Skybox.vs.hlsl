cbuffer cb_vsConstantBuffer : register(b0)
{
    float4x4 WVP;
    float4x4 Model;
};

struct VS_out
{
    float4 Pos : SV_POSITION;
    float3 texCoords : TEXCOORD;
};

VS_out main(float3 Pos : POSITION, float3 normal : NORMAL, float2 inTexCoord : TEXCOORD)
{
    VS_out output;
    
    // Transform vertex to clip space
    output.Pos = mul(float4(Pos, 1.0f), WVP);
    
    // Use local position as texture coordinates for cubemap sampling
    output.texCoords = Pos;
    
    // Ensure skybox is always rendered at far plane
    output.Pos.z = output.Pos.w;
    
    return output;
}