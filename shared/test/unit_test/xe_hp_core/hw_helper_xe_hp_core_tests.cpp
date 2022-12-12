/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;
using GfxCoreHelperXeHpCoreTest = ::testing::Test;

XE_HP_CORE_TEST_F(GfxCoreHelperXeHpCoreTest, givenSteppingAorBWhenCheckingSipWAThenTrueIsReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto renderCoreFamily = defaultHwInfo->platform.eRenderCoreFamily;
    auto productFamily = defaultHwInfo->platform.eProductFamily;

    auto &helper = GfxCoreHelper::get(renderCoreFamily);
    const auto &productHelper = *ProductHelper::get(productFamily);

    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hwInfo);
    EXPECT_TRUE(helper.isSipWANeeded(hwInfo));

    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_TRUE(helper.isSipWANeeded(hwInfo));
}

XE_HP_CORE_TEST_F(GfxCoreHelperXeHpCoreTest, givenSteppingCWhenCheckingSipWAThenFalseIsReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto renderCoreFamily = defaultHwInfo->platform.eRenderCoreFamily;
    auto productFamily = defaultHwInfo->platform.eProductFamily;

    auto &helper = GfxCoreHelper::get(renderCoreFamily);
    const auto &productHelper = *ProductHelper::get(productFamily);

    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_C, hwInfo);
    EXPECT_FALSE(helper.isSipWANeeded(hwInfo));
}
