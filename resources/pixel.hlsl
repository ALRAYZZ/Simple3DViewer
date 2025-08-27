// Pixel shader - improved lighting with proper normal handling

cbuffer Constants : register(b0)
{
    float4x4 mvpMatrix;
    float4x4 modelMatrix;
    float4x4 normalMatrix;
    float3 lightDirection;
    float padding;
}

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldNormal : NORMAL;
    float3 worldPos : WORLD_POS;
};

float SmoothShadow(float NdotL, float shadowHardness)
{
    // Shadow hardness = 0 = very soft, 1 = very hard
    return smoothstep(-shadowHardness, shadowHardness, NdotL);
}

float3 CalculateLighting(float3 normal, float3 worldPos, float3 mainLightDir)
{
    // Main directional light
    float NdotL = dot(normal, mainLightDir);
    float mainDiffuse = SmoothShadow(NdotL, 0.2) * 0.7;
    
    // Fill light
    float3 fillLightDir = normalize(float3(-0.5, 0.5, -0.5));
    float fillNdotL = dot(normal, fillLightDir);
    float fillDiffuse = SmoothShadow(fillNdotL, 0.3) * 0.3;
    
    // Rim light
    float3 viewDir = normalize(-worldPos); // Assuming camera at origin
    float rim = 1.0 - max(dot(normal, viewDir), 0.0);
    rim = pow(rim, 2.0) * 0.2;
    
    return mainDiffuse + fillDiffuse + rim;
}

float4 main(PSInput input) : SV_Target
{
    // Normalize the interpolated normal (very important!)
    float3 normal = normalize(input.worldNormal);
    
    // Check if normal is valid (not zero vector)
    if (length(input.worldNormal) < 0.001)
    {
        normal = float3(0.0, 1.0, 0.0); // Default up vector
    }
    
    float3 mainLightDir = normalize(lightDirection);
    
    // Light properties
    float3 lighting = CalculateLighting(normal, input.worldPos, mainLightDir);
   
    // Ambient light
    float ambient = 0.15;
    
    // Final light calc
    float totalLight = saturate(ambient + lighting);
    
    // Base color
    float3 baseColor = float3(0.8, 0.8, 0.8);
    
    // Final color with some debug visualization
    float3 finalColor = baseColor * totalLight;
    
    return float4(finalColor, 1.0);
}