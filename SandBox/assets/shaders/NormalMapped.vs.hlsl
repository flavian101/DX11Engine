cbuffer cb_vsConstantBuffer : register(b0)
{
    float4x4 WVP;
    float4x4 Model;
};

struct VS_out
{
    float4 Pos : SV_POSITION;
    float4 worldPos : POSITION;
    float2 texCoords : TEXCOORD;
    float3 normal : NORMAL;
    float4 tangents : TANGENT;
};

VS_out main(float3 Pos : POSITION, float2 inTexCoord : TEXCOORD, float3 normal : NORMAL, float4 tangents : TANGENT)
{
    VS_out output;
    
    output.Pos = mul(float4(Pos, 1.0f), WVP);
    
    output.worldPos = mul(float4(Pos, 1.0f), Model);
    
    output.normal = mul(normal, (float3x3) Model);
    output.tangents = float4(mul(tangents.xyz, (float3x3) Model), tangents.w);
    
    output.texCoords = inTexCoord;
    
    return output;
}