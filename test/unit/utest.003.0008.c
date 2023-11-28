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
#CODE-EXTRACT: driver/ngCore/mvx_discovery.c::mvx_bus_walker
#CODE-EXTRACT: driver/ngCore/mvx_discovery.c::mvx_discovery
#CODE-EXTRACT: driver/ngCore/mvx_discovery.c::mvx_device_notifications
#CODE-EXTRACT: driver/ngCore/mvx_discovery.c::mvx_discovery_target

#DESCRIPTION: mvx_bus_walker should validate the driver-data ptr for the target.
*/

#include <stdlib.h>
#include <string.h>

#ifndef bool
typedef int bool;
#define false (0)
#define true (1)
#endif

/**********************************************************
 * Facades
 **********************************************************/

struct driver {
  char const* name;
};

struct device {
  struct driver* driver;
  void* drvdata;
};

void* dev_get_drvdata(struct device* dev) { return dev->drvdata; }

/**********************************************************
 * Include artefact under test
 **********************************************************/
#include "extracted-code"

int main(int argc, char** argv) {
  int reply = -1;
  struct mvx_discovery discovery;
  struct device target;
  struct device source;
  struct driver driver;
  struct mvx_discovery_target target_discovery_target;

  char const* matches[2] = {"Driver-name", NULL};
  discovery.matches = matches;
  discovery.source = &source;

  target.driver = &driver;
  driver.name = "Driver-name";
  target_discovery_target.notify = NULL;
  target.drvdata = &target_discovery_target;

  source.driver = NULL;
  source.drvdata = NULL;

  /* Method is declared void so no return value to check.
   */
  if (0 == mvx_bus_walker(&target, &discovery)) {
    reply = 0;
  }

  return reply;
}
