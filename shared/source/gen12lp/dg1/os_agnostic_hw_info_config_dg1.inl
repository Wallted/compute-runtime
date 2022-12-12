/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aubstream/product_family.h"

namespace NEO {
template <>
uint32_t ProductHelperHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    switch (stepping) {
    case REVISION_A0:
        return 0x0;
    case REVISION_B:
        return 0x1;
    }
    return CommonConstants::invalidStepping;
}

template <>
AOT::PRODUCT_CONFIG ProductHelperHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    return AOT::DG1;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    switch (hwInfo.platform.usRevId) {
    case 0x0:
        return REVISION_A0;
    case 0x1:
        return REVISION_B;
    }
    return CommonConstants::invalidStepping;
}

template <>
bool ProductHelperHw<gfxProduct>::pipeControlWARequired(const HardwareInfo &hwInfo) const {
    return GfxCoreHelper::get(hwInfo.platform.eRenderCoreFamily).isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo);
}

template <>
bool ProductHelperHw<gfxProduct>::imagePitchAlignmentWARequired(const HardwareInfo &hwInfo) const {
    return GfxCoreHelper::get(hwInfo.platform.eRenderCoreFamily).isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo);
}

template <>
bool ProductHelperHw<gfxProduct>::isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) const {
    return GfxCoreHelper::get(hwInfo.platform.eRenderCoreFamily).isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo);
}

template <>
bool ProductHelperHw<gfxProduct>::obtainBlitterPreference(const HardwareInfo &hwInfo) const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::is3DPipelineSelectWARequired() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isStorageInfoAdjustmentRequired() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::overrideGfxPartitionLayoutForWsl() const {
    return true;
}

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Dg1;
};

} // namespace NEO
