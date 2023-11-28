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

#include <fcntl.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <iostream>

#include "mvx_securevideo.hpp"

using namespace std;
using namespace MVXSecure;

/****************************************************************************
 * Types
 ****************************************************************************/

#pragma pack(push, 1)

struct FirmwareProtocol {
  int32_t fd;
  uint32_t l2pages;
  struct {
    uint32_t major;
    uint32_t minor;
  } protocol;
};

struct MemoryProtocol {
  int32_t fd;
};

#pragma pack(pop)

/****************************************************************************
 * Functions
 ****************************************************************************/

static void help(const char *exe) {
  cerr << "Usage: " << exe << " COMMAND [ARGS]" << endl << endl;
  cerr << "Command:" << endl;
  cerr << "    firmware - Load firmware binary." << endl;
  cerr << "    memory   - Allocate memory." << endl;
  cerr << "    help     - This help message." << endl;
}

static void helpFirmware(const char *exe) {
  cerr << "Usage: " << exe << " firmware BINARY DST NUMCORES" << endl << endl;
  cerr << "Args:" << endl;
  cerr << "    BINARY  Encrypted firmware binary." << endl;
  cerr << "    DST     Destination where to write the firmware descriptor."
       << endl;
}

static int loadFirmware(const char *exe, int argc, char **argv) {
  if (argc != 3) {
    cerr << "Error: Incorrect number of arguments." << endl << endl;
    helpFirmware(exe);
    return 1;
  }

  const char *firmwareBinary = argv[0];
  const char *descriptor = argv[1];
  int cores = atoi(argv[2]);

  int fd = open(descriptor, O_WRONLY);
  if (fd < 0) {
    cerr << "Error: Failed to open '" << descriptor << "'." << endl;
    return 1;
  }

  FirmwareProtocol fw = {.fd = -1};

  try {
    Session session;
    Firmware *firmware = session.loadFirmware(firmwareBinary, cores);
    const FirmwareSecureDescriptor &fsd = firmware->getFirmwareDescriptor();

    fw.fd = firmware->getFileDescriptor();
    fw.l2pages = fsd.l2pages;
    fw.protocol.major = fsd.version.major;
    fw.protocol.minor = fsd.version.minor;
  } catch (...) {
    cerr << "Failed to encrypt secure firmware." << endl;
  }

  ssize_t n = write(fd, &fw, sizeof(fw));
  if (n != sizeof(fw)) {
    cerr << "Error: Failed to write firmware descriptor." << endl;
    return 1;
  }

  close(fd);

  return 0;
}

static void helpMemory(const char *exe) {
  cerr << "Usage: " << exe << " memory SIZE DST" << endl << endl;
  cerr << "Arguments:" << endl;
  cerr << "    SIZE    Size in bytes." << endl;
  cerr << "    DST     Destination where to write the memory descriptor."
       << endl;
}

static int allocateMemory(const char *exe, int argc, char **argv) {
  if (argc != 2) {
    cerr << "Error: Incorrect number of arguments." << endl << endl;
    helpMemory(exe);
    return 1;
  }

  int size = atoi(argv[0]);
  const char *descriptor = argv[1];

  if (size <= 0) {
    cerr << "Error: Size must be a positive integer." << endl;
    return 1;
  }

  int fd = open(descriptor, O_WRONLY);
  if (fd < 0) {
    cerr << "Error: Failed to open '" << descriptor << "'." << endl;
    return 1;
  }

  MemoryProtocol mem = {.fd = -1};

  try {
    Session session;
    BufferION *buffer =
        session.allocateBuffer(BufferION::BUFFER_TYPE_FIRMWARE, size);
    mem.fd = buffer->getFileDescriptor();
  } catch (...) {
    cerr << "Failed to allocate secure memory." << endl;
  }

  ssize_t n = write(fd, &mem, sizeof(mem));
  if (n != sizeof(mem)) {
    cerr << "Error: Failed to write memory descriptor." << endl;
    return 1;
  }

  close(fd);

  return 0;
}

int main(int argc, char **argv) {
  int ret = 0;

  if (argc < 2) {
    cerr << "Error: Too few arguments." << endl << endl;
    help(argv[0]);
    return 1;
  }

  string command(argv[1]);
  if (command == "firmware") {
    ret = loadFirmware(argv[0], argc - 2, &argv[2]);
  } else if (command == "memory") {
    ret = allocateMemory(argv[0], argc - 2, &argv[2]);
  } else if (command == "-h" || command == "--help" || command == "help") {
    help(argv[0]);
    return 1;
  } else {
    cerr << "Error: Unsupported argument: " << argv[1] << endl << endl;
    help(argv[0]);
    return 1;
  }

  return ret;
}
