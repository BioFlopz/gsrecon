
[[vk::binding(0, 0)]]
RWStructuredBuffer<uint> values;

[numthreads(4, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    values[dispatchThreadId.x] += 1;
}