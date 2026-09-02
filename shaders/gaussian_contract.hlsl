struct GaussianGpuData
{
    float3 position;
    float opacity;

    float3 scale;
    float padding0;

    float4 rotation;

    float3 color;
    float padding1;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<GaussianGpuData> gaussians;

[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x == 0)
    {
        gaussians[0].position.x += 1.0f;
    }
}