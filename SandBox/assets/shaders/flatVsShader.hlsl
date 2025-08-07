cbuffer cb_vsConstantBuffer : register(b0)
{
    float4x4 WVP;
    float4x4 Model;
};

float4 main(float4 Pos : POSITION, float2 inTexCoord : TEXCOORD, float3 normal : NORMAL,float4 tangents : TANGENT) : SV_POSITION
{
    return mul(Pos, WVP);
}