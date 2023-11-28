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

/****************************************************************************
 * Includes
 ****************************************************************************/

#include "emul/mveemul.h"
#include "hw/hw.h"
#include "hw/pci/pci.h"
#include "memory.h"
#include "mve_emul.h"
#include "qapi/error.h"
#include "qemu/osdep.h"
#include "qemu/timer.h"

/****************************************************************************
 * Defines
 ****************************************************************************/

#define TYPE_MVE_DEV "mve_pci"
#define MVE_DEVICE(obj) OBJECT_CHECK(MVEState, (obj), TYPE_MVE_DEV)
#define MVE_IO_SIZE (1 << 6)
#define MVE_MMIO_SIZE (1 << 11)

/****************************************************************************
 * Types
 ****************************************************************************/

typedef struct MVEState {
  PCIDevice parent_obj;

  MVEEmulState state;
  MemoryRegion io;
  MemoryRegion mmio;
  bool gdb_enable;
} MVEState;

/****************************************************************************
 * Static functions
 ****************************************************************************/

static void mve_pci_irq(void *arg) {
  MVEState *mve = arg;

  pci_irq_assert(&mve->parent_obj);
}

static const MemoryRegionOps mve_mmio_ops = {.read = mve_mmioread,
                                             .write = mve_mmiowrite,
                                             .endianness = DEVICE_NATIVE_ENDIAN,
                                             .valid = {
                                                 .min_access_size = 4,
                                                 .max_access_size = 4,
                                             }};

static const MemoryRegionOps mve_io_ops = {.read = mve_ioread,
                                           .write = mve_iowrite,
                                           .endianness = DEVICE_NATIVE_ENDIAN,
                                           .valid = {
                                               .min_access_size = 4,
                                               .max_access_size = 4,
                                           }};

static void mve_pci_realize(PCIDevice *pci_dev, Error **errp) {
  MVEState *mve = MVE_DEVICE(pci_dev);
  uint8_t *pci_conf = pci_dev->config;
  int ret;

  ret = mve_emul_construct(&mve->state, mve_pci_irq, mve, mve->gdb_enable);
  if (ret != 0) {
    error_setg(errp, "Failed to initialize MVE model.");
    return;
  }

  memory_region_init_io(&mve->io, OBJECT(mve), &mve_io_ops, &mve->state,
                        "mve_io", MVE_IO_SIZE);
  pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_IO, &mve->io);

  memory_region_init_io(&mve->mmio, OBJECT(mve), &mve_mmio_ops, &mve->state,
                        "mve_mmio", MVE_MMIO_SIZE);
  pci_register_bar(pci_dev, 1, PCI_BASE_ADDRESS_SPACE_MEMORY, &mve->mmio);

  pci_conf[PCI_INTERRUPT_PIN] = 0x02;
}

static void mve_pci_uninit(PCIDevice *pci_dev) {
  MVEState *mve = MVE_DEVICE(pci_dev);

  mve_emul_destruct(&mve->state);
}

static Property mve_pci_properties[] = {
    DEFINE_PROP_BOOL("gdb-enable", MVEState, gdb_enable, false),
    DEFINE_PROP_END_OF_LIST()};

static void mve_class_init(ObjectClass *klass, void *data) {
  DeviceClass *dc = DEVICE_CLASS(klass);
  PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

  k->realize = mve_pci_realize;
  k->exit = mve_pci_uninit;

  k->vendor_id = 0x13b5; /* ARM Ltd */
  k->device_id = 0x0001;
  k->class_id = PCI_CLASS_MULTIMEDIA_VIDEO;
  set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);

  k->revision = 0x00;
  dc->desc = "Linlon Video Engine";
  dc->props = mve_pci_properties;
}

static const TypeInfo mve_info = {
    .name = TYPE_MVE_DEV,
    .parent = TYPE_PCI_DEVICE,
    .class_init = mve_class_init,
    .instance_size = sizeof(MVEState),
    .interfaces = (InterfaceInfo[]){{INTERFACE_CONVENTIONAL_PCI_DEVICE}, {}}};

static void mve_register_types(void) { type_register_static(&mve_info); }

type_init(mve_register_types)
