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

#include "mve_emul.h"

#include <emul/core_toplevel.h>
#include <emul/mve_memory_map.h>
#include <emul/rasc_session_context.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/****************************************************************************
 * Defines
 ****************************************************************************/

#define MVE_TIMER_TICK 100
#define MVE_PAGE_SHIFT 12
#define MVE_TLB_SIZE (1 << (32 - MVE_PAGE_SHIFT)) /* 4 GB linear lookup. */
#define MVE_UNMAPPED (unsigned int)-1

/****************************************************************************
 * Types
 ****************************************************************************/

typedef struct mve_map {
  struct mve_map *next;
  uint32_t va;
  unsigned int size;
  void *ptr;
} mve_map;

struct memory_map {
  t_mve_config *config;
  t_coremanager *coreman;
  t_mve_init_data *init_data;
  t_rasc_session_context *rasc_session;
  unsigned int lsid;
  hwaddr l0;
  hwaddr tlb[MVE_TLB_SIZE];
  mve_map *map;
};

/****************************************************************************
 * Model memory interface
 ****************************************************************************/

t_mve_memory_map *rascemul_allocate_memory_map(t_mve_config *config,
                                               t_coremanager *coreman,
                                               t_mve_init_data *init_data) {
  MVEEmulState *state = init_data->callback_ctx;
  t_mve_memory_map *mmap;

  mmap = calloc(1, sizeof(*mmap));
  if (mmap == NULL) return NULL;

  mmap->config = config;
  mmap->coreman = coreman;
  mmap->init_data = init_data;
  mmap->rasc_session = rasc_session_create();
  mmap->lsid = state->tmp_lsid;
  mmap->l0 = state->tmp_l0;

  state->lsid_to_mmap[state->tmp_lsid] = mmap;

  return mmap;
}

void rascemul_remove_memory_map(t_mve_memory_map *mmap) {
  MVEEmulState *state = mmap->init_data->callback_ctx;

  rasc_session_destroy(mmap->rasc_session);

  while (mmap->map != NULL) {
    mve_map *next = mmap->map->next;
    free(mmap->map->ptr);
    free(mmap->map);
    mmap->map = next;
  }

  state->lsid_to_mmap[mmap->lsid] = NULL;

  free(mmap);
}

t_rasc_session_context *load_rasc_session_context(t_mve_memory_map *mm) {
  return mm->rasc_session;
}

static unsigned int get_attr(uint32_t pte) { return (pte >> 30) & 0xf; }

static unsigned int get_access(uint32_t pte) { return pte & 0xf; }

static hwaddr get_addr(uint32_t pte) {
  return (((hwaddr)pte >> 2) & 0xfffffff) << 12;
}

static unsigned int get_pte_offset(uint32_t va, unsigned level) {
  return (va >> ((1 - level) * 10 + 12)) & ((1 << 10) - 1);
}

static unsigned int get_page_offset(uint32_t va) { return va & 0xfff; }

static uint32_t set_pte(hwaddr addr, unsigned attr, unsigned access) {
  return ((attr & 0x3) << 30) | (((addr >> 12) & (0xfffffff)) << 2) |
         (access & 0x3);
}

static hwaddr mve_ptw(hwaddr l0, uint32_t va, bool assert_page_fault) {
  uint32_t pte;
  unsigned o0, o1;
  hwaddr l1, l2;

  if (l0 == 0) {
    assert(assert_page_fault == false);
    return 0;
  }

  o0 = get_pte_offset(va, 0);
  cpu_physical_memory_read(l0 + o0 * sizeof(pte), &pte, sizeof(pte));
  l1 = get_addr(pte);
  if (l1 == 0) {
    assert(assert_page_fault == false);
    return 0;
  }

  o1 = get_pte_offset(va, 1);
  cpu_physical_memory_read(l1 + o1 * sizeof(pte), &pte, sizeof(pte));
  l2 = get_addr(pte);
  if (l2 == 0) {
    assert(assert_page_fault == false);
    return 0;
  }

  return l2;
}

static hwaddr mve_tlb_lookup(t_mve_memory_map *mmap, uint32_t addr) {
  unsigned int offset = addr >> MVE_PAGE_SHIFT;

  if (offset >= MVE_TLB_SIZE) {
    hwaddr pa = mve_ptw(mmap->l0, addr, false);
    return (pa == 0) ? 0 : pa + get_page_offset(addr);
  }

  if (mmap->tlb[offset] == 0) {
    mmap->tlb[offset] = mve_ptw(mmap->l0, addr, false);
    if (mmap->tlb[offset] == 0) return 0;
  }

  return mmap->tlb[offset] + get_page_offset(addr);
}

static void mve_tlb_flush(t_mve_memory_map *mmap) {
  memset(mmap->tlb, 0, sizeof(mmap->tlb));
}

void *rascemul_translate_rasc_mem(t_mve_memory_map *mmap, uint32_t addr,
                                  unsigned int size, address_callback f) {
  mve_map *map;
  uint8_t *p;
  uint8_t *end;

  /* Search if map already exists. */
  map = mmap->map;
  while (map != NULL)
    if (map->va == addr && map->size >= size) return map->ptr;

  /* Create a new map. */
  map = calloc(1, sizeof(*map));
  if (map == NULL) return NULL;

  map->ptr = calloc(1, size);
  if (map->ptr == NULL) {
    free(map);
    return NULL;
  }

  map->va = addr;
  map->size = size;
  map->next = mmap->map;
  mmap->map = map;

  p = map->ptr;
  end = p + size;
  while (p < end) {
    hwaddr pa = mve_tlb_lookup(mmap, addr);
    if (pa != 0) cpu_physical_memory_read(pa, p, sizeof(*p));

    addr += sizeof(*p);
    p++;
  }

  return map->ptr;
}

uint32_t mve_memory_read8(t_mve_memory_map *mmap, bool is_axi, uint32_t addr,
                          address_callback f) {
  hwaddr pa;
  uint8_t value;

  pa = mve_tlb_lookup(mmap, addr);
  if (pa == 0) {
    t_mve_core *core = coremanager_get_core(mmap->coreman, 0);
    mve_core_memory_fault(core);
  }

  cpu_physical_memory_read(pa, &value, sizeof(value));

  return value;
}

void mve_memory_write8(t_mve_memory_map *mmap, uint32_t addr, uint8_t val,
                       address_callback f) {
  hwaddr pa;

  pa = mve_tlb_lookup(mmap, addr);
  if (pa == 0) {
    t_mve_core *core = coremanager_get_core(mmap->coreman, 0);
    mve_core_memory_fault(core);
  }

  cpu_physical_memory_write(pa, &val, sizeof(val));
}

void mve_memory_log_axi_access(t_mve_memory_map *mm, t_mve_core *core,
                               t_axi_cookie cookie, uint32_t addr, size_t len,
                               size_t bsize, bool wrap, bool read) {}

/****************************************************************************
 * Static functions
 ****************************************************************************/

static void mve_irq(void *ctx) {
  MVEEmulState *mve = ctx;

  if (mve->irq_cb != NULL) mve->irq_cb(mve->irq_arg);
}

static void mve_log(void *ctx, const char *str) {
  printf("%s: %s\n", __FUNCTION__, str);
}

static void mve_axilog(void *ctx, uint32_t cookie, uint64_t cycle, bool write,
                       uint64_t addr, size_t len, size_t bsize, bool wrap) {
  printf("%s\n", __FUNCTION__);
}

static void *mve_thread(void *arg) {
  MVEEmulState *state = arg;

  state->running = true;
  while (state->running != false) mveemul_run(true);

  return NULL;
}

/****************************************************************************
 * Exported functions
 ****************************************************************************/

uint64_t mve_ioread(void *opaque, hwaddr addr, unsigned size) {
  printf("%s\n", __FUNCTION__);
  return 0;
}

void mve_iowrite(void *opaque, hwaddr addr, uint64_t value, unsigned size) {
  printf("%s\n", __FUNCTION__);
}

uint64_t mve_mmioread(void *opaque, hwaddr addr, unsigned size) {
  return mveemul_read32((void *)addr);
}

void mve_mmiowrite(void *opaque, hwaddr addr, uint64_t value, unsigned size) {
  MVEEmulState *state = opaque;
  const unsigned lsid_offset = 0x0200;
  const unsigned lsid_size = 0x40;

  if (addr >= lsid_offset) {
    unsigned lsid = (addr - lsid_offset) / lsid_size;
    unsigned offset = addr - lsid_offset - lsid * lsid_size;

    switch (offset) {
      /* MMU_CTRL */
      case 0x4: {
        unsigned int attr = get_attr(value);
        unsigned int access = get_access(value);
        hwaddr l0 = get_addr(value);
        int ret;

        if (state->lsid_to_id[lsid] != MVE_UNMAPPED) {
          mveemul_remove_memory_map(state->lsid_to_id[lsid]);
          state->lsid_to_id[lsid] = MVE_UNMAPPED;
        }

        state->tmp_l0 = l0;
        ret = mveemul_allocate_memory_map(&state->lsid_to_id[lsid]);
        if (ret != MVEEMUL_RET_OK) {
          printf("Failed to allocate memory map id.\n");
          return;
        }

        value = set_pte(state->lsid_to_id[lsid], attr, access);
        break;
      }
      /* ALLOC */
      case 0xc: {
        /* Deallocate session. */
        if (value == 0)
          if (state->lsid_to_id[lsid] != MVE_UNMAPPED) {
            mveemul_remove_memory_map(state->lsid_to_id[lsid]);
            state->lsid_to_id[lsid] = MVE_UNMAPPED;
          }

        break;
      }
      /* FLUSH_ALL */
      case 0x10: {
        if (state->lsid_to_mmap[lsid] != NULL)
          mve_tlb_flush(state->lsid_to_mmap[lsid]);

        break;
      }
      default:
        break;
    }
  }

  mveemul_write32((void *)addr, value);
}

int mve_emul_construct(MVEEmulState *state, mve_irq_t irq_cb, void *irq_arg,
                       bool gdb_enable) {
  struct mveemul_init_data_1 init;
  int i;
  int ret;

  state->irq_cb = irq_cb;
  state->irq_arg = irq_arg;

  for (i = 0; i < MVE_LSID_MAX; i++) state->lsid_to_id[i] = MVE_UNMAPPED;

  ret = mveemul_configure("ncores=1");
  if (ret != MVEEMUL_RET_OK) {
    printf("Failed to configure ncores.\n");
    return -1;
  }

  ret = mveemul_configure("clock-quanta=50");
  if (ret != MVEEMUL_RET_OK) {
    printf("Failed to configure clock-quanta.\n");
    return -1;
  }

  if (gdb_enable != false) {
    ret = mveemul_configure("gdb-enable=yes\ngdb-port=8888");
    if (ret != MVEEMUL_RET_OK) {
      printf("Failed to configure GDB.\n");
      return -1;
    }
  }

  init.mve_base_addr = NULL;
  init.configmask = 0xffffffff;
  init.callback_ctx = state;
  init.irq_callback = mve_irq;
  init.log_callback = mve_log;
  init.axi_callback = mve_axilog;

  ret = mveemul_init(1, &init);
  if (ret != MVEEMUL_RET_OK) {
    printf("Failed to initialize emulator.\n");
    return -1;
  }

  ret = pthread_create(&state->tid, NULL, mve_thread, state);
  if (ret != 0) return ret;

  return 0;
}

void mve_emul_destruct(MVEEmulState *state) {
  state->running = false;
  pthread_join(state->tid, NULL);
}
