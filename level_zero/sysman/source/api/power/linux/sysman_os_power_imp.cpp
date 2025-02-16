/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/power/linux/sysman_os_power_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/source/sysman_const.h"

namespace L0 {
namespace Sysman {

const std::string LinuxPowerImp::sustainedPowerLimitEnabled("power1_max_enable");
const std::string LinuxPowerImp::burstPowerLimitEnabled("power1_cap_enable");
const std::string LinuxPowerImp::burstPowerLimit("power1_cap");
const std::string LinuxPowerImp::defaultPowerLimit("power_default_limit");
const std::string LinuxPowerImp::minPowerLimit("power_min_limit");
const std::string LinuxPowerImp::maxPowerLimit("power_max_limit");

ze_result_t LinuxPowerImp::getProperties(zes_power_properties_t *pProperties) {
    pProperties->onSubdevice = isSubdevice;
    pProperties->subdeviceId = subdeviceId;
    pProperties->canControl = canControl;
    pProperties->isEnergyThresholdSupported = false;
    pProperties->defaultLimit = -1;
    pProperties->minLimit = -1;
    pProperties->maxLimit = -1;

    uint32_t val = 0;
    auto result = pSysfsAccess->read(intelGraphicsHwmonDir + "/" + defaultPowerLimit, val);
    if (ZE_RESULT_SUCCESS == result) {
        pProperties->defaultLimit = static_cast<int32_t>(val / milliFactor); // need to convert from microwatt to milliwatt
    }

    result = pSysfsAccess->read(intelGraphicsHwmonDir + "/" + minPowerLimit, val);
    if (ZE_RESULT_SUCCESS == result && val != 0) {
        pProperties->minLimit = static_cast<int32_t>(val / milliFactor); // need to convert from microwatt to milliwatt
    }

    result = pSysfsAccess->read(intelGraphicsHwmonDir + "/" + maxPowerLimit, val);
    if (ZE_RESULT_SUCCESS == result && val != std::numeric_limits<uint32_t>::max()) {
        pProperties->maxLimit = static_cast<int32_t>(val / milliFactor); // need to convert from microwatt to milliwatt
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPowerImp::getPropertiesExt(zes_power_ext_properties_t *pExtPoperties) {
    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE \n", __FUNCTION__);
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPowerImp::getPmtEnergyCounter(zes_power_energy_counter_t *pEnergy) {
    const std::string key("PACKAGE_ENERGY");
    uint64_t energy = 0;
    constexpr uint64_t fixedPointToJoule = 1048576;
    ze_result_t result = pPmt->readValue(key, energy);
    // PMT will return energy counter in Q20 format(fixed point representation) where first 20 bits(from LSB) represent decimal part and remaining integral part which is converted into joule by division with 1048576(2^20) and then converted into microjoules
    pEnergy->energy = (energy / fixedPointToJoule) * convertJouleToMicroJoule;
    return result;
}
ze_result_t LinuxPowerImp::getEnergyCounter(zes_power_energy_counter_t *pEnergy) {
    pEnergy->timestamp = SysmanDevice::getSysmanTimestamp();
    std::string energyCounterNode = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameEnergyCounterNode, subdeviceId, false);
    ze_result_t result = pSysfsAccess->read(energyCounterNode, pEnergy->energy);
    if (result != ZE_RESULT_SUCCESS) {
        if (pPmt != nullptr) {
            return getPmtEnergyCounter(pEnergy);
        }
    }
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), energyCounterNode.c_str(), getErrorCode(result));
        return getErrorCode(result);
    }
    return result;
}

ze_result_t LinuxPowerImp::getLimits(zes_power_sustained_limit_t *pSustained, zes_power_burst_limit_t *pBurst, zes_power_peak_limit_t *pPeak) {
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    uint64_t val = 0;
    if (pSustained != nullptr) {
        result = pSysfsAccess->read(intelGraphicsHwmonDir + "/" + sustainedPowerLimitEnabled, val);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitEnabled.c_str(), getErrorCode(result));
            return getErrorCode(result);
        }
        pSustained->enabled = static_cast<ze_bool_t>(val);

        if (pSustained->enabled) {
            val = 0;
            std::string sustainedPowerLimit = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameSustainedPowerLimit, subdeviceId, false);
            result = pSysfsAccess->read(sustainedPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimit.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
            val /= milliFactor; // Convert microWatts to milliwatts
            pSustained->power = static_cast<int32_t>(val);

            val = 0;
            std::string sustainedPowerLimitInterval = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameSustainedPowerLimitInterval, subdeviceId, false);
            result = pSysfsAccess->read(sustainedPowerLimitInterval, val);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitInterval.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
            pSustained->interval = static_cast<int32_t>(val);
        }
    }
    if (pBurst != nullptr) {
        result = pSysfsAccess->read(intelGraphicsHwmonDir + "/" + burstPowerLimitEnabled, val);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), burstPowerLimitEnabled.c_str(), getErrorCode(result));
            return getErrorCode(result);
        }
        pBurst->enabled = static_cast<ze_bool_t>(val);

        if (pBurst->enabled) {
            result = pSysfsAccess->read(intelGraphicsHwmonDir + "/" + burstPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), burstPowerLimit.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
            val /= milliFactor; // Convert microWatts to milliwatts
            pBurst->power = static_cast<int32_t>(val);
        }
    }
    if (pPeak != nullptr) {
        pPeak->powerAC = -1;
        pPeak->powerDC = -1;
        result = ZE_RESULT_SUCCESS;
    }
    return result;
}

ze_result_t LinuxPowerImp::setLimits(const zes_power_sustained_limit_t *pSustained, const zes_power_burst_limit_t *pBurst, const zes_power_peak_limit_t *pPeak) {
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    int32_t val = 0;
    if (pSustained != nullptr) {
        uint64_t isSustainedPowerLimitEnabled = 0;
        result = pSysfsAccess->read(intelGraphicsHwmonDir + "/" + sustainedPowerLimitEnabled, isSustainedPowerLimitEnabled);
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitEnabled.c_str(), getErrorCode(result));
            return getErrorCode(result);
        }
        if (isSustainedPowerLimitEnabled != static_cast<uint64_t>(pSustained->enabled)) {
            result = pSysfsAccess->write(intelGraphicsHwmonDir + "/" + sustainedPowerLimitEnabled, static_cast<int>(pSustained->enabled));
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitEnabled.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
            isSustainedPowerLimitEnabled = static_cast<uint64_t>(pSustained->enabled);
        }

        if (isSustainedPowerLimitEnabled) {
            val = static_cast<uint32_t>(pSustained->power) * milliFactor; // Convert milliWatts to microwatts
            std::string sustainedPowerLimit = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameSustainedPowerLimit, subdeviceId, false);
            result = pSysfsAccess->write(sustainedPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimit.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }

            std::string sustainedPowerLimitInterval = intelGraphicsHwmonDir + "/" + pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameSustainedPowerLimitInterval, subdeviceId, false);
            result = pSysfsAccess->write(sustainedPowerLimitInterval, pSustained->interval);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), sustainedPowerLimitInterval.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
        }
        result = ZE_RESULT_SUCCESS;
    }
    if (pBurst != nullptr) {
        result = pSysfsAccess->write(intelGraphicsHwmonDir + "/" + burstPowerLimitEnabled, static_cast<int>(pBurst->enabled));
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), burstPowerLimitEnabled.c_str(), getErrorCode(result));
            return getErrorCode(result);
        }

        if (pBurst->enabled) {
            val = static_cast<uint32_t>(pBurst->power) * milliFactor; // Convert milliWatts to microwatts
            result = pSysfsAccess->write(intelGraphicsHwmonDir + "/" + burstPowerLimit, val);
            if (ZE_RESULT_SUCCESS != result) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s/%s and returning error:0x%x \n", __FUNCTION__, intelGraphicsHwmonDir.c_str(), burstPowerLimit.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
        }
    }
    return result;
}

ze_result_t LinuxPowerImp::getEnergyThreshold(zes_energy_threshold_t *pThreshold) {
    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE \n", __FUNCTION__);
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPowerImp::setEnergyThreshold(double threshold) {
    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE \n", __FUNCTION__);
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPowerImp::getLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) {
    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE \n", __FUNCTION__);
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPowerImp::setLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) {
    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() returning UNSUPPORTED_FEATURE \n", __FUNCTION__);
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool LinuxPowerImp::isHwmonDir(std::string name) {
    std::string intelGraphicsHwmonName = pSysmanKmdInterface->getHwmonName(subdeviceId, isSubdevice);
    if (isSubdevice == false && (name == intelGraphicsHwmonName)) {
        return true;
    }
    return false;
}

bool LinuxPowerImp::isPowerModuleSupported() {
    std::vector<std::string> listOfAllHwmonDirs = {};
    const std::string hwmonDir("device/hwmon");
    bool hwmonDirExists = false;
    if (ZE_RESULT_SUCCESS != pSysfsAccess->scanDirEntries(hwmonDir, listOfAllHwmonDirs)) {
        hwmonDirExists = false;
    }
    for (const auto &tempHwmonDirEntry : listOfAllHwmonDirs) {
        const std::string i915NameFile = hwmonDir + "/" + tempHwmonDirEntry + "/" + "name";
        std::string name;
        if (ZE_RESULT_SUCCESS != pSysfsAccess->read(i915NameFile, name)) {
            continue;
        }
        if (isHwmonDir(name)) {
            intelGraphicsHwmonDir = hwmonDir + "/" + tempHwmonDirEntry;
            hwmonDirExists = true;
            canControl = true;
        }
    }
    if (hwmonDirExists == false) {
        return (pPmt != nullptr);
    }
    return true;
}

LinuxPowerImp::LinuxPowerImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pPmt = pLinuxSysmanImp->getPlatformMonitoringTechAccess(subdeviceId);
    pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    pSysfsAccess = pSysmanKmdInterface->getSysFsAccess();
}

OsPower *OsPower::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    LinuxPowerImp *pLinuxPowerImp = new LinuxPowerImp(pOsSysman, onSubdevice, subdeviceId);
    return static_cast<OsPower *>(pLinuxPowerImp);
}
} // namespace Sysman
} // namespace L0
