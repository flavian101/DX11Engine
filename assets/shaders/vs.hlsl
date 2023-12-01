struct VS_out
{
    float4 Pos : SV_POSITION;
    float4 color : COLOR;
};

VS_out main(float4 Pos : POSITION, float4 inColor : COLOR)
{
    VS_out output;
    output.Pos = Pos;
    output.color = inColor;
    return output;
}