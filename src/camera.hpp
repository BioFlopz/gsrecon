#pragma once

#include <cstddef>

struct alignas(16) CameraGpuData
{
    float view[16];
    float projection[16];

    float viewportSize[2];
    float padding[2];
};

static_assert(sizeof(CameraGpuData) == 144);
static_assert(alignof(CameraGpuData) == 16);

static_assert(offsetof(CameraGpuData, view) == 0);
static_assert(offsetof(CameraGpuData, projection) == 64);
static_assert(offsetof(CameraGpuData, viewportSize) == 128);