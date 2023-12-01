struct VS_out
{
    float4 Pos : SV_POSITION;
    float4 color : COLOR;
};

float4 main(VS_out input) : SV_Target
{
    return input.color;
    
}