/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_operations_handler.h"

#include <memory>

namespace NEO {

class Wddm;
class WddmResidentAllocationsContainer;

class WddmMemoryOperationsHandler : public MemoryOperationsHandler {
  public:
    WddmMemoryOperationsHandler(Wddm *wddm);
    ~WddmMemoryOperationsHandler() override;

    MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) override;
    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override;
    MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) override;

  protected:
    Wddm *wddm;
    std::unique_ptr<WddmResidentAllocationsContainer> residentAllocations;
};
} // namespace NEO
