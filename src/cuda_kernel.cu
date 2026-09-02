#include <cuda_runtime.h>

__global__ void cudaKernelSmoke(int* value)
{
    if (threadIdx.x == 0)
    {
        *value = 1;
    }
}

__global__ void cudaExternalMemorySmoke(unsigned int* values)
{
    const unsigned int i = threadIdx.x;

    if (i < 4)
    {
        values[i] = i + 10;
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


extern "C" bool runCudaExternalMemorySmoke(void* mappedStorage)
{
    auto* values = static_cast<unsigned int*>(mappedStorage);

    cudaExternalMemorySmoke<<<1, 4>>>(values);

    if (cudaGetLastError() != cudaSuccess)
    {
        return false;
    }

    if (cudaDeviceSynchronize() != cudaSuccess)
    {
        return false;
    }

    unsigned int hostValues[4]{};

    if (cudaMemcpy(hostValues, values, sizeof(hostValues), cudaMemcpyDeviceToHost) != cudaSuccess)
    {
        return false;
    }

    return
        hostValues[0] == 10 &&
        hostValues[1] == 11 &&
        hostValues[2] == 12 &&
        hostValues[3] == 13;
}

extern "C" bool runCudaExternalMemoryWriteAsync(void* mappedStorage)
{
    auto* values = static_cast<unsigned int*>(mappedStorage);

    cudaExternalMemorySmoke<<<1, 4>>>(values);

    return cudaGetLastError() == cudaSuccess;
}