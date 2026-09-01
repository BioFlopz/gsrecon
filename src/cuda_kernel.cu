#include <cuda_runtime.h>

__global__ void cudaKernelSmoke(int* value)
{
    if (threadIdx.x == 0)
    {
        *value = 1;
    }
}

extern "C" bool runCudaKernelSmoke()
{
    int* deviceValue = nullptr;

    if (cudaMalloc(&deviceValue, sizeof(int)) != cudaSuccess)
    {
        return false;
    }

    cudaKernelSmoke<<<1, 1>>>(deviceValue);

    if (cudaGetLastError() != cudaSuccess)
    {
        cudaFree(deviceValue);
        return false;
    }

    if (cudaDeviceSynchronize() != cudaSuccess)
    {
        cudaFree(deviceValue);
        return false;
    }

    int hostValue = 0;

    const cudaError_t copyResult = cudaMemcpy(&hostValue, deviceValue, sizeof(int), cudaMemcpyDeviceToHost);

    cudaFree(deviceValue);

    return copyResult == cudaSuccess && hostValue == 1;
}