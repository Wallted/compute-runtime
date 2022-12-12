/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {
bool CompressionSelector::preferCompressedAllocation(const AllocationProperties &properties, const HardwareInfo &hwInfo) {
    switch (properties.allocationType) {
    case AllocationType::GLOBAL_SURFACE:
    case AllocationType::CONSTANT_SURFACE:
    case AllocationType::SVM_GPU:
    case AllocationType::PRINTF_SURFACE: {
        const auto &productHelper = *ProductHelper::get(hwInfo.platform.eProductFamily);
        return productHelper.allowStatelessCompression(hwInfo);
    }
    default:
        return false;
    }
}

} // namespace NEO
