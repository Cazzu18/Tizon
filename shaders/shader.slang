// we will be taking input from a vertex buffer using the structs
//The inPosition and inColor variables are vertex attributes. 
//They’re properties that are specified per-vertex in the vertex buffer 
struct VSInput {
    float2 inPosition;
    float3 inColor;
}

struct VSOutput {
    float3 color;
    float4 pos : SV_Position;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = float4(input.inPosition, 0.0, 1.0);
    output.color = input.inColor;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_Target {
    return float4(vertIn.color, 1.0);
}