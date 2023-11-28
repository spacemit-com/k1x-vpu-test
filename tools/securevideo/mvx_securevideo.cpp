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

#include "mvx_securevideo.hpp"

#include <fcntl.h>
#include <optee/sedget_video_ta.h>
#include <sedget_video.h>
#include <sys/ioctl.h>
#include <tee_client_api_extensions.h>
#include <unistd.h>

#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

namespace MVXSecure {
/****************************************************************************
 * Exception
 ****************************************************************************/

Exception::Exception(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
}

Exception::Exception(const string &str) {
  strncpy(msg, str.c_str(), sizeof(msg));
}

Exception::~Exception() throw() {}

const char *Exception::what() const throw() { return msg; }

/****************************************************************************
 * Session
 ****************************************************************************/

Session::Session() {
  TEEC_Result res = TEEC_InitializeContext(NULL, &context);
  if (res != TEEC_SUCCESS) {
    throw Exception("Failed to initialize context.");
  }

  TEEC_UUID uuid = SEDGET_VIDEO_TA_UUID;
  uint32_t err;
  res = TEEC_OpenSession(&context, &session, &uuid, TEEC_LOGIN_PUBLIC, NULL,
                         NULL, &err);
  if (res != TEEC_SUCCESS) {
    throw Exception("Failed to open session.");
  }
}

Session::~Session() {
  TEEC_CloseSession(&session);
  TEEC_FinalizeContext(&context);
}

Firmware *Session::loadFirmware(const char *fw, unsigned int cores) {
  Firmware *firmware = new Firmware(fw, cores, context, session);
  const FirmwareSecureDescriptor &firmwareDescriptor =
      firmware->getFirmwareDescriptor();

  cout << "Loaded firmware. major=" << (int)firmwareDescriptor.version.major
       << ", minor=" << (int)firmwareDescriptor.version.minor
       << ", l2pages=" << hex << firmwareDescriptor.l2pages << dec << endl;

  return firmware;
}

BufferION *Session::allocateBuffer(const BufferION::BufferType type,
                                   const std::size_t size) {
  return new BufferION(type, size, context);
}

/****************************************************************************
 * Firmware
 ****************************************************************************/

Firmware::Firmware(const char *file, const unsigned int ncores,
                   TEEC_Context &context, TEEC_Session &session)
    : ncores(ncores), context(context), session(session), secureBuffer(0) {
  vector<char> buffer;
  readFirmwareBinary(file, buffer);

  secureBuffer =
      new BufferION(BufferION::BUFFER_TYPE_FIRMWARE, SIZE_4M, context);

  loadFirmware(buffer);
}

Firmware::~Firmware() { delete secureBuffer; }

const FirmwareSecureDescriptor &Firmware::getFirmwareDescriptor() const {
  return firmwareDescriptor;
}

int Firmware::getFileDescriptor() const {
  return secureBuffer->getFileDescriptor();
}

void Firmware::readFirmwareBinary(const char *file, vector<char> &buffer) {
  /* Open file and get size of firmware binary. */
  ifstream ifs(file, ios::binary | ios::ate);
  streamsize size = ifs.tellg();
  ifs.seekg(0, ios::beg);

  buffer.resize(size);
  if (!ifs.read(buffer.data(), size)) {
    throw Exception("Failed to read '%s' into buffer.", file);
  }
}

void Firmware::loadFirmware(vector<char> &buffer) {
  TEEC_Operation op = {0};
  op.paramTypes =
      TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_PARTIAL_OUTPUT,
                       TEEC_MEMREF_TEMP_OUTPUT, TEEC_VALUE_INPUT);
  op.params[0].tmpref.buffer = &buffer[0];
  op.params[0].tmpref.size = buffer.size();
  op.params[1].memref.parent = &secureBuffer->getSharedMemory();
  op.params[1].memref.size = secureBuffer->getSize();
  op.params[1].memref.offset = 0;
  op.params[2].tmpref.buffer = &firmwareDescriptor;
  op.params[2].tmpref.size = sizeof(firmwareDescriptor);
  op.params[3].value.a = ncores;
  op.params[3].value.b = 0;

  uint32_t err;
  TEEC_Result result =
      TEEC_InvokeCommand(&session, SEDGET_VIDEO_TA_CMD_LOAD_FW, &op, &err);
  if (result != TEEC_SUCCESS) {
    throw Exception("TA Load firmware failed. result=0x%x, error=%u.", result,
                    err);
  }
}

/****************************************************************************
 * BufferION
 ****************************************************************************/

BufferION::BufferION(const BufferType type, const size_t size,
                     TEEC_Context &context)
    : context(context), size(size), fd(-1) {
  allocateBuffer(type);
  registerSharedMemory();
}

BufferION::~BufferION() {
  releaseSharedMemory();

  if (fd >= 0) {
    close(fd);
  }
}

size_t BufferION::getSize() const { return size; }

int BufferION::getFileDescriptor() const { return fd; }

TEEC_SharedMemory &BufferION::getSharedMemory() { return sharedMemory; }

void BufferION::allocateBuffer(const BufferType type) {
  int ionfd = open("/dev/ion", O_RDWR);
  if (ionfd < 0) {
    throw Exception("Failed to open ion device.");
  }

  struct ion_allocation_data allocData = {.len = size};

  switch (type) {
    case BUFFER_TYPE_INPUT:
      allocData.heap_id_mask = 1 << (ION_HEAP_TYPE_CUSTOM + 1);
      break;
    case BUFFER_TYPE_OUTPUT:
      allocData.heap_id_mask = 1 << (ION_HEAP_TYPE_CUSTOM + 2);
      break;
    case BUFFER_TYPE_FIRMWARE:
      allocData.heap_id_mask = 1 << ION_HEAP_TYPE_CUSTOM;
      break;
    default:
      throw Exception("Unsupported ION buffer type. type=%d.", type);
  }

  if (ioctl(ionfd, ION_IOC_ALLOC, &allocData) < 0) {
    throw Exception("ION_IOC_ALLOC failed. errno=%d (%s).", errno,
                    strerror(errno));
  }

#ifdef ION_IOC_MAP
  ion_user_handle_t handle = allocData.handle;

  struct ion_fd_data fdData = {.handle = handle, .fd = -1};
  if (ioctl(ionfd, ION_IOC_MAP, &fdData) < 0) {
    throw Exception("ION_IOC_MAP failed. errno=%d (%s).", errno,
                    strerror(errno));
  }

  fd = fdData.fd;

  struct ion_handle_data handleData = {.handle = handle};
  if (ioctl(ionfd, ION_IOC_FREE, &handleData) < 0) {
    throw Exception("ION_IOC_FREE failed. errno=%d (%s).", errno,
                    strerror(errno));
  }
#else
  fd = allocData.fd;
#endif

  close(ionfd);
}

void BufferION::registerSharedMemory() {
  sharedMemory.buffer = 0;
  sharedMemory.size = 0;
  sharedMemory.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;

  TEEC_Result result =
      TEEC_RegisterSharedMemoryFileDescriptor(&context, &sharedMemory, fd);
  if (result != TEEC_SUCCESS) {
    throw Exception("TEEC_RegisterMemoryFileDescriptor failed. result=0x%x.",
                    result);
  }
}

void BufferION::releaseSharedMemory() {
  TEEC_ReleaseSharedMemory(&sharedMemory);
}
}  // namespace MVXSecure