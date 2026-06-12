// triangle.frag.slang
// Phase 1 minimal fragment shader — solid red

[shader("fragment")]
float4 main() : SV_Target0
{
    return float4(1.0, 0.0, 0.0, 1.0);
}
