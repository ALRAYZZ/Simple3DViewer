// Vertex shader - transforms vertices with a model-view-projection matrix

cbuffer Constants : register(b0)
{
    float4x4 mvpMatrix;
};

struct VSInput
{
    float3 position : POSITION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(mvpMatrix, float4(input.position, 1.0));
    return output;
}