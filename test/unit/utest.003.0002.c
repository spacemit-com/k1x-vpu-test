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

/*
#CODE-EXTRACT: driver/ngCore/mvx_discovery.c::mvx_discover

#DESCRIPTION: mvx_discover should validate that the bus is not null.
*/

#include <stdlib.h>

#ifndef bool
typedef int bool;
#define false (0)
#define true (1)
#endif

/**********************************************************
 * Facades
 **********************************************************/

struct bus_type;

struct device {
  struct bus_type *bus;
};
struct mvx_discovery {
  struct device *source;
};

bool called = false;
bool correctly = false;

static int mvx_bus_walker(struct device *dev, void *data) {}

struct mvx_discovery discovery;

int bus_for_each_dev(struct bus_type *bus, struct device *start, void *data,
                     int (*fn)(struct device *dev, void *data)) {
  called = true;

  if (NULL != bus && NULL == start && data == &discovery &&
      mvx_bus_walker == fn) {
    correctly = true;
  }
}

/**********************************************************
 * Include artefact under test
 **********************************************************/
#include "extracted-code"

int main(int argc, char **argv) {
  int reply = -1;
  struct device test_device;

  /* Create facade data */
  discovery.source = &test_device;
  test_device.bus = (struct bus_type *)&test_device.bus;

  /* Method is declared void so no return value to check.
   */
  mvx_discover(&discovery);

  if (false != called && false != correctly) {
    reply = 0;
  }

  return reply;
}
