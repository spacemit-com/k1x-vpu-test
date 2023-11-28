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

#include <linux/ion.h>
#include <optee/sedget_video_ta.h>
#include <tee_client_api.h>

#include <cstddef>
#include <exception>
#include <string>
#include <vector>

namespace MVXSecure {
/****************************************************************************
 * Exception
 ****************************************************************************/

class Exception : public std::exception {
 public:
  Exception(const char *fmt, ...);
  Exception(const std::string &str);
  virtual ~Exception() throw();

  virtual const char *what() const throw();

 private:
  char msg[100];
};

/****************************************************************************
 * BufferION
 ****************************************************************************/

class BufferION {
 public:
  enum BufferType {
    BUFFER_TYPE_INPUT,
    BUFFER_TYPE_OUTPUT,
    BUFFER_TYPE_FIRMWARE
  };

  BufferION(const BufferType type, const std::size_t size,
            TEEC_Context &context);
  ~BufferION();

  std::size_t getSize() const;
  int getFileDescriptor() const;
  TEEC_SharedMemory &getSharedMemory();

 private:
  TEEC_Context &context;
  TEEC_SharedMemory sharedMemory;
  const std::size_t size;
  int fd;

  void allocateBuffer(const BufferType type);
  void registerSharedMemory();
  void releaseSharedMemory();
};

/****************************************************************************
 * Firmware
 ****************************************************************************/

struct FirmwareVersion {
  uint8_t major; /**< Firmware major version. */
  uint8_t minor; /**< Firmware minor version. */
};

struct FirmwareSecureDescriptor {
  FirmwareVersion version; /**< FW protocol version */
  uint32_t l2pages; /**< Physical address of l2pages created by secure OS */
};

class Firmware {
 public:
  Firmware(const char *file, const unsigned int ncores, TEEC_Context &context,
           TEEC_Session &session);
  ~Firmware();

  const FirmwareSecureDescriptor &getFirmwareDescriptor() const;
  int getFileDescriptor() const;

 private:
  const unsigned int ncores;
  TEEC_Context &context;
  TEEC_Session &session;
  BufferION *secureBuffer;
  FirmwareSecureDescriptor firmwareDescriptor;

  static const std::size_t SIZE_4M = 0x400000;

  void readFirmwareBinary(const char *file, std::vector<char> &buffer);
  void loadFirmware(std::vector<char> &buffer);
};

/****************************************************************************
 * Session
 ****************************************************************************/

class Session {
 public:
  Session();
  ~Session();

  Firmware *loadFirmware(const char *fw, unsigned int cores);
  BufferION *allocateBuffer(const BufferION::BufferType type,
                            const std::size_t size);

 private:
  TEEC_Context context;
  TEEC_Session session;
};
}  // namespace MVXSecure
