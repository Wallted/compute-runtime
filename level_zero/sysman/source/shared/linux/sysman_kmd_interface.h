/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>

#include "igfxfmid.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace NEO {
class Drm;
} // namespace NEO

namespace L0 {
namespace Sysman {

class FsAccessInterface;
class ProcFsAccessInterface;
class SysFsAccessInterface;
class PmuInterface;
class LinuxSysmanImp;

typedef std::pair<std::string, std::string> valuePair;

enum EngineClass {
    ENGINE_CLASS_RENDER = 0,
    ENGINE_CLASS_COPY = 1,
    ENGINE_CLASS_VIDEO = 2,
    ENGINE_CLASS_VIDEO_ENHANCE = 3,
    ENGINE_CLASS_COMPUTE = 4,
    ENGINE_CLASS_INVALID = -1
};

const std::multimap<uint16_t, zes_engine_group_t> engineClassToEngineGroup = {
    {static_cast<uint16_t>(EngineClass::ENGINE_CLASS_RENDER), ZES_ENGINE_GROUP_RENDER_SINGLE},
    {static_cast<uint16_t>(EngineClass::ENGINE_CLASS_VIDEO), ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE},
    {static_cast<uint16_t>(EngineClass::ENGINE_CLASS_VIDEO), ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE},
    {static_cast<uint16_t>(EngineClass::ENGINE_CLASS_COPY), ZES_ENGINE_GROUP_COPY_SINGLE},
    {static_cast<uint16_t>(EngineClass::ENGINE_CLASS_COMPUTE), ZES_ENGINE_GROUP_COMPUTE_SINGLE},
    {static_cast<uint16_t>(EngineClass::ENGINE_CLASS_VIDEO_ENHANCE), ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE}};

const std::multimap<zes_engine_group_t, uint16_t> engineGroupToEngineClass = {
    {ZES_ENGINE_GROUP_RENDER_SINGLE, static_cast<uint16_t>(EngineClass::ENGINE_CLASS_RENDER)},
    {ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, static_cast<uint16_t>(EngineClass::ENGINE_CLASS_VIDEO)},
    {ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, static_cast<uint16_t>(EngineClass::ENGINE_CLASS_VIDEO)},
    {ZES_ENGINE_GROUP_COPY_SINGLE, static_cast<uint16_t>(EngineClass::ENGINE_CLASS_COPY)},
    {ZES_ENGINE_GROUP_COMPUTE_SINGLE, static_cast<uint16_t>(EngineClass::ENGINE_CLASS_COMPUTE)},
    {ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, static_cast<uint16_t>(EngineClass::ENGINE_CLASS_VIDEO_ENHANCE)}};

enum class SysfsName {
    sysfsNameMinFrequency,
    sysfsNameMaxFrequency,
    sysfsNameMinDefaultFrequency,
    sysfsNameMaxDefaultFrequency,
    sysfsNameBoostFrequency,
    sysfsNameCurrentFrequency,
    sysfsNameTdpFrequency,
    sysfsNameActualFrequency,
    sysfsNameEfficientFrequency,
    sysfsNameMaxValueFrequency,
    sysfsNameMinValueFrequency,
    sysfsNameThrottleReasonStatus,
    sysfsNameThrottleReasonPL1,
    sysfsNameThrottleReasonPL2,
    sysfsNameThrottleReasonPL4,
    sysfsNameThrottleReasonThermal,
    sysfsNameSustainedPowerLimit,
    sysfsNameSustainedPowerLimitInterval,
    sysfsNameEnergyCounterNode,
    sysfsNameDefaultPowerLimit,
    sysfsNameCriticalPowerLimit,
    sysfsNameStandbyModeControl,
    sysfsNameMemoryAddressRange,
    sysfsNameMaxMemoryFrequency,
    sysfsNameMinMemoryFrequency,
    sysfsNameSchedulerTimeout,
    sysfsNameSchedulerTimeslice,
    sysfsNameSchedulerWatchDogTimeout,
    sysfsNameSchedulerWatchDogTimeoutMaximum,
    sysfsNamePerformanceBaseFrequencyFactor,
    sysfsNamePerformanceMediaFrequencyFactor,
    sysfsNamePerformanceBaseFrequencyFactorScale,
    sysfsNamePerformanceMediaFrequencyFactorScale,
    sysfsNamePerformanceSystemPowerBalance,
};

class SysmanKmdInterface {
  public:
    SysmanKmdInterface();
    virtual ~SysmanKmdInterface();
    enum SysfsValueUnit : uint32_t {
        milliSecond,
        microSecond,
        unAvailable,
    };
    static std::unique_ptr<SysmanKmdInterface> create(NEO::Drm &drm);

    virtual std::string getBasePath(uint32_t subDeviceId) const = 0;
    virtual std::string getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool baseDirectoryExists) = 0;
    virtual std::string getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) = 0;
    virtual int64_t getEngineActivityFd(zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId, PmuInterface *const &pmuInterface) = 0;
    virtual std::string getHwmonName(uint32_t subDeviceId, bool isSubdevice) const = 0;
    virtual bool isStandbyModeControlAvailable() const = 0;
    virtual bool clientInfoAvailableInFdInfo() const = 0;
    virtual bool isGroupEngineInterfaceAvailable() const = 0;
    ze_result_t initFsAccessInterface(const NEO::Drm &drm);
    virtual bool isBaseFrequencyFactorAvailable() const = 0;
    virtual bool isSystemPowerBalanceAvailable() const = 0;
    FsAccessInterface *getFsAccess();
    ProcFsAccessInterface *getProcFsAccess();
    SysFsAccessInterface *getSysFsAccess();
    virtual std::string getEngineBasePath(uint32_t subDeviceId) const = 0;
    virtual bool useDefaultMaximumWatchdogTimeoutForExclusiveMode() = 0;
    virtual ze_result_t getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                                     LinuxSysmanImp *pLinuxSysmanImp,
                                                     SysFsAccessInterface *pSysfsAccess,
                                                     ze_bool_t onSubdevice,
                                                     uint32_t subdeviceId) = 0;
    ze_result_t getNumEngineTypeAndInstancesForDevice(std::string engineDir, std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                                      SysFsAccessInterface *pSysfsAccess);
    SysfsValueUnit getNativeUnit(const SysfsName sysfsName);
    void convertSysfsValueUnit(const SysfsValueUnit dstUnit, const SysfsValueUnit srcUnit,
                               const uint64_t srcValue, uint64_t &dstValue) const;
    virtual std::optional<std::string> getEngineClassString(uint16_t engineClass) = 0;
    virtual uint32_t getEventType(const bool isIntegratedDevice) = 0;
    virtual bool isDefaultFrequencyAvailable() const = 0;
    virtual bool isBoostFrequencyAvailable() const = 0;
    virtual bool isTdpFrequencyAvailable() const = 0;
    virtual bool isPhysicalMemorySizeSupported() const = 0;
    virtual void getWedgedStatus(LinuxSysmanImp *pLinuxSysmanImp, zes_device_state_t *pState) = 0;

  protected:
    std::unique_ptr<FsAccessInterface> pFsAccess;
    std::unique_ptr<ProcFsAccessInterface> pProcfsAccess;
    std::unique_ptr<SysFsAccessInterface> pSysfsAccess;
    virtual const std::map<SysfsName, SysfsValueUnit> &getSysfsNameToNativeUnitMap() = 0;
    uint32_t getEventTypeImpl(std::string &dirName, const bool isIntegratedDevice);
    void getWedgedStatusImpl(LinuxSysmanImp *pLinuxSysmanImp, zes_device_state_t *pState);
};

class SysmanKmdInterfaceI915 {

  protected:
    static const std::map<uint16_t, std::string> i915EngineClassToSysfsEngineMap;
    static std::string getBasePathI915(uint32_t subDeviceId);
    static std::string getHwmonNameI915(uint32_t subDeviceId, bool isSubdevice);
    static std::optional<std::string> getEngineClassStringI915(uint16_t engineClass);
    static std::string getEngineBasePathI915(uint32_t subDeviceId);
};

class SysmanKmdInterfaceI915Upstream : public SysmanKmdInterface, SysmanKmdInterfaceI915 {
  public:
    SysmanKmdInterfaceI915Upstream(const PRODUCT_FAMILY productFamily);
    ~SysmanKmdInterfaceI915Upstream() override;

    std::string getBasePath(uint32_t subDeviceId) const override;
    std::string getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool baseDirectoryExists) override;
    std::string getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) override;
    int64_t getEngineActivityFd(zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId, PmuInterface *const &pmuInterface) override;
    std::string getHwmonName(uint32_t subDeviceId, bool isSubdevice) const override;
    bool isStandbyModeControlAvailable() const override { return true; }
    bool clientInfoAvailableInFdInfo() const override { return false; }
    bool isGroupEngineInterfaceAvailable() const override { return false; }
    std::string getEngineBasePath(uint32_t subDeviceId) const override;
    bool useDefaultMaximumWatchdogTimeoutForExclusiveMode() override { return false; };
    ze_result_t getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                             LinuxSysmanImp *pLinuxSysmanImp,
                                             SysFsAccessInterface *pSysfsAccess,
                                             ze_bool_t onSubdevice,
                                             uint32_t subdeviceId) override;
    std::optional<std::string> getEngineClassString(uint16_t engineClass) override;
    uint32_t getEventType(const bool isIntegratedDevice) override;
    bool isBaseFrequencyFactorAvailable() const override { return false; }
    bool isSystemPowerBalanceAvailable() const override { return false; }
    bool isDefaultFrequencyAvailable() const override { return true; }
    bool isBoostFrequencyAvailable() const override { return true; }
    bool isTdpFrequencyAvailable() const override { return true; }
    bool isPhysicalMemorySizeSupported() const override { return false; }
    void getWedgedStatus(LinuxSysmanImp *pLinuxSysmanImp, zes_device_state_t *pState) override;

  protected:
    std::map<SysfsName, valuePair> sysfsNameToFileMap;
    void initSysfsNameToFileMap(const PRODUCT_FAMILY productFamily);
    const std::map<SysfsName, SysfsValueUnit> &getSysfsNameToNativeUnitMap() override {
        return sysfsNameToNativeUnitMap;
    }
    const std::map<SysfsName, SysfsValueUnit> sysfsNameToNativeUnitMap = {
        {SysfsName::sysfsNameSchedulerTimeout, milliSecond},
        {SysfsName::sysfsNameSchedulerTimeslice, milliSecond},
        {SysfsName::sysfsNameSchedulerWatchDogTimeout, milliSecond},
    };
};

class SysmanKmdInterfaceI915Prelim : public SysmanKmdInterface, SysmanKmdInterfaceI915 {
  public:
    SysmanKmdInterfaceI915Prelim(const PRODUCT_FAMILY productFamily);
    ~SysmanKmdInterfaceI915Prelim() override;

    std::string getBasePath(uint32_t subDeviceId) const override;
    std::string getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool baseDirectoryExists) override;
    std::string getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) override;
    int64_t getEngineActivityFd(zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId, PmuInterface *const &pmuInterface) override;
    std::string getHwmonName(uint32_t subDeviceId, bool isSubdevice) const override;
    bool isStandbyModeControlAvailable() const override { return true; }
    bool clientInfoAvailableInFdInfo() const override { return false; }
    bool isGroupEngineInterfaceAvailable() const override { return false; }
    std::string getEngineBasePath(uint32_t subDeviceId) const override;
    bool useDefaultMaximumWatchdogTimeoutForExclusiveMode() override { return false; };
    ze_result_t getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                             LinuxSysmanImp *pLinuxSysmanImp,
                                             SysFsAccessInterface *pSysfsAccess,
                                             ze_bool_t onSubdevice,
                                             uint32_t subdeviceId) override;
    std::optional<std::string> getEngineClassString(uint16_t engineClass) override;
    uint32_t getEventType(const bool isIntegratedDevice) override;
    bool isBaseFrequencyFactorAvailable() const override { return false; }
    bool isSystemPowerBalanceAvailable() const override { return false; }
    bool isDefaultFrequencyAvailable() const override { return true; }
    bool isBoostFrequencyAvailable() const override { return true; }
    bool isTdpFrequencyAvailable() const override { return true; }
    bool isPhysicalMemorySizeSupported() const override { return true; }
    void getWedgedStatus(LinuxSysmanImp *pLinuxSysmanImp, zes_device_state_t *pState) override;

  protected:
    std::map<SysfsName, valuePair> sysfsNameToFileMap;
    void initSysfsNameToFileMap(const PRODUCT_FAMILY productFamily);
    const std::map<SysfsName, SysfsValueUnit> &getSysfsNameToNativeUnitMap() override {
        return sysfsNameToNativeUnitMap;
    }
    const std::map<SysfsName, SysfsValueUnit> sysfsNameToNativeUnitMap = {
        {SysfsName::sysfsNameSchedulerTimeout, milliSecond},
        {SysfsName::sysfsNameSchedulerTimeslice, milliSecond},
        {SysfsName::sysfsNameSchedulerWatchDogTimeout, milliSecond},
    };
};

class SysmanKmdInterfaceXe : public SysmanKmdInterface {
  public:
    SysmanKmdInterfaceXe(const PRODUCT_FAMILY productFamily);
    ~SysmanKmdInterfaceXe() override;

    std::string getBasePath(uint32_t subDeviceId) const override;
    std::string getSysfsFilePath(SysfsName sysfsName, uint32_t subDeviceId, bool baseDirectoryExists) override;
    std::string getSysfsFilePathForPhysicalMemorySize(uint32_t subDeviceId) override;
    std::string getEngineBasePath(uint32_t subDeviceId) const override;
    int64_t getEngineActivityFd(zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId, PmuInterface *const &pmuInterface) override;
    std::string getHwmonName(uint32_t subDeviceId, bool isSubdevice) const override;
    bool isStandbyModeControlAvailable() const override { return false; }
    bool clientInfoAvailableInFdInfo() const override { return true; }
    bool isGroupEngineInterfaceAvailable() const override { return true; }
    bool useDefaultMaximumWatchdogTimeoutForExclusiveMode() override { return true; };
    ze_result_t getNumEngineTypeAndInstances(std::map<zes_engine_type_flag_t, std::vector<std::string>> &mapOfEngines,
                                             LinuxSysmanImp *pLinuxSysmanImp,
                                             SysFsAccessInterface *pSysfsAccess,
                                             ze_bool_t onSubdevice,
                                             uint32_t subdeviceId) override;
    std::optional<std::string> getEngineClassString(uint16_t engineClass) override;
    uint32_t getEventType(const bool isIntegratedDevice) override;
    bool isBaseFrequencyFactorAvailable() const override { return true; }
    bool isSystemPowerBalanceAvailable() const override { return true; }
    bool isDefaultFrequencyAvailable() const override { return false; }
    bool isBoostFrequencyAvailable() const override { return false; }
    bool isTdpFrequencyAvailable() const override { return false; }
    bool isPhysicalMemorySizeSupported() const override { return true; }

    // Wedged state is not supported in XE.
    void getWedgedStatus(LinuxSysmanImp *pLinuxSysmanImp, zes_device_state_t *pState) override{};

  protected:
    std::map<SysfsName, valuePair> sysfsNameToFileMap;
    void initSysfsNameToFileMap(const PRODUCT_FAMILY productFamily);
    const std::map<SysfsName, SysfsValueUnit> &getSysfsNameToNativeUnitMap() override {
        return sysfsNameToNativeUnitMap;
    }
    const std::map<SysfsName, SysfsValueUnit> sysfsNameToNativeUnitMap = {
        {SysfsName::sysfsNameSchedulerTimeout, microSecond},
        {SysfsName::sysfsNameSchedulerTimeslice, microSecond},
        {SysfsName::sysfsNameSchedulerWatchDogTimeout, milliSecond},
        {SysfsName::sysfsNameSchedulerWatchDogTimeoutMaximum, milliSecond},
    };
};

} // namespace Sysman
} // namespace L0
