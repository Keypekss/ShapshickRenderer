struct VSInput
{
    float4 position : POSITION;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VS(VSInput input)
{
    PSInput result;

    result.position = input.position;
    result.color = input.color;

    return result;
}

float4 PS(PSInput input) : SV_TARGET
{
    return input.color;
}




