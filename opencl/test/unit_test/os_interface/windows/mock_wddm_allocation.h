/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/test/unit_test/mock_gdi/mock_gdi.h"

#include "opencl/test/unit_test/mocks/mock_gmm.h"

namespace NEO {

class MockWddmAllocation : public WddmAllocation {
  public:
    MockWddmAllocation() : MockWddmAllocation(EngineLimits::maxHandleCount) {}
    MockWddmAllocation(uint32_t numGmms) : WddmAllocation(0, numGmms, GraphicsAllocation::AllocationType::UNKNOWN,
                                                          nullptr, 0, nullptr, MemoryPool::MemoryNull, 0u, 3u),
                                           gpuPtr(gpuAddress), handle(handles[0]) {
        for (uint32_t i = 0; i < numGmms; i++) {
            setGmm(new MockGmm, i);
            setHandle(ALLOCATION_HANDLE, i);
        }
    }
    void clearGmms() {
        for (uint32_t i = 0; i < getNumGmms(); i++) {
            delete getGmm(i);
        }
        gmms.resize(0);
    }
    ~MockWddmAllocation() {
        clearGmms();
    }
    void resizeGmms(size_t newSize) {
        clearGmms();
        gmms.resize(newSize);
        handles.resize(newSize);
    }
    using WddmAllocation::cpuPtr;
    using WddmAllocation::handles;
    using WddmAllocation::memoryPool;
    using WddmAllocation::size;

    D3DGPU_VIRTUAL_ADDRESS &gpuPtr;
    D3DKMT_HANDLE &handle;
};

} // namespace NEO
