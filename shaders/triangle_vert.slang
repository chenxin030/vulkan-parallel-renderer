// triangle.vert.slang
// Phase 1 minimal vertex shader — hardcoded triangle from SV_VertexID

[shader("vertex")]
float4 main(uint vertexID : SV_VertexID) : SV_Position
{
    float2 positions[3] = {
        float2(0.0, -0.5),
        float2(0.5, 0.5),
        float2(-0.5, 0.5)
    };
    return float4(positions[vertexID], 0.0, 1.0);
}
