#pragma once

#include <cstddef>
#include <type_traits>

struct alignas(16) GaussianGpuData
{
    float position[3];
    float opacity;

    float scale[3];
    float padding0;

    // Quaternion order: w, x, y, z.
    float rotation[4];

    float color[3];
    float padding1;
};

static_assert(std::is_standard_layout_v<GaussianGpuData>);
static_assert(std::is_trivially_copyable_v<GaussianGpuData>);

static_assert(sizeof(GaussianGpuData) == 64);
static_assert(alignof(GaussianGpuData) == 16);

static_assert(offsetof(GaussianGpuData, position) == 0);
static_assert(offsetof(GaussianGpuData, opacity) == 12);
static_assert(offsetof(GaussianGpuData, scale) == 16);
static_assert(offsetof(GaussianGpuData, rotation) == 32);
static_assert(offsetof(GaussianGpuData, color) == 48);