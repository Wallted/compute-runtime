/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds_glk.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_and_later.inl"

#include "platforms.h"

constexpr static auto gfxProduct = IGFX_GEMINILAKE;

#include "shared/source/gen9/glk/os_agnostic_hw_info_config_glk.inl"

template class NEO::ProductHelperHw<gfxProduct>;
