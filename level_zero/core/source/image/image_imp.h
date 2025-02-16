/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/surface_format_info.h"

#include "level_zero/core/source/image/image.h"

#include <memory>
#include <optional>

namespace L0 {

struct ImageImp : public Image, NEO::NonCopyableOrMovableClass {
    ze_result_t destroy() override;
    ze_result_t destroyPeerImages(const void *ptr, Device *device) override;

    virtual ze_result_t initialize(Device *device, const ze_image_desc_t *desc) = 0;

    ~ImageImp() override;

    NEO::GraphicsAllocation *getAllocation() override { return allocation; }
    NEO::GraphicsAllocation *getImplicitArgsAllocation() override { return implicitArgsAllocation; }
    NEO::ImageInfo getImageInfo() override { return imgInfo; }
    ze_image_desc_t getImageDesc() override {
        return imageFormatDesc;
    }

    ze_result_t createView(Device *device, const ze_image_desc_t *desc, ze_image_handle_t *pImage) override;

    ze_result_t getMemoryProperties(ze_image_memory_properties_exp_t *pMemoryProperties) override {
        pMemoryProperties->rowPitch = imgInfo.rowPitch;
        pMemoryProperties->slicePitch = imgInfo.slicePitch;
        pMemoryProperties->size = imgInfo.surfaceFormat->imageElementSizeInBytes;

        return ZE_RESULT_SUCCESS;
    }

    bool isImageView() const {
        return sourceImageFormatDesc.has_value();
    }

    ze_result_t allocateBindlessSlot() override;
    NEO::SurfaceStateInHeapInfo *getBindlessSlot() override;

  protected:
    Device *device = nullptr;
    NEO::ImageInfo imgInfo = {};
    NEO::GraphicsAllocation *allocation = nullptr;
    NEO::GraphicsAllocation *implicitArgsAllocation = nullptr;
    ze_image_desc_t imageFormatDesc = {};
    std::optional<ze_image_desc_t> sourceImageFormatDesc = {};
    std::unique_ptr<NEO::SurfaceStateInHeapInfo> bindlessInfo;
};
} // namespace L0
