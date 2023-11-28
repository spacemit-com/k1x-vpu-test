/*
 * The confidential and proprietary information contained in this file may
 * only be used by a person authorised under and to the extent permitted
 * by a subsisting licensing agreement from Arm Technology (China) Co., Ltd.
 *
 *            (C) COPYRIGHT 2021-2021 Arm Technology (China) Co., Ltd.
 *                ALL RIGHTS RESERVED
 *
 * This entire notice must be reproduced on all copies of this file
 * and copies of this file may only be made by a person if such person is
 * permitted to do so under the terms of a subsisting license agreement
 * from Arm Technology (China) Co., Ltd.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 */

#ifndef __MVE_EMUL_H__
#define __MVE_EMUL_H__

/****************************************************************************
 * Includes
 ****************************************************************************/

#include <pthread.h>

#include "exec/address-spaces.h"
#include "exec/memory.h"
#include "hw/hw.h"
#include "qemu/osdep.h"
#include "qemu/timer.h"

/****************************************************************************
 * Defines
 ****************************************************************************/

#define MVE_LSID_MAX 4

/****************************************************************************
 * Types
 ****************************************************************************/

struct memory_map;

typedef void (*mve_irq_t)(void *arg);

typedef struct MVEEmulState {
  mve_irq_t irq_cb;
  void *irq_arg;
  unsigned int lsid_to_id[MVE_LSID_MAX];
  struct memory_map *lsid_to_mmap[MVE_LSID_MAX];
  unsigned int tmp_lsid;
  hwaddr tmp_l0;
  pthread_t tid;
  bool running;
} MVEEmulState;

/****************************************************************************
 * Exported functions
 ****************************************************************************/

uint64_t mve_ioread(void *opaque, hwaddr addr, unsigned size);

void mve_iowrite(void *opaque, hwaddr addr, uint64_t value, unsigned size);

uint64_t mve_mmioread(void *opaque, hwaddr addr, unsigned size);

void mve_mmiowrite(void *opaque, hwaddr addr, uint64_t value, unsigned size);

/**
 *
 */
int mve_emul_construct(MVEEmulState *state, mve_irq_t irq_cb, void *irq_arg,
                       bool gdb_enable);

/**
 *
 */
void mve_emul_destruct(MVEEmulState *state);

#endif /* __MVE_EMUL_H__ */
