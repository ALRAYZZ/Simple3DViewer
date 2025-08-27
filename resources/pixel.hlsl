// Pixel shader - improved lighting with proper normal handling

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldNormal : NORMAL;
    float3 worldPos : WORLD_POS;
};

float4 main(PSInput input) : SV_Target
{
    // Normalize the interpolated normal (very important!)
    float3 normal = normalize(input.worldNormal);
    
    // Check if normal is valid (not zero vector)
    if (length(input.worldNormal) < 0.001)
    {
        normal = float3(0.0, 1.0, 0.0); // Default up vector
    }
    
    // Light direction (pointing towards the light)
    float3 lightDir = normalize(float3(1.0, 1.0, 1.0));
    
    // Calculate diffuse lighting
    float diffuse = max(dot(normal, lightDir), 0.0);
    
    // Add some ambient lighting to prevent complete darkness
    float ambient = 0.3;
    
    // Combine lighting
    float lightIntensity = ambient + diffuse * 0.7;
    
    // Base color
    float3 baseColor = float3(0.8, 0.8, 0.8);
    
    // Final color with some debug visualization
    float3 finalColor = baseColor * lightIntensity;
    
    // DEBUG: Show face orientation - front faces brighter
    float facing = dot(normal, normalize(float3(0.0, 0.0, 1.0)));
    if (facing < 0.0)
    {
        finalColor *= 0.5; // Darken back faces
    }
    
    return float4(finalColor, 1.0);
}