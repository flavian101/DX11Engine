cbuffer cb_vsConstantBuffer : register(b0)
{
    float4x4 WVP;
    float4x4 Model;
}
struct VS_out
{
    float4 pos : SV_Position;
    float2 texCoords : TEXCOORD;
};
VS_out main(float3 Pos : POSITION, float2 inTexCoord : TEXCOORD, float3 normal : NORMAL, float4 tangents : TANGENT)
{
    VS_out output;
    
    output.pos = mul(float4(Pos, 1.0f), WVP);
    output.texCoords = inTexCoord;
    return output;
}