/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_image.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
namespace ult {

using MultiTileImmediateCommandListTest = Test<MultiTileCommandListFixture<true, false, false, -1>>;

HWTEST2_F(MultiTileImmediateCommandListTest, GivenMultiTileDeviceWhenCreatingImmediateCommandListThenExpectPartitionCountMatchTileCount, IsWithinXeGfxFamily) {
    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(2u, commandList->partitionCount);

    auto returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(2u, commandList->partitionCount);
}

using MultiTileImmediateInternalCommandListTest = Test<MultiTileCommandListFixture<true, true, false, -1>>;

HWTEST2_F(MultiTileImmediateInternalCommandListTest, GivenMultiTileDeviceWhenCreatingInternalImmediateCommandListThenExpectPartitionCountEqualOne, IsWithinXeGfxFamily) {
    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(1u, commandList->partitionCount);

    auto returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, commandList->partitionCount);
}

using MultiTileCopyEngineCommandListTest = Test<MultiTileCommandListFixture<false, false, true, -1>>;

HWTEST2_F(MultiTileCopyEngineCommandListTest, GivenMultiTileDeviceWhenCreatingCopyEngineCommandListThenExpectPartitionCountEqualOne, IsWithinXeGfxFamily) {
    EXPECT_EQ(2u, device->getNEODevice()->getDeviceBitfield().count());
    EXPECT_EQ(1u, commandList->partitionCount);

    auto returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, commandList->partitionCount);
}

using CommandListExecuteImmediate = Test<DeviceFixture>;
HWTEST2_F(CommandListExecuteImmediate, whenExecutingCommandListImmediateWithFlushTaskThenRequiredStreamStateIsCorrectlyReported, IsAtLeastSkl) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseImmediateFlushTask.set(0);

    auto &productHelper = device->getProductHelper();

    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    auto &currentCsrStreamProperties = commandListImmediate.csr->getStreamProperties();

    commandListImmediate.requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value = 1;
    commandListImmediate.requiredStreamState.frontEndState.disableEUFusion.value = 1;
    commandListImmediate.requiredStreamState.frontEndState.disableOverdispatch.value = 1;
    commandListImmediate.requiredStreamState.stateComputeMode.isCoherencyRequired.value = 0;
    commandListImmediate.requiredStreamState.stateComputeMode.largeGrfMode.value = 1;
    commandListImmediate.requiredStreamState.stateComputeMode.threadArbitrationPolicy.value = NEO::ThreadArbitrationPolicy::RoundRobin;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, true);

    NEO::StateComputeModePropertiesSupport scmPropertiesSupport = {};
    productHelper.fillScmPropertiesSupportStructure(scmPropertiesSupport);
    NEO::FrontEndPropertiesSupport frontEndPropertiesSupport = {};
    productHelper.fillFrontEndPropertiesSupportStructure(frontEndPropertiesSupport, device->getHwInfo());

    int expectedDisableOverdispatch = frontEndPropertiesSupport.disableOverdispatch;
    int expectedLargeGrfMode = scmPropertiesSupport.largeGrfMode ? 1 : -1;
    int expectedThreadArbitrationPolicy = scmPropertiesSupport.threadArbitrationPolicy ? NEO::ThreadArbitrationPolicy::RoundRobin : -1;

    int expectedComputeDispatchAllWalkerEnable = frontEndPropertiesSupport.computeDispatchAllWalker ? 1 : -1;
    int expectedDisableEuFusion = frontEndPropertiesSupport.disableEuFusion ? 1 : -1;
    expectedDisableOverdispatch = frontEndPropertiesSupport.disableOverdispatch ? expectedDisableOverdispatch : -1;

    EXPECT_EQ(expectedComputeDispatchAllWalkerEnable, currentCsrStreamProperties.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(expectedDisableEuFusion, currentCsrStreamProperties.frontEndState.disableEUFusion.value);
    EXPECT_EQ(expectedDisableOverdispatch, currentCsrStreamProperties.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(expectedLargeGrfMode, currentCsrStreamProperties.stateComputeMode.largeGrfMode.value);
    EXPECT_EQ(expectedThreadArbitrationPolicy, currentCsrStreamProperties.stateComputeMode.threadArbitrationPolicy.value);

    commandListImmediate.requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value = 0;
    commandListImmediate.requiredStreamState.frontEndState.disableEUFusion.value = 0;
    commandListImmediate.requiredStreamState.frontEndState.disableOverdispatch.value = 0;
    commandListImmediate.requiredStreamState.stateComputeMode.isCoherencyRequired.value = 0;
    commandListImmediate.requiredStreamState.stateComputeMode.largeGrfMode.value = 0;
    commandListImmediate.requiredStreamState.stateComputeMode.threadArbitrationPolicy.value = NEO::ThreadArbitrationPolicy::AgeBased;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, true);

    expectedLargeGrfMode = scmPropertiesSupport.largeGrfMode ? 0 : -1;
    expectedThreadArbitrationPolicy = scmPropertiesSupport.threadArbitrationPolicy ? NEO::ThreadArbitrationPolicy::AgeBased : -1;

    expectedComputeDispatchAllWalkerEnable = frontEndPropertiesSupport.computeDispatchAllWalker ? 0 : -1;
    expectedDisableOverdispatch = frontEndPropertiesSupport.disableOverdispatch ? 0 : -1;
    expectedDisableEuFusion = frontEndPropertiesSupport.disableEuFusion ? 0 : -1;

    EXPECT_EQ(expectedComputeDispatchAllWalkerEnable, currentCsrStreamProperties.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(expectedDisableEuFusion, currentCsrStreamProperties.frontEndState.disableEUFusion.value);
    EXPECT_EQ(expectedDisableOverdispatch, currentCsrStreamProperties.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(expectedLargeGrfMode, currentCsrStreamProperties.stateComputeMode.largeGrfMode.value);
    EXPECT_EQ(expectedThreadArbitrationPolicy, currentCsrStreamProperties.stateComputeMode.threadArbitrationPolicy.value);
}

HWTEST2_F(CommandListExecuteImmediate, whenExecutingCommandListImmediateWithFlushTaskThenContainsAnyKernelFlagIsReset, IsAtLeastSkl) {
    std::unique_ptr<L0::CommandList> commandList;
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceMemoryPrefetchForKmdMigratedSharedAllocations.set(true);
    debugManager.flags.EnableBOChunkingPrefetch.set(true);
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    commandListImmediate.containsAnyKernel = true;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, true);
    EXPECT_FALSE(commandListImmediate.containsAnyKernel);
}

HWTEST2_F(CommandListExecuteImmediate, whenExecutingCommandListImmediateWithFlushTaskThenContainsAnyKernelFlagIsReset2, IsAtLeastSkl) {
    std::unique_ptr<L0::CommandList> commandList;
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceMemoryPrefetchForKmdMigratedSharedAllocations.set(true);
    debugManager.flags.EnableBOChunkingPrefetch.set(false);
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    commandListImmediate.containsAnyKernel = true;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, true);
    EXPECT_FALSE(commandListImmediate.containsAnyKernel);
}

HWTEST2_F(CommandListExecuteImmediate, whenExecutingCommandListImmediateWithFlushTaskThenContainsAnyKernelFlagIsReset3, IsAtLeastSkl) {
    std::unique_ptr<L0::CommandList> commandList;
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceMemoryPrefetchForKmdMigratedSharedAllocations.set(false);
    debugManager.flags.EnableBOChunkingPrefetch.set(true);
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    commandListImmediate.containsAnyKernel = true;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, true);
    EXPECT_FALSE(commandListImmediate.containsAnyKernel);
}

HWTEST2_F(CommandListExecuteImmediate, whenExecutingCommandListImmediateWithFlushTaskThenContainsAnyKernelFlagIsReset4, IsAtLeastSkl) {
    std::unique_ptr<L0::CommandList> commandList;
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceMemoryPrefetchForKmdMigratedSharedAllocations.set(false);
    debugManager.flags.EnableBOChunkingPrefetch.set(false);
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    commandListImmediate.containsAnyKernel = true;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, true);
    EXPECT_FALSE(commandListImmediate.containsAnyKernel);
}

HWTEST2_F(CommandListExecuteImmediate, whenExecutingCommandListImmediateWithFlushTaskThenSuccessIsReturned, IsAtLeastSkl) {
    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, false));
}

HWTEST2_F(CommandListExecuteImmediate, givenOutOfHostMemoryErrorOnFlushWhenExecutingCommandListImmediateWithFlushTaskThenProperErrorIsReturned, IsAtLeastSkl) {
    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    auto &commandStreamReceiver = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfHostMemory;
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, false));
}

HWTEST2_F(CommandListExecuteImmediate, givenOutOfDeviceMemoryErrorOnFlushWhenExecutingCommandListImmediateWithFlushTaskThenProperErrorIsReturned, IsAtLeastSkl) {
    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    auto &commandStreamReceiver = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.flushReturnValue = SubmissionStatus::outOfMemory;
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, false));
}

HWTEST2_F(CommandListExecuteImmediate, GivenImmediateCommandListWhenCommandListIsCreatedThenCsrStateIsNotSet, IsAtLeastSkl) {
    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    auto &currentCsrStreamProperties = commandListImmediate.csr->getStreamProperties();
    EXPECT_EQ(-1, currentCsrStreamProperties.stateComputeMode.isCoherencyRequired.value);
    EXPECT_EQ(-1, currentCsrStreamProperties.stateComputeMode.devicePreemptionMode.value);

    EXPECT_EQ(-1, currentCsrStreamProperties.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(-1, currentCsrStreamProperties.frontEndState.singleSliceDispatchCcsMode.value);

    EXPECT_EQ(-1, currentCsrStreamProperties.pipelineSelect.modeSelected.value);
    EXPECT_EQ(-1, currentCsrStreamProperties.pipelineSelect.mediaSamplerDopClockGate.value);

    EXPECT_EQ(-1, currentCsrStreamProperties.stateBaseAddress.globalAtomics.value);
}

using CommandListTest = Test<DeviceFixture>;
using IsDcFlushSupportedPlatform = IsWithinGfxCore<IGFX_GEN9_CORE, IGFX_XE_HP_CORE>;

HWTEST2_F(CommandListTest, givenCopyCommandListWhenRequiredFlushOperationThenExpectNoPipeControl, IsDcFlushSupportedPlatform) {
    EXPECT_TRUE(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()));

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList->addFlushRequiredCommand(true, nullptr);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(usedBefore, usedAfter);
}

HWTEST2_F(CommandListTest, givenCopyCommandListWhenAppendCopyWithDependenciesThenDoNotTrackDependencies, IsAtLeastSkl) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdList.commandContainer.setImmediateCmdListCsr(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    cmdList.csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

    ze_result_t result = ZE_RESULT_SUCCESS;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x5678);
    auto zeEvent = event->toHandle();

    cmdList.appendMemoryCopy(dstPtr, srcPtr, sizeof(uint32_t), nullptr, 1, &zeEvent, false, false);

    EXPECT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver->peekBarrierCount(), 0u);

    cmdList.csr->getInternalAllocationStorage()->getTemporaryAllocations().freeAllGraphicsAllocations(device->getNEODevice());
}

HWTEST2_F(CommandListTest, givenCopyCommandListWhenAppendCopyRegionWithDependenciesThenDoNotTrackDependencies, IsAtLeastSkl) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdList.commandContainer.setImmediateCmdListCsr(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    cmdList.csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

    ze_result_t result = ZE_RESULT_SUCCESS;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x5678);
    auto zeEvent = event->toHandle();
    ze_copy_region_t region = {};

    cmdList.appendMemoryCopyRegion(dstPtr, &region, 0, 0, srcPtr, &region, 0, 0, nullptr, 1, &zeEvent, false, false);

    EXPECT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver->peekBarrierCount(), 0u);

    cmdList.csr->getInternalAllocationStorage()->getTemporaryAllocations().freeAllGraphicsAllocations(device->getNEODevice());
}

HWTEST2_F(CommandListTest, givenCopyCommandListWhenAppendFillWithDependenciesThenDoNotTrackDependencies, IsAtLeastSkl) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdList.commandContainer.setImmediateCmdListCsr(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    cmdList.csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;

    ze_result_t result = ZE_RESULT_SUCCESS;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    void *srcPtr = reinterpret_cast<void *>(0x1234);
    uint32_t patter = 1;
    auto zeEvent = event->toHandle();

    cmdList.appendMemoryFill(srcPtr, &patter, 1, sizeof(uint32_t), nullptr, 1, &zeEvent, false);

    EXPECT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver->peekBarrierCount(), 0u);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenRequiredFlushOperationThenExpectPipeControlWithDcFlush, IsDcFlushSupportedPlatform) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    EXPECT_TRUE(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()));

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList->addFlushRequiredCommand(true, nullptr);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(sizeof(PIPE_CONTROL), usedAfter - usedBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));
    auto pipeControl = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(pipeControl, cmdList.end());
    auto cmdPipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControl);
    EXPECT_TRUE(cmdPipeControl->getDcFlushEnable());
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenNoRequiredFlushOperationThenExpectNoPipeControl, IsDcFlushSupportedPlatform) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    EXPECT_TRUE(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()));

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList->addFlushRequiredCommand(false, nullptr);
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(usedBefore, usedAfter);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenRequiredFlushOperationAndNoSignalScopeEventThenExpectPipeControlWithDcFlush, IsDcFlushSupportedPlatform) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    EXPECT_TRUE(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()));

    ze_result_t result = ZE_RESULT_SUCCESS;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList->addFlushRequiredCommand(true, event.get());
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(sizeof(PIPE_CONTROL), usedAfter - usedBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        usedAfter - usedBefore));
    auto pipeControl = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(pipeControl, cmdList.end());
    auto cmdPipeControl = genCmdCast<PIPE_CONTROL *>(*pipeControl);
    EXPECT_TRUE(cmdPipeControl->getDcFlushEnable());
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenRequiredFlushOperationAndSignalScopeEventThenExpectNoPipeControl, IsDcFlushSupportedPlatform) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    EXPECT_TRUE(NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, device->getNEODevice()->getRootDeviceEnvironment()));

    ze_result_t result = ZE_RESULT_SUCCESS;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    auto commandList = std::make_unique<::L0::ult::CommandListCoreFamily<gfxCoreFamily>>();
    ASSERT_NE(nullptr, commandList);
    ze_result_t returnValue = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto &commandContainer = commandList->commandContainer;

    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    commandList->addFlushRequiredCommand(true, event.get());
    size_t usedAfter = commandContainer.getCommandStream()->getUsed();
    EXPECT_EQ(usedBefore, usedAfter);
}

HWTEST2_F(CommandListTest, givenImmediateCommandListWhenAppendMemoryRangesBarrierUsingFlushTaskThenExpectCorrectExecuteCall, IsAtLeastSkl) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t numRanges = 1;
    const size_t rangeSizes = 1;
    const char *rangesBuffer[rangeSizes];
    const void **ranges = reinterpret_cast<const void **>(&rangesBuffer[0]);

    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.isFlushTaskSubmissionEnabled = true;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList.commandContainer.setImmediateCmdListCsr(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    result = cmdList.appendMemoryRangesBarrier(numRanges, &rangeSizes,
                                               ranges, nullptr, 0,
                                               nullptr);
    EXPECT_EQ(0u, cmdList.executeCommandListImmediateCalledCount);
    EXPECT_EQ(1u, cmdList.executeCommandListImmediateWithFlushTaskCalledCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListTest, givenImmediateCommandListWhenAppendMemoryRangesBarrierNotUsingFlushTaskThenExpectCorrectExecuteCall, IsAtLeastSkl) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t numRanges = 1;
    const size_t rangeSizes = 1;
    const char *rangesBuffer[rangeSizes];
    const void **ranges = reinterpret_cast<const void **>(&rangesBuffer[0]);

    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.isFlushTaskSubmissionEnabled = false;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList.commandContainer.setImmediateCmdListCsr(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    result = cmdList.appendMemoryRangesBarrier(numRanges, &rangeSizes,
                                               ranges, nullptr, 0,
                                               nullptr);
    EXPECT_EQ(1u, cmdList.executeCommandListImmediateCalledCount);
    EXPECT_EQ(0u, cmdList.executeCommandListImmediateWithFlushTaskCalledCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST2_F(CommandListTest, givenImmediateCommandListWhenFlushImmediateThenOverrideEventCsr, IsAtLeastSkl) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdList.commandContainer.setImmediateCmdListCsr(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto event = std::unique_ptr<Event>(static_cast<Event *>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device)));

    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    cmdList.csr = event->csrs[0];
    event->csrs[0] = &mockCommandStreamReceiver;
    cmdList.flushImmediate(ZE_RESULT_SUCCESS, false, false, false, false, event->toHandle());
    EXPECT_EQ(event->csrs[0], cmdList.csr);
}

HWTEST2_F(CommandListTest, givenRegularCmdListWhenAskingForRelaxedOrderingThenReturnFalse, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    EXPECT_FALSE(commandList->isRelaxedOrderingDispatchAllowed(5));
}

HWTEST2_F(CommandListTest,
          givenComputeCommandListAnd2dRegionWhenMemoryCopyRegionInExternalHostAllocationCalledThenBuiltinFlagAndDestinationAllocSystemIsSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);
}

HWTEST2_F(CommandListTest,
          givenComputeCommandListAnd2dRegionWhenMemoryCopyRegionInUsmHostAllocationCalledThenBuiltinFlagAndDestinationAllocSystemIsSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t allocSize = 4096;
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    commandList->appendMemoryCopyRegion(dstBuffer, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest,
          givenComputeCommandListAnd2dRegionWhenMemoryCopyRegionInUsmDeviceAllocationCalledThenBuiltinFlagIsSetAndDestinationAllocSystemFlagNotSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    ze_copy_region_t dstRegion = {4, 4, 0, 2, 2, 1};
    ze_copy_region_t srcRegion = {4, 4, 0, 2, 2, 1};
    commandList->appendMemoryCopyRegion(dstBuffer, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest,
          givenComputeCommandListAnd3dRegionWhenMemoryCopyRegionInExternalHostAllocationCalledThenBuiltinAndDestinationAllocSystemFlagIsSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);
}

HWTEST2_F(CommandListTest,
          givenComputeCommandListAnd3dRegionWhenMemoryCopyRegionInUsmHostAllocationCalledThenBuiltinAndDestinationAllocSystemFlagIsSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t allocSize = 4096;
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendMemoryCopyRegion(dstBuffer, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest,
          givenComputeCommandListAnd3dRegionWhenMemoryCopyRegionInUsmDeviceAllocationCalledThenBuiltinFlagIsSetAndDestinationAllocSystemFlagNotSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    ze_copy_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    ze_copy_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendMemoryCopyRegion(dstBuffer, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr, false, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

using ImageSupport = IsNotAnyGfxCores<IGFX_GEN8_CORE, IGFX_XE_HPC_CORE>;

HWTEST2_F(CommandListTest, givenComputeCommandListWhenCopyFromImageToImageTheBuiltinFlagIsSet, ImageSupport) {
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHwSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto imageHwDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHwSrc->initialize(device, &zeDesc);
    imageHwDst->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    ze_image_region_t dstRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendImageCopyRegion(imageHwDst->toHandle(), imageHwSrc->toHandle(), &dstRegion, &srcRegion, nullptr, 0, nullptr, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenCopyFromImageToExternalHostMemoryThenBuiltinFlagAndDestinationAllocSystemIsSet, ImageSupport) {
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    void *dstPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHw = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHw->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendImageCopyToMemory(dstPtr, imageHw->toHandle(), &srcRegion, nullptr, 0, nullptr, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenCopyFromImageToUsmHostMemoryThenBuiltinFlagAndDestinationAllocSystemIsSet, ImageSupport) {
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t allocSize = 4096;
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHw = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHw->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendImageCopyToMemory(dstBuffer, imageHw->toHandle(), &srcRegion, nullptr, 0, nullptr, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenCopyFromImageToUsmDeviceMemoryThenBuiltinFlagIsSetAndDestinationAllocSystemNotSet, ImageSupport) {
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto imageHw = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHw->initialize(device, &zeDesc);

    ze_image_region_t srcRegion = {4, 4, 4, 2, 2, 2};
    commandList->appendImageCopyToMemory(dstBuffer, imageHw->toHandle(), &srcRegion, nullptr, 0, nullptr, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenImageCopyFromMemoryThenBuiltinFlagIsSet, ImageSupport) {
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_3D;
    zeDesc.height = 2;
    zeDesc.depth = 2;
    auto imageHw = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHw->initialize(device, &zeDesc);

    Vec3<size_t> expectedRegionCopySize = {zeDesc.width, zeDesc.height, zeDesc.depth};
    Vec3<size_t> expectedRegionOrigin = {0, 0, 0};
    commandList->appendImageCopyFromMemory(imageHw->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenMemoryCopyInExternalHostAllocationThenBuiltinFlagAndDestinationAllocSystemIsSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, false, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenMemoryCopyInUsmHostAllocationThenBuiltinFlagAndDestinationAllocSystemIsSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t allocSize = 4096;
    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    commandList->appendMemoryCopy(dstBuffer, srcPtr, 8, nullptr, 0, nullptr, false, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenMemoryCopyInUsmDeviceAllocationThenBuiltinFlagIsSetAndDestinationAllocSystemNotSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    commandList->appendMemoryCopy(dstBuffer, srcPtr, 8, nullptr, 0, nullptr, false, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenMemoryCopyWithReservedDeviceAllocationThenResidencyContainerHasImplicitMappedAllocations, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    driverHandle->devices[0]->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    void *dstBuffer = nullptr;
    size_t size = MemoryConstants::pageSize64k;
    size_t reservationSize = size * 2;

    auto res = context->reserveVirtualMem(nullptr, reservationSize, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    ze_physical_mem_desc_t desc = {};
    desc.size = size;
    ze_physical_mem_handle_t phPhysicalMemory;
    res = context->createPhysicalMem(device->toHandle(), &desc, &phPhysicalMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    ze_physical_mem_handle_t phPhysicalMemory2;
    res = context->createPhysicalMem(device->toHandle(), &desc, &phPhysicalMemory2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = context->mapVirtualMem(dstBuffer, size, phPhysicalMemory, 0, ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    void *offsetAddress = reinterpret_cast<void *>(reinterpret_cast<uint64_t>(dstBuffer) + size);
    res = context->mapVirtualMem(offsetAddress, size, phPhysicalMemory2, 0, ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    commandList->appendMemoryCopy(dstBuffer, srcPtr, size, nullptr, 0, nullptr, false, false);

    bool phys2Resident = false;
    for (auto alloc : commandList->getCmdContainer().getResidencyContainer()) {
        if (alloc && alloc->getGpuAddress() == reinterpret_cast<uint64_t>(offsetAddress)) {
            phys2Resident = true;
        }
    }

    EXPECT_TRUE(phys2Resident);
    res = context->unMapVirtualMem(dstBuffer, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = context->unMapVirtualMem(offsetAddress, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = context->freeVirtualMem(dstBuffer, reservationSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = context->destroyPhysicalMem(phPhysicalMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = context->destroyPhysicalMem(phPhysicalMemory2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenMemoryCopyWithOneReservedDeviceAllocationMappedToFullReservationThenExtendedBufferSizeIsZero, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    driverHandle->devices[0]->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    void *dstBuffer = nullptr;
    size_t size = MemoryConstants::pageSize64k;
    size_t reservationSize = size * 2;

    auto res = context->reserveVirtualMem(nullptr, reservationSize, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    ze_physical_mem_desc_t desc = {};
    desc.size = reservationSize;
    ze_physical_mem_handle_t phPhysicalMemory;
    res = context->createPhysicalMem(device->toHandle(), &desc, &phPhysicalMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = context->mapVirtualMem(dstBuffer, reservationSize, phPhysicalMemory, 0, ZE_MEMORY_ACCESS_ATTRIBUTE_READWRITE);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    commandList->appendMemoryCopy(dstBuffer, srcPtr, reservationSize, nullptr, 0, nullptr, false, false);

    res = context->unMapVirtualMem(dstBuffer, reservationSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = context->freeVirtualMem(dstBuffer, reservationSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    res = context->destroyPhysicalMem(phPhysicalMemory);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenMemoryFillInUsmHostThenBuiltinFlagAndDestinationAllocSystemIsSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t allocSize = 4096;
    constexpr size_t patternSize = 8;
    uint8_t pattern[patternSize] = {1, 2, 3, 4};

    void *dstBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocHostMem(&hostDesc, allocSize, allocSize, &dstBuffer);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->appendMemoryFill(dstBuffer, pattern, patternSize, allocSize, nullptr, 0, nullptr, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    commandList->appendMemoryFill(dstBuffer, pattern, 1, allocSize, nullptr, 0, nullptr, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenMemoryFillInUsmDeviceThenBuiltinFlagIsSetAndDestinationAllocSystemNotSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t patternSize = 8;
    uint8_t pattern[patternSize] = {1, 2, 3, 4};

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->appendMemoryFill(dstBuffer, pattern, patternSize, size, nullptr, 0, nullptr, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    commandList->appendMemoryFill(dstBuffer, pattern, 1, size, nullptr, 0, nullptr, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenMemoryFillRequiresMultiKernelsThenSplitFlagIsSet, IsAtLeastSkl) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);

    constexpr size_t patternSize = 8;
    uint8_t pattern[patternSize] = {1, 2, 3, 4};

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    constexpr size_t fillSize = size - 1;

    commandList->appendMemoryFill(dstBuffer, pattern, patternSize, fillSize, nullptr, 0, nullptr, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    commandList->appendMemoryFill(dstBuffer, pattern, 1, fillSize, nullptr, 0, nullptr, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    context->freeMem(dstBuffer);
}

using IsPlatformSklToDg1 = IsWithinProducts<IGFX_SKYLAKE, IGFX_DG1>;
HWTEST2_F(CommandListTest, givenComputeCommandListWhenMemoryCopyInUsmDeviceAllocationThenSplitFlagIsSetAndHeapsEstimationIsProper, IsPlatformSklToDg1) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->isFlushTaskSubmissionEnabled = true;
    commandList->immediateCmdListHeapSharing = true;
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    commandList->commandContainer.setImmediateCmdListCsr(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 0;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *srcPtr = reinterpret_cast<void *>(0x1234);

    auto &cmdContainer = commandList->commandContainer;
    auto csrDshHeap = &device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getIndirectHeap(HeapType::dynamicState, MemoryConstants::pageSize64k);
    auto csrSshHeap = &device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getIndirectHeap(HeapType::surfaceState, MemoryConstants::pageSize64k);

    size_t dshUsed = csrDshHeap->getUsed();
    size_t sshUsed = csrSshHeap->getUsed();

    commandList->appendMemoryCopy(dstBuffer, srcPtr, 0x101, nullptr, 0, nullptr, false, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    // As numKernelsExecutedInSplitLaunch is incremented after split kernel launch. But we are storing usedKernelLaunchParams before actual split kernel launch.
    // Hence below comparison tells that actually (usedKernelLaunchParams.numKernelsExecutedInSplitLaunch + 1) split kernels are launched
    EXPECT_EQ(commandList->usedKernelLaunchParams.numKernelsInSplitLaunch, commandList->usedKernelLaunchParams.numKernelsExecutedInSplitLaunch + 1);

    size_t dshEstimated = NEO::EncodeDispatchKernel<FamilyType>::getSizeRequiredDsh(
        commandList->firstKernelInSplitOperation->getKernelDescriptor(),
        cmdContainer.getNumIddPerBlock());
    size_t sshEstimated = NEO::EncodeDispatchKernel<FamilyType>::getSizeRequiredSsh(*commandList->firstKernelInSplitOperation->getImmutableData()->getKernelInfo());

    auto expectedDshToBeConsumed = dshEstimated * commandList->usedKernelLaunchParams.numKernelsInSplitLaunch;
    auto expectedSshToBeConsumed = sshEstimated * commandList->usedKernelLaunchParams.numKernelsInSplitLaunch;
    auto consumedDsh1 = csrDshHeap->getUsed();
    auto consumedSsh1 = csrSshHeap->getUsed();

    EXPECT_EQ(expectedDshToBeConsumed, (consumedDsh1 - dshUsed));
    EXPECT_EQ(expectedSshToBeConsumed, (consumedSsh1 - sshUsed));

    context->freeMem(dstBuffer);
}

HWTEST2_F(CommandListTest, givenComputeCommandListWhenMemoryFillRequiresMultiKernelsThenSplitFlagIsSetAndHeapsEstimationIsProper, IsPlatformSklToDg1) {
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->isFlushTaskSubmissionEnabled = true;
    commandList->immediateCmdListHeapSharing = true;
    commandList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    commandList->commandContainer.setImmediateCmdListCsr(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    constexpr size_t patternSize = 8;
    uint8_t pattern[patternSize] = {1, 2, 3, 4};

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *dstBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(),
                                          &deviceDesc,
                                          size, alignment, &dstBuffer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    constexpr size_t fillSize = size - 1;

    auto &cmdContainer = commandList->commandContainer;
    auto csrDshHeap = &device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getIndirectHeap(HeapType::dynamicState, MemoryConstants::pageSize64k);
    auto csrSshHeap = &device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getIndirectHeap(HeapType::surfaceState, MemoryConstants::pageSize64k);

    size_t dshUsed = csrDshHeap->getUsed();
    size_t sshUsed = csrSshHeap->getUsed();

    commandList->appendMemoryFill(dstBuffer, pattern, patternSize, fillSize, nullptr, 0, nullptr, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    // As numKernelsExecutedInSplitLaunch is incremented after split kernel launch. But we are storing usedKernelLaunchParams before actual split kernel launch.
    // Hence below comparison tells that actually (usedKernelLaunchParams.numKernelsExecutedInSplitLaunch + 1) split kernels are launched
    EXPECT_EQ(commandList->usedKernelLaunchParams.numKernelsInSplitLaunch, commandList->usedKernelLaunchParams.numKernelsExecutedInSplitLaunch + 1);

    size_t dshEstimated = NEO::EncodeDispatchKernel<FamilyType>::getSizeRequiredDsh(
        commandList->firstKernelInSplitOperation->getKernelDescriptor(),
        cmdContainer.getNumIddPerBlock());
    size_t sshEstimated = NEO::EncodeDispatchKernel<FamilyType>::getSizeRequiredSsh(*commandList->firstKernelInSplitOperation->getImmutableData()->getKernelInfo());

    auto expectedDshToBeConsumed = dshEstimated * commandList->usedKernelLaunchParams.numKernelsInSplitLaunch;
    auto expectedSshToBeConsumed = sshEstimated * commandList->usedKernelLaunchParams.numKernelsInSplitLaunch;
    auto consumedDsh1 = csrDshHeap->getUsed();
    auto consumedSsh1 = csrSshHeap->getUsed();

    EXPECT_EQ(expectedDshToBeConsumed, (consumedDsh1 - dshUsed));
    EXPECT_EQ(expectedSshToBeConsumed, (consumedSsh1 - sshUsed));

    commandList->appendMemoryFill(dstBuffer, pattern, 1, fillSize, nullptr, 0, nullptr, false);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);
    EXPECT_EQ(commandList->usedKernelLaunchParams.numKernelsInSplitLaunch, commandList->usedKernelLaunchParams.numKernelsExecutedInSplitLaunch + 1);

    dshEstimated = NEO::EncodeDispatchKernel<FamilyType>::getSizeRequiredDsh(
        commandList->firstKernelInSplitOperation->getKernelDescriptor(),
        cmdContainer.getNumIddPerBlock());
    sshEstimated = NEO::EncodeDispatchKernel<FamilyType>::getSizeRequiredSsh(*commandList->firstKernelInSplitOperation->getImmutableData()->getKernelInfo());

    expectedDshToBeConsumed = dshEstimated * commandList->usedKernelLaunchParams.numKernelsInSplitLaunch;
    expectedSshToBeConsumed = sshEstimated * commandList->usedKernelLaunchParams.numKernelsInSplitLaunch;
    auto consumedDsh2 = csrDshHeap->getUsed();
    auto consumedSsh2 = csrSshHeap->getUsed();
    EXPECT_EQ(expectedDshToBeConsumed, (consumedDsh2 - consumedDsh1));
    EXPECT_EQ(expectedSshToBeConsumed, (consumedSsh2 - consumedSsh1));

    context->freeMem(dstBuffer);
}

TEST(CommandList, whenAsMutableIsCalledNullptrIsReturned) {
    MockCommandList cmdList;
    EXPECT_EQ(nullptr, cmdList.asMutable());
}
class MockCommandQueueIndirectAccess : public Mock<CommandQueue> {
  public:
    MockCommandQueueIndirectAccess(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc) : Mock(device, csr, desc) {}
    void handleIndirectAllocationResidency(UnifiedMemoryControls unifiedMemoryControls, std::unique_lock<std::mutex> &lockForIndirect, bool performMigration) override {
        handleIndirectAllocationResidencyCalledTimes++;
    }
    uint32_t handleIndirectAllocationResidencyCalledTimes = 0;
};

HWTEST2_F(CommandListTest, givenCmdListWithIndirectAccessWhenExecutingCommandListImmediateWithFlushTaskThenHandleIndirectAccessCalled, IsAtLeastSkl) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    MockCommandQueueIndirectAccess mockCommandQueue(device, &mockCommandStreamReceiver, &desc);

    auto oldCommandQueue = commandListImmediate.cmdQImmediate;
    commandListImmediate.cmdQImmediate = &mockCommandQueue;
    commandListImmediate.indirectAllocationsAllowed = true;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, false);
    EXPECT_EQ(mockCommandQueue.handleIndirectAllocationResidencyCalledTimes, 1u);
    commandListImmediate.cmdQImmediate = oldCommandQueue;
}

HWTEST2_F(CommandListTest, givenCmdListWithNoIndirectAccessWhenExecutingCommandListImmediateWithFlushTaskThenHandleIndirectAccessNotCalled, IsAtLeastSkl) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<gfxCoreFamily> &>(*commandList);

    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    MockCommandQueueIndirectAccess mockCommandQueue(device, &mockCommandStreamReceiver, &desc);

    auto oldCommandQueue = commandListImmediate.cmdQImmediate;
    commandListImmediate.cmdQImmediate = &mockCommandQueue;
    commandListImmediate.indirectAllocationsAllowed = false;
    commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, false);
    EXPECT_EQ(mockCommandQueue.handleIndirectAllocationResidencyCalledTimes, 0u);
    commandListImmediate.cmdQImmediate = oldCommandQueue;
}

using ImmediateCmdListSharedHeapsTest = Test<ImmediateCmdListSharedHeapsFixture>;
HWTEST2_F(ImmediateCmdListSharedHeapsTest, givenMultipleCommandListsUsingSharedHeapsWhenDispatchingKernelThenExpectSingleSbaCommandAndHeapsReused, IsAtLeastSkl) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    using SAMPLER_BORDER_COLOR_STATE = typename FamilyType::SAMPLER_BORDER_COLOR_STATE;

    auto &cmdContainer = commandListImmediate->commandContainer;

    EXPECT_TRUE(commandListImmediate->isFlushTaskSubmissionEnabled);
    EXPECT_TRUE(commandListImmediate->immediateCmdListHeapSharing);

    EXPECT_EQ(1u, cmdContainer.getNumIddPerBlock());
    EXPECT_TRUE(cmdContainer.immediateCmdListSharedHeap(HeapType::dynamicState));
    EXPECT_TRUE(cmdContainer.immediateCmdListSharedHeap(HeapType::surfaceState));

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = ultCsr.commandStream;

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = ZE_RESULT_SUCCESS;

    auto csrDshHeap = &ultCsr.getIndirectHeap(HeapType::dynamicState, MemoryConstants::pageSize64k);
    auto csrSshHeap = &ultCsr.getIndirectHeap(HeapType::surfaceState, MemoryConstants::pageSize64k);

    size_t dshUsed = csrDshHeap->getUsed();
    size_t sshUsed = csrSshHeap->getUsed();

    size_t csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrUsedAfter = csrStream.getUsed();

    NEO::IndirectHeap *containerDshHeap = cmdContainer.getIndirectHeap(HeapType::dynamicState);
    NEO::IndirectHeap *containerSshHeap = cmdContainer.getIndirectHeap(HeapType::surfaceState);

    if (this->dshRequired) {
        EXPECT_EQ(csrDshHeap, containerDshHeap);
    } else {
        EXPECT_EQ(nullptr, containerDshHeap);
    }
    EXPECT_EQ(csrSshHeap, containerSshHeap);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        (csrUsedAfter - csrUsedBefore)));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto &sbaCmd = *genCmdCast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);
    if (this->dshRequired) {
        EXPECT_TRUE(sbaCmd.getDynamicStateBaseAddressModifyEnable());
        EXPECT_EQ(csrDshHeap->getHeapGpuBase(), sbaCmd.getDynamicStateBaseAddress());
    } else {
        EXPECT_FALSE(sbaCmd.getDynamicStateBaseAddressModifyEnable());
        EXPECT_EQ(0u, sbaCmd.getDynamicStateBaseAddress());
    }
    EXPECT_TRUE(sbaCmd.getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(csrSshHeap->getHeapGpuBase(), sbaCmd.getSurfaceStateBaseAddress());

    dshUsed = csrDshHeap->getUsed() - dshUsed;
    sshUsed = csrSshHeap->getUsed() - sshUsed;
    if (this->dshRequired) {
        EXPECT_LT(0u, dshUsed);
    } else {
        EXPECT_EQ(0u, dshUsed);
    }
    EXPECT_LT(0u, sshUsed);

    size_t dshEstimated = NEO::EncodeDispatchKernel<FamilyType>::getSizeRequiredDsh(kernel->getKernelDescriptor(), cmdContainer.getNumIddPerBlock());
    size_t sshEstimated = NEO::EncodeDispatchKernel<FamilyType>::getSizeRequiredSsh(*kernel->getImmutableData()->getKernelInfo());

    EXPECT_GE(dshEstimated, dshUsed);
    EXPECT_GE(sshEstimated, sshUsed);

    auto &cmdContainerCoexisting = commandListImmediateCoexisting->commandContainer;
    EXPECT_EQ(1u, cmdContainerCoexisting.getNumIddPerBlock());
    EXPECT_TRUE(cmdContainerCoexisting.immediateCmdListSharedHeap(HeapType::dynamicState));
    EXPECT_TRUE(cmdContainerCoexisting.immediateCmdListSharedHeap(HeapType::surfaceState));

    dshUsed = csrDshHeap->getUsed();
    sshUsed = csrSshHeap->getUsed();

    csrUsedBefore = csrStream.getUsed();
    result = commandListImmediateCoexisting->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    csrUsedAfter = csrStream.getUsed();

    auto containerDshHeapCoexisting = cmdContainerCoexisting.getIndirectHeap(HeapType::dynamicState);
    auto containerSshHeapCoexisting = cmdContainerCoexisting.getIndirectHeap(HeapType::surfaceState);

    size_t dshAlignment = NEO::EncodeDispatchKernel<FamilyType>::getDefaultDshAlignment();
    size_t sshAlignment = NEO::EncodeDispatchKernel<FamilyType>::getDefaultSshAlignment();

    void *ptr = containerSshHeapCoexisting->getSpace(0);
    size_t expectedSshAlignedSize = sshEstimated + ptrDiff(alignUp(ptr, sshAlignment), ptr);

    size_t expectedDshAlignedSize = dshEstimated;
    if (this->dshRequired) {
        ptr = containerDshHeapCoexisting->getSpace(0);
        expectedDshAlignedSize += ptrDiff(alignUp(ptr, dshAlignment), ptr);

        EXPECT_EQ(csrDshHeap, containerDshHeapCoexisting);
    } else {
        EXPECT_EQ(nullptr, containerDshHeapCoexisting);
    }
    EXPECT_EQ(csrSshHeap, containerSshHeapCoexisting);

    cmdList.clear();
    sbaCmds.clear();

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        (csrUsedAfter - csrUsedBefore)));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, sbaCmds.size());

    dshUsed = csrDshHeap->getUsed() - dshUsed;
    sshUsed = csrSshHeap->getUsed() - sshUsed;

    if (this->dshRequired) {
        EXPECT_LT(0u, dshUsed);
    } else {
        EXPECT_EQ(0u, dshUsed);
    }
    EXPECT_LT(0u, sshUsed);

    EXPECT_GE(expectedDshAlignedSize, dshUsed);
    EXPECT_GE(expectedSshAlignedSize, sshUsed);
}

using CommandListStateBaseAddressGlobalStatelessTest = Test<CommandListGlobalHeapsFixture<static_cast<int32_t>(NEO::HeapAddressModel::globalStateless)>>;
HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest, givenGlobalStatelessWhenExecutingCommandListThenMakeAllocationResident, IsAtLeastXeHpCore) {
    EXPECT_EQ(NEO::HeapAddressModel::globalStateless, commandList->cmdListHeapAddressModel);
    EXPECT_EQ(NEO::HeapAddressModel::globalStateless, commandListImmediate->cmdListHeapAddressModel);
    EXPECT_EQ(NEO::HeapAddressModel::globalStateless, commandQueue->cmdListHeapAddressModel);

    ASSERT_EQ(commandListImmediate->csr, commandQueue->getCsr());
    auto globalStatelessAlloc = commandListImmediate->csr->getGlobalStatelessHeapAllocation();
    EXPECT_NE(nullptr, globalStatelessAlloc);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandListImmediate->csr);
    ultCsr->storeMakeResidentAllocations = true;

    commandList->close();

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    auto result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(ultCsr->isMadeResident(globalStatelessAlloc));
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessWhenExecutingRegularCommandListThenBaseAddressPropertiesSetCorrectlyAndCommandProperlyDispatched,
          IsAtLeastXeHpCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &container = commandList->getCmdContainer();

    auto statlessMocs = getMocs(true);
    auto ioBaseAddress = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto &requiredState = commandList->requiredStreamState.stateBaseAddress;
    auto &finalState = commandList->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), requiredState.statelessMocs.value);

    EXPECT_EQ(-1, requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.surfaceStateSize.value);
    EXPECT_EQ(-1, requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, requiredState.indirectObjectSize.value);

    EXPECT_EQ(-1, requiredState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.bindingTablePoolSize.value);

    EXPECT_EQ(finalState.surfaceStateBaseAddress.value, requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalState.surfaceStateSize.value, requiredState.surfaceStateSize.value);

    EXPECT_EQ(finalState.dynamicStateBaseAddress.value, requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalState.dynamicStateSize.value, requiredState.dynamicStateSize.value);

    EXPECT_EQ(finalState.indirectObjectBaseAddress.value, requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(finalState.indirectObjectSize.value, requiredState.indirectObjectSize.value);

    EXPECT_EQ(finalState.bindingTablePoolBaseAddress.value, requiredState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(finalState.bindingTablePoolSize.value, requiredState.bindingTablePoolSize.value);

    EXPECT_EQ(finalState.globalAtomics.value, requiredState.globalAtomics.value);
    EXPECT_EQ(finalState.statelessMocs.value, requiredState.statelessMocs.value);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    auto globalSurfaceHeap = commandQueue->getCsr()->getGlobalStatelessHeap();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();
    auto ssSize = globalSurfaceHeap->getHeapSizeInPages();

    auto &csrState = commandQueue->getCsr()->getStreamProperties().stateBaseAddress;

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, csrState.surfaceStateSize.value);

    EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(-1, csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());

    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioBaseAddress);
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessWhenExecutingImmediateCommandListThenBaseAddressPropertiesSetCorrectlyAndCommandProperlyDispatched,
          IsAtLeastXeHpCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;
    auto &csrState = csrImmediate.getStreamProperties().stateBaseAddress;
    auto globalSurfaceHeap = csrImmediate.getGlobalStatelessHeap();

    size_t csrUsedBefore = csrStream.getUsed();
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrUsedAfter = csrStream.getUsed();

    auto &container = commandListImmediate->getCmdContainer();
    auto statlessMocs = getMocs(true);
    auto ioBaseAddress = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();
    auto ssSize = globalSurfaceHeap->getHeapSizeInPages();

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, csrState.surfaceStateSize.value);

    EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(-1, csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());

    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioBaseAddress);
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessWhenExecutingRegularCommandListAndImmediateCommandListThenBaseAddressPropertiesSetCorrectlyAndCommandProperlyDispatchedOnlyOnce,
          IsAtLeastXeHpCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &container = commandList->getCmdContainer();

    auto statlessMocs = getMocs(true);
    auto ioBaseAddress = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    auto globalSurfaceHeap = commandQueue->getCsr()->getGlobalStatelessHeap();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());

    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioBaseAddress);
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;

    size_t csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrUsedAfter = csrStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(0u, sbaCmds.size());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessWhenExecutingImmediateCommandListAndRegularCommandListThenBaseAddressPropertiesSetCorrectlyAndCommandProperlyDispatchedOnlyOnce,
          IsAtLeastXeHpCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;
    auto globalSurfaceHeap = csrImmediate.getGlobalStatelessHeap();

    size_t csrUsedBefore = csrStream.getUsed();
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrUsedAfter = csrStream.getUsed();

    auto &container = commandListImmediate->getCmdContainer();
    auto statlessMocs = getMocs(true);
    auto ioBaseAddress = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());

    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioBaseAddress);
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(0u, sbaCmds.size());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessWhenExecutingRegularCommandListAndPrivateHeapsCommandListThenBaseAddressPropertiesSetCorrectlyAndCommandProperlyDispatched,
          IsAtLeastXeHpCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &container = commandList->getCmdContainer();

    auto statlessMocs = getMocs(true);
    auto ioBaseAddress = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto &requiredState = commandList->requiredStreamState.stateBaseAddress;
    auto &finalState = commandList->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), requiredState.statelessMocs.value);

    EXPECT_EQ(-1, requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.surfaceStateSize.value);
    EXPECT_EQ(-1, requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, requiredState.indirectObjectSize.value);

    EXPECT_EQ(-1, requiredState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.bindingTablePoolSize.value);

    EXPECT_EQ(finalState.surfaceStateBaseAddress.value, requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalState.surfaceStateSize.value, requiredState.surfaceStateSize.value);

    EXPECT_EQ(finalState.dynamicStateBaseAddress.value, requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalState.dynamicStateSize.value, requiredState.dynamicStateSize.value);

    EXPECT_EQ(finalState.indirectObjectBaseAddress.value, requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(finalState.indirectObjectSize.value, requiredState.indirectObjectSize.value);

    EXPECT_EQ(finalState.bindingTablePoolBaseAddress.value, requiredState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(finalState.bindingTablePoolSize.value, requiredState.bindingTablePoolSize.value);

    EXPECT_EQ(finalState.globalAtomics.value, requiredState.globalAtomics.value);
    EXPECT_EQ(finalState.statelessMocs.value, requiredState.statelessMocs.value);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    auto globalSurfaceHeap = commandQueue->getCsr()->getGlobalStatelessHeap();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();
    auto ssSize = globalSurfaceHeap->getHeapSizeInPages();

    auto &csrState = commandQueue->getCsr()->getStreamProperties().stateBaseAddress;

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, csrState.surfaceStateSize.value);

    EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(-1, csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());
    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioBaseAddress);
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    result = commandListPrivateHeap->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &containerPrivateHeap = commandListPrivateHeap->getCmdContainer();

    auto sshPrivateHeap = containerPrivateHeap.getIndirectHeap(NEO::HeapType::surfaceState);
    auto ssBaseAddressPrivateHeap = sshPrivateHeap->getHeapGpuBase();
    auto ssSizePrivateHeap = sshPrivateHeap->getHeapSizeInPages();

    uint64_t dsBaseAddressPrivateHeap = -1;
    size_t dsSizePrivateHeap = static_cast<size_t>(-1);

    auto dshPrivateHeap = containerPrivateHeap.getIndirectHeap(NEO::HeapType::dynamicState);
    if (!this->dshRequired) {
        EXPECT_EQ(nullptr, dshPrivateHeap);
    } else {
        EXPECT_NE(nullptr, dshPrivateHeap);
    }
    if (dshPrivateHeap) {
        dsBaseAddressPrivateHeap = dshPrivateHeap->getHeapGpuBase();
        dsSizePrivateHeap = dshPrivateHeap->getHeapSizeInPages();
    }

    auto &requiredStatePrivateHeap = commandListPrivateHeap->requiredStreamState.stateBaseAddress;
    auto &finalStatePrivateHeap = commandListPrivateHeap->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), requiredStatePrivateHeap.statelessMocs.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), requiredStatePrivateHeap.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, requiredStatePrivateHeap.surfaceStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), requiredStatePrivateHeap.dynamicStateBaseAddress.value);
    EXPECT_EQ(dsSizePrivateHeap, requiredStatePrivateHeap.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), requiredStatePrivateHeap.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, requiredStatePrivateHeap.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), requiredStatePrivateHeap.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, requiredStatePrivateHeap.bindingTablePoolSize.value);

    EXPECT_EQ(finalStatePrivateHeap.surfaceStateBaseAddress.value, requiredStatePrivateHeap.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.surfaceStateSize.value, requiredStatePrivateHeap.surfaceStateSize.value);

    EXPECT_EQ(finalStatePrivateHeap.dynamicStateBaseAddress.value, requiredStatePrivateHeap.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.dynamicStateSize.value, requiredStatePrivateHeap.dynamicStateSize.value);

    EXPECT_EQ(finalStatePrivateHeap.indirectObjectBaseAddress.value, requiredStatePrivateHeap.indirectObjectBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.indirectObjectSize.value, requiredStatePrivateHeap.indirectObjectSize.value);

    EXPECT_EQ(finalStatePrivateHeap.bindingTablePoolBaseAddress.value, requiredStatePrivateHeap.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.bindingTablePoolSize.value, requiredStatePrivateHeap.bindingTablePoolSize.value);

    EXPECT_EQ(finalStatePrivateHeap.globalAtomics.value, requiredStatePrivateHeap.globalAtomics.value);
    EXPECT_EQ(finalStatePrivateHeap.statelessMocs.value, requiredStatePrivateHeap.statelessMocs.value);

    result = commandListPrivateHeap->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    queueBefore = cmdQueueStream.getUsed();
    cmdListHandle = commandListPrivateHeap->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    queueAfter = cmdQueueStream.getUsed();

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.surfaceStateSize.value);

    if (dshPrivateHeap) {
        EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(dsSizePrivateHeap, csrState.dynamicStateSize.value);
    } else {
        EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);
    }

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    if (dshPrivateHeap) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsBaseAddressPrivateHeap, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsSizePrivateHeap, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddressPrivateHeap, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessWhenExecutingPrivateHeapsCommandListAndRegularCommandListThenBaseAddressPropertiesSetCorrectlyAndCommandProperlyDispatched,
          IsAtLeastXeHpCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    auto result = commandListPrivateHeap->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &containerPrivateHeap = commandListPrivateHeap->getCmdContainer();

    auto statlessMocs = getMocs(true);
    auto ioBaseAddress = containerPrivateHeap.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = containerPrivateHeap.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto sshPrivateHeap = containerPrivateHeap.getIndirectHeap(NEO::HeapType::surfaceState);
    auto ssBaseAddressPrivateHeap = sshPrivateHeap->getHeapGpuBase();
    auto ssSizePrivateHeap = sshPrivateHeap->getHeapSizeInPages();

    uint64_t dsBaseAddressPrivateHeap = -1;
    size_t dsSizePrivateHeap = static_cast<size_t>(-1);

    auto dshPrivateHeap = containerPrivateHeap.getIndirectHeap(NEO::HeapType::dynamicState);
    if (!this->dshRequired) {
        EXPECT_EQ(nullptr, dshPrivateHeap);
    } else {
        EXPECT_NE(nullptr, dshPrivateHeap);
    }
    if (dshPrivateHeap) {
        dsBaseAddressPrivateHeap = dshPrivateHeap->getHeapGpuBase();
        dsSizePrivateHeap = dshPrivateHeap->getHeapSizeInPages();
    }

    auto &requiredStatePrivateHeap = commandListPrivateHeap->requiredStreamState.stateBaseAddress;
    auto &finalStatePrivateHeap = commandListPrivateHeap->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), requiredStatePrivateHeap.statelessMocs.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), requiredStatePrivateHeap.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, requiredStatePrivateHeap.surfaceStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), requiredStatePrivateHeap.dynamicStateBaseAddress.value);
    EXPECT_EQ(dsSizePrivateHeap, requiredStatePrivateHeap.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), requiredStatePrivateHeap.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, requiredStatePrivateHeap.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), requiredStatePrivateHeap.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, requiredStatePrivateHeap.bindingTablePoolSize.value);

    EXPECT_EQ(finalStatePrivateHeap.surfaceStateBaseAddress.value, requiredStatePrivateHeap.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.surfaceStateSize.value, requiredStatePrivateHeap.surfaceStateSize.value);

    EXPECT_EQ(finalStatePrivateHeap.dynamicStateBaseAddress.value, requiredStatePrivateHeap.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.dynamicStateSize.value, requiredStatePrivateHeap.dynamicStateSize.value);

    EXPECT_EQ(finalStatePrivateHeap.indirectObjectBaseAddress.value, requiredStatePrivateHeap.indirectObjectBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.indirectObjectSize.value, requiredStatePrivateHeap.indirectObjectSize.value);

    EXPECT_EQ(finalStatePrivateHeap.bindingTablePoolBaseAddress.value, requiredStatePrivateHeap.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.bindingTablePoolSize.value, requiredStatePrivateHeap.bindingTablePoolSize.value);

    EXPECT_EQ(finalStatePrivateHeap.globalAtomics.value, requiredStatePrivateHeap.globalAtomics.value);
    EXPECT_EQ(finalStatePrivateHeap.statelessMocs.value, requiredStatePrivateHeap.statelessMocs.value);

    result = commandListPrivateHeap->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    auto &csrState = commandQueue->getCsr()->getStreamProperties().stateBaseAddress;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandListPrivateHeap->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.surfaceStateSize.value);

    if (dshPrivateHeap) {
        EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(dsSizePrivateHeap, csrState.dynamicStateSize.value);
    } else {
        EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);
    }

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    if (dshPrivateHeap) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsBaseAddressPrivateHeap, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsSizePrivateHeap, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddressPrivateHeap, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());
    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioBaseAddress);
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &requiredState = commandList->requiredStreamState.stateBaseAddress;
    auto &finalState = commandList->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), requiredState.statelessMocs.value);

    EXPECT_EQ(-1, requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.surfaceStateSize.value);
    EXPECT_EQ(-1, requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, requiredState.indirectObjectSize.value);

    EXPECT_EQ(-1, requiredState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), requiredState.bindingTablePoolSize.value);

    EXPECT_EQ(finalState.surfaceStateBaseAddress.value, requiredState.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalState.surfaceStateSize.value, requiredState.surfaceStateSize.value);

    EXPECT_EQ(finalState.dynamicStateBaseAddress.value, requiredState.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalState.dynamicStateSize.value, requiredState.dynamicStateSize.value);

    EXPECT_EQ(finalState.indirectObjectBaseAddress.value, requiredState.indirectObjectBaseAddress.value);
    EXPECT_EQ(finalState.indirectObjectSize.value, requiredState.indirectObjectSize.value);

    EXPECT_EQ(finalState.bindingTablePoolBaseAddress.value, requiredState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(finalState.bindingTablePoolSize.value, requiredState.bindingTablePoolSize.value);

    EXPECT_EQ(finalState.globalAtomics.value, requiredState.globalAtomics.value);
    EXPECT_EQ(finalState.statelessMocs.value, requiredState.statelessMocs.value);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    queueBefore = cmdQueueStream.getUsed();
    cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    queueAfter = cmdQueueStream.getUsed();

    auto globalSurfaceHeap = commandQueue->getCsr()->getGlobalStatelessHeap();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();
    auto ssSize = globalSurfaceHeap->getHeapSizeInPages();

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, csrState.surfaceStateSize.value);

    if (dshPrivateHeap) {
        EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(dsSizePrivateHeap, csrState.dynamicStateSize.value);
    } else {
        EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);
    }

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    if (dshPrivateHeap) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsBaseAddressPrivateHeap, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsSizePrivateHeap, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessWhenExecutingImmediateCommandListAndPrivateHeapsCommandListThenBaseAddressPropertiesSetCorrectlyAndCommandProperlyDispatched,
          IsAtLeastXeHpCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;
    auto &csrState = csrImmediate.getStreamProperties().stateBaseAddress;
    auto globalSurfaceHeap = csrImmediate.getGlobalStatelessHeap();

    size_t csrUsedBefore = csrStream.getUsed();
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrUsedAfter = csrStream.getUsed();

    auto &container = commandListImmediate->getCmdContainer();
    auto statlessMocs = getMocs(true);
    auto ioBaseAddress = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = container.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();
    auto ssSize = globalSurfaceHeap->getHeapSizeInPages();

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, csrState.surfaceStateSize.value);

    EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(-1, csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());

    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioBaseAddress);
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    result = commandListPrivateHeap->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &containerPrivateHeap = commandListPrivateHeap->getCmdContainer();

    auto sshPrivateHeap = containerPrivateHeap.getIndirectHeap(NEO::HeapType::surfaceState);
    auto ssBaseAddressPrivateHeap = sshPrivateHeap->getHeapGpuBase();
    auto ssSizePrivateHeap = sshPrivateHeap->getHeapSizeInPages();

    uint64_t dsBaseAddressPrivateHeap = -1;
    size_t dsSizePrivateHeap = static_cast<size_t>(-1);

    auto dshPrivateHeap = containerPrivateHeap.getIndirectHeap(NEO::HeapType::dynamicState);
    if (!this->dshRequired) {
        EXPECT_EQ(nullptr, dshPrivateHeap);
    } else {
        EXPECT_NE(nullptr, dshPrivateHeap);
    }
    if (dshPrivateHeap) {
        dsBaseAddressPrivateHeap = dshPrivateHeap->getHeapGpuBase();
        dsSizePrivateHeap = dshPrivateHeap->getHeapSizeInPages();
    }

    auto &requiredStatePrivateHeap = commandListPrivateHeap->requiredStreamState.stateBaseAddress;
    auto &finalStatePrivateHeap = commandListPrivateHeap->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), requiredStatePrivateHeap.statelessMocs.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), requiredStatePrivateHeap.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, requiredStatePrivateHeap.surfaceStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), requiredStatePrivateHeap.dynamicStateBaseAddress.value);
    EXPECT_EQ(dsSizePrivateHeap, requiredStatePrivateHeap.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), requiredStatePrivateHeap.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, requiredStatePrivateHeap.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), requiredStatePrivateHeap.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, requiredStatePrivateHeap.bindingTablePoolSize.value);

    EXPECT_EQ(finalStatePrivateHeap.surfaceStateBaseAddress.value, requiredStatePrivateHeap.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.surfaceStateSize.value, requiredStatePrivateHeap.surfaceStateSize.value);

    EXPECT_EQ(finalStatePrivateHeap.dynamicStateBaseAddress.value, requiredStatePrivateHeap.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.dynamicStateSize.value, requiredStatePrivateHeap.dynamicStateSize.value);

    EXPECT_EQ(finalStatePrivateHeap.indirectObjectBaseAddress.value, requiredStatePrivateHeap.indirectObjectBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.indirectObjectSize.value, requiredStatePrivateHeap.indirectObjectSize.value);

    EXPECT_EQ(finalStatePrivateHeap.bindingTablePoolBaseAddress.value, requiredStatePrivateHeap.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.bindingTablePoolSize.value, requiredStatePrivateHeap.bindingTablePoolSize.value);

    EXPECT_EQ(finalStatePrivateHeap.globalAtomics.value, requiredStatePrivateHeap.globalAtomics.value);
    EXPECT_EQ(finalStatePrivateHeap.statelessMocs.value, requiredStatePrivateHeap.statelessMocs.value);

    result = commandListPrivateHeap->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandListPrivateHeap->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.surfaceStateSize.value);

    if (dshPrivateHeap) {
        EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(dsSizePrivateHeap, csrState.dynamicStateSize.value);
    } else {
        EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);
    }

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    if (dshPrivateHeap) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsBaseAddressPrivateHeap, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsSizePrivateHeap, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddressPrivateHeap, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessWhenExecutingPrivateHeapsCommandListAndImmediateCommandListThenBaseAddressPropertiesSetCorrectlyAndCommandProperlyDispatched,
          IsAtLeastXeHpCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    auto result = commandListPrivateHeap->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &containerPrivateHeap = commandListPrivateHeap->getCmdContainer();

    auto statlessMocs = getMocs(true);
    auto ioBaseAddress = containerPrivateHeap.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapGpuBase();
    auto ioSize = containerPrivateHeap.getIndirectHeap(NEO::HeapType::indirectObject)->getHeapSizeInPages();

    auto sshPrivateHeap = containerPrivateHeap.getIndirectHeap(NEO::HeapType::surfaceState);
    auto ssBaseAddressPrivateHeap = sshPrivateHeap->getHeapGpuBase();
    auto ssSizePrivateHeap = sshPrivateHeap->getHeapSizeInPages();

    uint64_t dsBaseAddressPrivateHeap = -1;
    size_t dsSizePrivateHeap = static_cast<size_t>(-1);

    auto dshPrivateHeap = containerPrivateHeap.getIndirectHeap(NEO::HeapType::dynamicState);
    if (!this->dshRequired) {
        EXPECT_EQ(nullptr, dshPrivateHeap);
    } else {
        EXPECT_NE(nullptr, dshPrivateHeap);
    }
    if (dshPrivateHeap) {
        dsBaseAddressPrivateHeap = dshPrivateHeap->getHeapGpuBase();
        dsSizePrivateHeap = dshPrivateHeap->getHeapSizeInPages();
    }

    auto &requiredStatePrivateHeap = commandListPrivateHeap->requiredStreamState.stateBaseAddress;
    auto &finalStatePrivateHeap = commandListPrivateHeap->finalStreamState.stateBaseAddress;

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), requiredStatePrivateHeap.statelessMocs.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), requiredStatePrivateHeap.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, requiredStatePrivateHeap.surfaceStateSize.value);
    EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), requiredStatePrivateHeap.dynamicStateBaseAddress.value);
    EXPECT_EQ(dsSizePrivateHeap, requiredStatePrivateHeap.dynamicStateSize.value);

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), requiredStatePrivateHeap.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, requiredStatePrivateHeap.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), requiredStatePrivateHeap.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, requiredStatePrivateHeap.bindingTablePoolSize.value);

    EXPECT_EQ(finalStatePrivateHeap.surfaceStateBaseAddress.value, requiredStatePrivateHeap.surfaceStateBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.surfaceStateSize.value, requiredStatePrivateHeap.surfaceStateSize.value);

    EXPECT_EQ(finalStatePrivateHeap.dynamicStateBaseAddress.value, requiredStatePrivateHeap.dynamicStateBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.dynamicStateSize.value, requiredStatePrivateHeap.dynamicStateSize.value);

    EXPECT_EQ(finalStatePrivateHeap.indirectObjectBaseAddress.value, requiredStatePrivateHeap.indirectObjectBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.indirectObjectSize.value, requiredStatePrivateHeap.indirectObjectSize.value);

    EXPECT_EQ(finalStatePrivateHeap.bindingTablePoolBaseAddress.value, requiredStatePrivateHeap.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(finalStatePrivateHeap.bindingTablePoolSize.value, requiredStatePrivateHeap.bindingTablePoolSize.value);

    EXPECT_EQ(finalStatePrivateHeap.globalAtomics.value, requiredStatePrivateHeap.globalAtomics.value);
    EXPECT_EQ(finalStatePrivateHeap.statelessMocs.value, requiredStatePrivateHeap.statelessMocs.value);

    result = commandListPrivateHeap->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    auto &csrState = commandQueue->getCsr()->getStreamProperties().stateBaseAddress;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandListPrivateHeap->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.surfaceStateSize.value);

    if (dshPrivateHeap) {
        EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(dsSizePrivateHeap, csrState.dynamicStateSize.value);
    } else {
        EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);
    }

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    if (dshPrivateHeap) {
        EXPECT_TRUE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsBaseAddressPrivateHeap, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(dsSizePrivateHeap, sbaCmd->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
        EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());
    }

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddressPrivateHeap, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());
    auto ioBaseAddressDecanonized = neoDevice->getGmmHelper()->decanonize(ioBaseAddress);
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;
    auto globalSurfaceHeap = csrImmediate.getGlobalStatelessHeap();

    size_t csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrUsedAfter = csrStream.getUsed();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();
    auto ssSize = globalSurfaceHeap->getHeapSizeInPages();

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddress), csrState.surfaceStateBaseAddress.value);
    EXPECT_EQ(ssSize, csrState.surfaceStateSize.value);

    if (dshPrivateHeap) {
        EXPECT_EQ(static_cast<int64_t>(dsBaseAddressPrivateHeap), csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(dsSizePrivateHeap, csrState.dynamicStateSize.value);
    } else {
        EXPECT_EQ(-1, csrState.dynamicStateBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), csrState.dynamicStateSize.value);
    }

    EXPECT_EQ(static_cast<int64_t>(ioBaseAddress), csrState.indirectObjectBaseAddress.value);
    EXPECT_EQ(ioSize, csrState.indirectObjectSize.value);

    EXPECT_EQ(static_cast<int64_t>(ssBaseAddressPrivateHeap), csrState.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(ssSizePrivateHeap, csrState.bindingTablePoolSize.value);

    EXPECT_EQ(static_cast<int32_t>(statlessMocs), csrState.statelessMocs.value);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_FALSE(sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd->getDynamicStateBufferSize());

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd->getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd->getGeneralStateBufferSizeModifyEnable());
    EXPECT_EQ(ioBaseAddressDecanonized, sbaCmd->getGeneralStateBaseAddress());
    EXPECT_EQ(ioSize, sbaCmd->getGeneralStateBufferSize());

    EXPECT_EQ((statlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessKernelUsingScratchSpaceWhenExecutingRegularCommandListThenBaseAddressAndFrontEndStateCommandsProperlyDispatched,
          IsAtLeastXeHpCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using CFE_STATE = typename FamilyType::CFE_STATE;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x100;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueAfter = cmdQueueStream.getUsed();

    auto globalSurfaceHeap = commandQueue->getCsr()->getGlobalStatelessHeap();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdQueueStream.getCpuBase(), queueBefore),
        queueAfter - queueBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    auto frontEndCmds = findAll<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, frontEndCmds.size());

    constexpr size_t expectedScratchOffset = 2 * sizeof(RENDER_SURFACE_STATE);

    auto frontEndCmd = reinterpret_cast<CFE_STATE *>(*frontEndCmds[0]);
    EXPECT_EQ(expectedScratchOffset, frontEndCmd->getScratchSpaceBuffer());

    auto scratchSpaceController = commandQueue->csr->getScratchSpaceController();
    EXPECT_EQ(expectedScratchOffset, scratchSpaceController->getScratchPatchAddress());

    auto surfaceStateHeapAlloc = globalSurfaceHeap->getGraphicsAllocation();
    void *scratchSurfaceStateBuffer = ptrOffset(surfaceStateHeapAlloc->getUnderlyingBuffer(), expectedScratchOffset);
    auto scratchSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(scratchSurfaceStateBuffer);

    auto scratchAllocation = scratchSpaceController->getScratchSpaceSlot0Allocation();
    EXPECT_EQ(scratchAllocation->getGpuAddress(), scratchSurfaceState->getSurfaceBaseAddress());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenGlobalStatelessKernelUsingScratchSpaceWhenExecutingImmediateCommandListThenBaseAddressAndFrontEndStateCommandsProperlyDispatched,
          IsAtLeastXeHpCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using CFE_STATE = typename FamilyType::CFE_STATE;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x100;

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;
    auto globalSurfaceHeap = csrImmediate.getGlobalStatelessHeap();

    size_t csrUsedBefore = csrStream.getUsed();
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrUsedAfter = csrStream.getUsed();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());

    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    auto frontEndCmds = findAll<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, frontEndCmds.size());

    constexpr size_t expectedScratchOffset = 2 * sizeof(RENDER_SURFACE_STATE);

    auto frontEndCmd = reinterpret_cast<CFE_STATE *>(*frontEndCmds[0]);
    EXPECT_EQ(expectedScratchOffset, frontEndCmd->getScratchSpaceBuffer());

    auto scratchSpaceController = commandQueue->csr->getScratchSpaceController();
    EXPECT_EQ(expectedScratchOffset, scratchSpaceController->getScratchPatchAddress());

    auto surfaceStateHeapAlloc = globalSurfaceHeap->getGraphicsAllocation();
    void *scratchSurfaceStateBuffer = ptrOffset(surfaceStateHeapAlloc->getUnderlyingBuffer(), expectedScratchOffset);
    auto scratchSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(scratchSurfaceStateBuffer);

    auto scratchAllocation = scratchSpaceController->getScratchSpaceSlot0Allocation();
    EXPECT_EQ(scratchAllocation->getGpuAddress(), scratchSurfaceState->getSurfaceBaseAddress());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenCommandListNotUsingPrivateSurfaceHeapWhenCommandListDestroyedThenCsrDoesNotDispatchStateCacheFlush,
          IsAtLeastXeHpCore) {
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csr.commandStream;

    ze_result_t returnValue;
    L0::ult::CommandList *cmdListObject = CommandList::whiteboxCast(CommandList::create(productFamily, device, engineGroupType, 0u, returnValue, false));

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    cmdListObject->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    returnValue = cmdListObject->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto cmdListHandle = cmdListObject->toHandle();
    returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    returnValue = cmdListObject->destroy();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_EQ(0u, csrStream.getUsed());
}

HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenCommandListUsingGlobalHeapsWhenCommandListCreatedThenNoStateHeapAllocationsCreated,
          IsAtLeastXeHpCore) {
    auto &container = commandList->getCmdContainer();

    auto ssh = container.getIndirectHeap(NEO::HeapType::surfaceState);
    EXPECT_EQ(nullptr, ssh);

    auto dsh = container.getIndirectHeap(NEO::HeapType::dynamicState);
    EXPECT_EQ(nullptr, dsh);
}
HWTEST2_F(CommandListStateBaseAddressGlobalStatelessTest,
          givenKernelUsingStatefulAccessWhenAppendingKernelOnGlobalStatelessThenExpectError,
          IsAtLeastXeHpCore) {
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs.resize(1);

    auto ptrArg = ArgDescriptor(ArgDescriptor::argTPointer);
    ptrArg.as<ArgDescPointer>().bindless = 0x40;
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[0] = ptrArg;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    ptrArg.as<ArgDescPointer>().bindless = undefined<CrossThreadDataOffset>;
    ptrArg.as<ArgDescPointer>().bindful = 0x40;
    mockKernelImmData->kernelDescriptor->payloadMappings.explicitArgs[0] = ptrArg;

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

} // namespace ult
} // namespace L0
