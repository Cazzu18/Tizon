// we will be taking input from a vertex buffer using the structs
//The inPosition and inColor variables are vertex attributes. 
//They’re properties that are specified per-vertex in the vertex buffer 
struct VSInput {
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
}

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

//built-in language feature used to pass immutable parameter data from a host application to GPU code.
ConstantBuffer<UniformBuffer> ubo;

struct VSOutput {
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
};

// A sampler represents a combined image sampler descriptor in Slang
// There are equivalent sampler1D and sampler3D types for other types of images.
Sampler2D texture;

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    //this is actually multiplying position by model matrix first, then view, then projections
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_Target {
    return float4(vertIn.fragColor * texture.Sample(vertIn.fragTexCoord).rgb, 1.0);
}