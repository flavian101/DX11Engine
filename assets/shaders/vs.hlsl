cbuffer cb_vsConstantBuffer
{
    float4x4 WVP;
    float4x4 World;
};

    struct VS_out
{
    float4 Pos : SV_POSITION;
    float4 color : COLOR;
};

VS_out main(float4 Pos : POSITION, float4 inColor : COLOR)
{
    VS_out output;
    output.Pos = mul(Pos, WVP);
    output.color = inColor;
    return output;
}