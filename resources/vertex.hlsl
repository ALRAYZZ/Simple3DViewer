// Vertex shader - transforms vertices with proper matrix handling

cbuffer Constants : register(b0)
{
    float4x4 mvpMatrix;
    float4x4 modelMatrix;
    float4x4 normalMatrix;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldNormal : NORMAL;
    float3 worldPos : WORLD_POS;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    // Transform position to world space
    float4 worldPos = mul(modelMatrix, float4(input.position, 1.0));
    output.worldPos = worldPos.xyz;

    // Transform position to clip space
    output.position = mul(mvpMatrix, float4(input.position, 1.0));
    
    // Transform normal to world space using normal matrix
    output.worldNormal = normalize(mul((float3x3) normalMatrix, input.normal));
    
    return output;
}