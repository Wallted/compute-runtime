/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

using namespace NEO;

template <>
bool HwInfoConfigHw<IGFX_XE_HP_SDV>::isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const {
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    uint32_t stepping = hwHelper.getSteppingFromHwRevId(hwInfo);
    return REVISION_B > stepping;
}

template <>
uint32_t HwInfoConfigHw<IGFX_XE_HP_SDV>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    switch (stepping) {
    case REVISION_A0:
        return 0x0;
    case REVISION_A1:
        return 0x1;
    case REVISION_B:
        return 0x4;
    }
    return CommonConstants::invalidStepping;
}
template <>
void HwInfoConfigHw<IGFX_XE_HP_SDV>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) {
    using SAMPLER_STATE = typename XeHpFamily::SAMPLER_STATE;

    auto samplerState = reinterpret_cast<SAMPLER_STATE *>(sampler);
    if (DebugManager.flags.ForceSamplerLowFilteringPrecision.get()) {
        samplerState->setLowQualityFilter(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE);
    }
}