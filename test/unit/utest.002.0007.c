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
#CODE-EXTRACT: driver/ngCore/mvx_if.c::validate_mvx_notify_device

#DESCRIPTION: validate_mvx_notify_device should return false if
#DESCRIPTION: data is null.
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
struct device;
struct drv_data {};

void* dev_get_drvdata(struct device* dev) { return NULL; }

/**********************************************************
 * Include artefact under test
 **********************************************************/
#include "extracted-code"

int main(int argc, char** argv) {
  int n, i;
  int reply = -1;
  struct device* test_ptr;
  void* data;

  /* Create a fake data-ptr that is null*/
  data = NULL;
  /* Create a fake device-ptr that is null. */
  test_ptr = (struct device*)&test_ptr;

  /* Do the check */
  if (false == validate_mvx_notify_device(test_ptr, data)) {
    reply = 0;
  }

  return reply;
}
