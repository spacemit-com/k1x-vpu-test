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

#include "mvx_player.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

#include "md5.h"

using namespace std;

/****************************************************************************
 * Defines
 ****************************************************************************/
// Define bit field extract and test macros
// SBFX = signed bitfield extract
//  BFS = bit field set
#define SBFX(Rn, lsb, width) \
  ((int32_t)((Rn) << (32 - (lsb) - (width))) >> (32 - (width)))
#define BFS(Rn, lsb, width) (((Rn) & ((1u << (width)) - 1)) << (lsb))

// Extract macros as above but with width macro automatically generated
#define SET(lsb, Rn) BFS(Rn, lsb, lsb##_SZ)
#define SGET(lsb, Rn) SBFX(Rn, lsb, lsb##_SZ)

#ifndef V4L2_BUF_FLAG_LAST
#define V4L2_BUF_FLAG_LAST 0x00100000
#endif

#define V4L2_ALLOCATE_BUFFER_ROI 1048576 * 3
#define V4L2_READ_LEN_BUFFER_ROI 1048576 * 2
#define MAX_PRE_STORE_BUFFER_SIZE 1048576 * 50

#define INPUT_NUM_BUFFERS 3
#define OUTPUT_EXTRA_NUM_BUFFERS 3

#ifndef V4L2_EVENT_SOURCE_CHANGE
#define V4L2_EVENT_SOURCE_CHANGE 5
#endif

typedef reader::result result;
/****************************************************************************
 * Misc
 ****************************************************************************/

template <typename T, typename U>
static T divRoundUp(T value, U round) {
  return (value + round - 1) / round;
}

template <typename T, typename U>
static T roundUp(T value, U round) {
  return divRoundUp(value, round) * round;
}

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
  memset(msg, '\0', sizeof(msg));
  strncpy(msg, str.c_str(), sizeof(msg) - 1);
}

Exception::~Exception() throw() {}

const char *Exception::what() const throw() { return msg; }

/****************************************************************************
 * Input and output
 ****************************************************************************/

const uint32_t IVFHeader::signatureDKIF = v4l2_fourcc('D', 'K', 'I', 'F');
static const uint8_t startCode[4] = {0x00, 0x00, 0x00, 0x01};
static const uint8_t subStartCode[3] = {0x00, 0x00, 0x01};
static std::mutex m_Mutex;

// COnfiguration file maximum line lengths, for use by fgets()
// ROI requires 512
// ERP requires 10+1 + 7+1 + 11+1 + (22+1)*256 = 5919 (8k width)
#define CFG_FILE_LINE_SIZE (6144)
static char cfg_file_line_buf[CFG_FILE_LINE_SIZE];

static int startcode_find_candidate(char *buf, int size);

IVFHeader::IVFHeader()
    : signature(signatureDKIF),
      version(0),
      length(32),
      codec(0),
      width(0),
      height(0),
      frameRate(30 << 16),
      timeScale(1 << 16),
      frameCount(0),
      padding(0) {}

IVFHeader::IVFHeader(uint32_t codec, uint16_t width, uint16_t height)
    : signature(signatureDKIF),
      version(0),
      length(32),
      codec(codec),
      width(width),
      height(height),
      frameRate(30 << 16),
      timeScale(1 << 16),
      frameCount(0),
      padding(0) {}

IVFFrame::IVFFrame() : size(0), timestamp(0) {}

IVFFrame::IVFFrame(uint32_t size, uint64_t timestamp)
    : size(size), timestamp(timestamp) {}

const uint8_t VC1SequenceLayerData::magic1 = 0xC5;
const uint32_t VC1SequenceLayerData::magic2 = 4;

VC1SequenceLayerData::VC1SequenceLayerData()
    : numFrames(0), signature1(0), signature2(0), headerC(), restOfSLD() {}

VC1FrameLayerData::VC1FrameLayerData()
    : frameSize(0), reserved(0), key(0), timestamp(0) {}

AFBCHeader::AFBCHeader() {}

AFBCHeader::AFBCHeader(const v4l2_format &format, size_t frameSize,
                       const v4l2_crop &crop, bool tiled, const int field)
    : magic(MAGIC),
      headerSize(sizeof(AFBCHeader)),
      version(VERSION),
      frameSize(frameSize),
      numComponents(0),
      subsampling(0),
      yuvTransform(false),
      blockSplit(false),
      yBits(0),
      cbBits(0),
      crBits(0),
      alphaBits(0),
      mbWidth(0),
      mbHeight(0),
      width(0),
      height(0),
      cropLeft(crop.c.left),
      cropTop(crop.c.top),
      param(0),
      fileMessage(field) {
  uint32_t pixelformat;

  if (V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
    const v4l2_pix_format_mplane &f = format.fmt.pix_mp;

    width = f.width;
    height = f.height;
    pixelformat = f.pixelformat;
  } else {
    const v4l2_pix_format &f = format.fmt.pix;

    width = f.width;
    height = f.height;
    pixelformat = f.pixelformat;
  }

  if (field != FIELD_NONE) {
    height = divRoundUp(height, 2);
  }
  mbWidth = (width + cropLeft + 15) / 16;
  mbHeight = (height + cropTop + 15) / 16;

  switch (pixelformat) {
    case V4L2_PIX_FMT_YUV420_AFBC_8:
      numComponents = 3;
      subsampling = 1;
      yBits = 8;
      crBits = 8;
      cbBits = 8;
      alphaBits = 0;
      break;
    case V4L2_PIX_FMT_YUV420_AFBC_10:
      numComponents = 3;
      subsampling = 1;
      yBits = 10;
      crBits = 10;
      cbBits = 10;
      alphaBits = 0;
      break;
    case V4L2_PIX_FMT_YUV422_AFBC_8:
      numComponents = 3;
      subsampling = 2;
      yBits = 8;
      crBits = 8;
      cbBits = 8;
      alphaBits = 0;
      break;
    case V4L2_PIX_FMT_YUV422_AFBC_10:
      numComponents = 3;
      subsampling = 2;
      yBits = 10;
      crBits = 10;
      cbBits = 10;
      alphaBits = 0;
      break;
    default:
      throw Exception("Unsupported AFBC pixel format. pixelformat=0x%x.",
                      pixelformat);
  }

  if (tiled) {
    param |= (PARAM_TILED_BODY | PARAM_TILED_HEADER);
  }
}

IO::IO(uint32_t format, bool preload, size_t width, size_t height,
       size_t strideAlign)
    : format(format),
      width(width),
      height(height),
      strideAlign(strideAlign),
      isPreload(preload),
      timestamp(0) {}

Input::Input(uint32_t format, bool preload, size_t width, size_t height,
             size_t strideAlign)
    : IO(format, preload, width, height, strideAlign) {
  if (format == V4L2_PIX_FMT_VC1_ANNEX_G) {
    profile = 12;
  }
  dir = 0;
}

InputFile::InputFile(istream &input, uint32_t format, bool preload)
    : Input(format, preload), input(input) {
  state = 7;
  offset = 0;
  curlen = 0;
  iseof = false;
  naluFmt = 0;
  remaining_bytes = 0;
  inputPreBuf = NULL;
  inputBuf = NULL;
  reader = NULL;
  read_pos = 0;
  total_len = 0;
}

InputFile::InputFile(istream &input, uint32_t format, size_t width,
                     size_t height, size_t strideAlign, bool preload)
    : Input(format, preload, width, height, strideAlign), input(input) {
  state = 7;
  offset = 0;
  curlen = 0;
  iseof = false;
  naluFmt = 0;
  remaining_bytes = 0;
  inputPreBuf = NULL;
  inputBuf = NULL;
  reader = NULL;
  read_pos = 0;
  total_len = 0;
}

InputFile::~InputFile() {
  if (inputBuf) {
    free(inputBuf);
  }
  if (inputPreBuf) {
    free(inputPreBuf);
  }
  if (reader) {
    delete reader;
  }
}

void InputFile::prepare(Buffer &buf) {
  vector<iovec> iov = buf.getImageSize();
  if (getNaluFormat() == V4L2_OPT_NALU_FORMAT_ONE_NALU_PER_BUFFER) {
    int buflen = V4L2_ALLOCATE_BUFFER_ROI;
    int readlen = V4L2_READ_LEN_BUFFER_ROI;
    int iovoff = 0;
    {
      m_Mutex.lock();
      if (!inputBuf) {
        inputBuf = static_cast<char *>(malloc(buflen));  // ToDo
        input.read(static_cast<char *>(inputBuf), readlen);
        curlen = readlen;
      }
      m_Mutex.unlock();
    }
    if (offset == 0 && (0 == memcmp(inputBuf + offset, startCode, 4))) {
      offset += 4;
    }
    if (offset == curlen) {
      offset = 0;
      memset(static_cast<char *>(inputBuf), 0, curlen);
      input.read(static_cast<char *>(inputBuf), readlen);
      curlen = input.gcount();
    }
    int i = offset;
    for (;; i++) {
      i += startcode_find_candidate(inputBuf + i, curlen - i);
      if (i == curlen) {
        memcpy(static_cast<char *>(iov[0].iov_base) + iovoff, inputBuf + offset,
               curlen - offset);
        iovoff += curlen - offset;
        offset = 0;
        i = -1;
        if (input.peek() != EOF) {
          input.read(static_cast<char *>(inputBuf), readlen);
          curlen = input.gcount();
        } else {
          iseof = true;
          break;
        }
      } else if (i >= curlen - 4) {
        int remainlen = curlen - i;
        memcpy(static_cast<char *>(iov[0].iov_base) + iovoff, inputBuf + offset,
               i - offset);
        iovoff += i - offset;
        memcpy(inputBuf, inputBuf + i, remainlen);
        if (input.peek() != EOF) {
          input.read(static_cast<char *>(inputBuf) + remainlen, readlen);
          curlen = input.gcount() + remainlen;
          offset = 0;
          i = -1;
        } else {
          memcpy(static_cast<char *>(iov[0].iov_base) + iovoff, inputBuf,
                 remainlen);
          iovoff += remainlen;
          iseof = true;
          break;
        }
      } else if (0 == memcmp(inputBuf + i, startCode,
                             4)) {  // FIX ME: combine start code
        memcpy(static_cast<char *>(iov[0].iov_base) + iovoff, inputBuf + offset,
               i - offset);
        iov[0].iov_len = i - offset + iovoff;
        offset = i + 4;
        break;
      } else if (0 == memcmp(inputBuf + i, subStartCode, 3)) {
        memcpy(static_cast<char *>(iov[0].iov_base) + iovoff, inputBuf + offset,
               i - offset);
        iov[0].iov_len = i - offset + iovoff;
        offset = i + 3;
        break;
      }
    }
    buf.setEndOfSubFrame(true);
    buf.setBytesUsed(iov);
  } else if (getNaluFormat() == V4L2_OPT_NALU_FORMAT_ONE_FRAME_PER_BUFFER) {
    int buflen = V4L2_READ_LEN_BUFFER_ROI;
    bool found_frame = false;
    {
      m_Mutex.lock();
      if (!inputBuf) {
        inputBuf = static_cast<char *>(malloc(buflen));  // ToDo
        // input.read(static_cast<char *>(inputBuf), readlen);
        // curlen = readlen;
      }
      if (!reader) {
        reader = new start_code_reader(getFormat());
      }
      m_Mutex.unlock();
    }
    uint32_t frame_start_pos = 0;
    uint32_t frame_size = 0;
    uint32_t slice_cnt = 0;
    uint8_t *remaining_data = NULL;
    while (!found_frame && !iseof) {
      uint32_t size = buflen - remaining_bytes;
      uint32_t readlen = 0;
      if (input.peek() != EOF) {
        input.read(static_cast<char *>(inputBuf) + remaining_bytes, size);
        readlen = input.gcount();
        // cout<<"readen:"<<readlen<<endl;
        if (readlen < size) {
          reader->set_eos(input.peek() == EOF);
        }
      }
      size = remaining_bytes + readlen;
      reader->reset_bitstream_data((uint8_t *)inputBuf, size);
      result rtn =
          reader->find_one_frame(slice_cnt, frame_start_pos, frame_size);
      switch (rtn) {
        case reader::RR_OK:
        case reader::RR_EOP_CODEC_CONFIG:
        case reader::RR_EOP_FRAME:
          memcpy(static_cast<char *>(iov[0].iov_base),
                 inputBuf + frame_start_pos, frame_size);
          iov[0].iov_len = frame_size;
          found_frame = true;
          reader->get_remaining_bitstream_data(remaining_data, remaining_bytes);
          if (remaining_bytes) {
            memcpy(inputBuf, remaining_data, remaining_bytes);
          }
          break;
        case reader::RR_EOP:
          reader->get_remaining_bitstream_data(remaining_data, remaining_bytes);
          if (remaining_bytes) {
            memcpy(inputBuf, remaining_data, remaining_bytes);
          }
          break;
        case reader::RR_EOS:
          iseof = true;
          if (frame_size != 0) {
            memcpy(static_cast<char *>(iov[0].iov_base),
                   inputBuf + frame_start_pos, frame_size);
            iov[0].iov_len = frame_size;
            found_frame = true;
          }
          break;
        case reader::RR_ERROR:
        default:
          break;
      }
    }
    buf.setEndOfFrame(true);
    buf.setBytesUsed(iov);

  } else {
    for (size_t i = 0; i < iov.size(); ++i) {
      if (!getPreload()) {
        input.read(static_cast<char *>(iov[i].iov_base), iov[i].iov_len);
        if ((unsigned int)input.gcount() < iov[i].iov_len) {
          iseof = true;
        }
        iov[i].iov_len = input.gcount();
      } else {
        int read_len =
            readBuffer(static_cast<char *>(iov[i].iov_base), iov[i].iov_len);
        if (read_len < iov[i].iov_len) {
          // printf("read all data from input buffer.\n");
        }
        iov[i].iov_len = read_len;
      }
      // printf("read iov len: %lu, i: %zu, read_len: %td, getPreload: %d\n",
      // iov[i].iov_len, i, input.gcount(), getPreload());
    }

    buf.setBytesUsed(iov);
  }
}

int InputFile::readBuffer(char *dest, int len) {
  int read_len;
  int remain_len = total_len - read_pos;
  if (len <= remain_len) {
    memcpy(dest, inputPreBuf + read_pos, len);
    read_pos = read_pos + len;
    read_len = len;
    // printf("readBuffer. 1. read_pos: %d, total_len: %d, len: %d\n", read_pos,
    // total_len, len);
  } else {
    if (remain_len > 0) {
      memcpy(dest, inputPreBuf + read_pos, remain_len);
      read_pos = 0;
      read_len = remain_len;
    } else {
      read_pos = 0;
      read_len = readBuffer(dest, len);
    }
    // printf("readBuffer. 2. read_pos: %d, total_len: %d, len: %d, remain_len:
    // %d\n", read_pos, total_len, len, remain_len);
  }
  return read_len;
}

void InputFile::ignoreBuffer(int len) {
  read_pos = read_pos + len;
  if (read_pos >= total_len) read_pos = 0;
}

int startcode_find_candidate(char *buf, int size) {
  int i = 0;
  for (; i < size; i++) {
    if (!buf[i]) break;
  }
  return i;
}

bool InputFile::eof() {
  // return input.peek() == EOF;
  return iseof;
}

void InputFile::preloadBuffer(v4l2_buf_type type) {
  if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
    printf("it is InputFile for decoder.\n");
    int buflen = MAX_PRE_STORE_BUFFER_SIZE;
    int readlen = 0;

    if (!inputPreBuf) {
      inputPreBuf = static_cast<char *>(malloc(buflen));
      input.read(static_cast<char *>(inputPreBuf), buflen);
      readlen = input.gcount();
      if (readlen <= buflen) {
        printf("read all %d bytes for input file. buflen is %d\n", readlen,
               buflen);
      }
      total_len = readlen;
      read_pos = 0;
    }
  } else if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
    int width = getWidth();
    int height = getHeight();
    printf("it is InputFile for encoder. width: %d, height: %d\n", width,
           height);

    int frameSize = width * height * 1.5;
    int readlen = frameSize * 5;
    if (!inputPreBuf) {
      inputPreBuf = static_cast<char *>(malloc(readlen));  // ToDo
      input.read(static_cast<char *>(inputPreBuf), readlen);
      int curlen = input.gcount();
      if (curlen < readlen) {
        printf("there is no 10 frames for input file. curlen: %d, buflen: %d\n",
               curlen, readlen);
      }
      total_len = curlen;
      read_pos = 0;
    }
  }
}

InputIVF::InputIVF(istream &input, uint32_t informat, bool preload)
    : InputFile(input, 0, preload) {
  IVFHeader header;
  left_bytes = 0;
  timestamp = 0;

  input.read(reinterpret_cast<char *>(&header), sizeof(header));

  if (header.signature != IVFHeader::signatureDKIF) {
    char *c = reinterpret_cast<char *>(&header.signature);
    throw Exception("Incorrect DKIF signature. signature=%c%c%c%c.", c[0], c[1],
                    c[2], c[3]);
  }

  if (header.version != 0) {
    throw Exception("Incorrect DKIF version. version=%u.", header.version);
  }

  if (header.length != 32) {
    throw Exception("Incorrect DKIF length. length=%u.", header.length);
  }
  if (informat != header.codec) {
    format = informat;
  } else {
    format = header.codec;
  }
}

bool InputIVF::eof() {
  bool eof = false;
  if (!getPreload()) eof = (input.peek() == EOF);
  return eof;
}

void InputIVF::prepare(Buffer &buf) {
  uint32_t cal_size = 0;
  if (left_bytes == 0) {
    IVFFrame frame;
    if (!getPreload()) {
      input.read(reinterpret_cast<char *>(&frame), sizeof(frame));
    } else {
      readBuffer(reinterpret_cast<char *>(&frame), sizeof(frame));
    }
    cal_size = frame.size;
    timestamp = frame.timestamp;
  } else {
    cal_size = left_bytes;
  }

  vector<iovec> iov = buf.getImageSize();
  uint32_t read_bytes = cal_size > iov[0].iov_len ? iov[0].iov_len : cal_size;
  int read_len = 0;
  if (!getPreload()) {
    input.read(static_cast<char *>(iov[0].iov_base), read_bytes);
    read_len = input.gcount();
  } else {
    read_len = readBuffer(static_cast<char *>(iov[0].iov_base), read_bytes);
  }
  if (read_len < read_bytes) {
    cout << "read less than need!." << endl;
  }
  iov[0].iov_len = read_len;
  buf.setEndOfFrame(cal_size == iov[0].iov_len);
  left_bytes = cal_size - iov[0].iov_len;
  buf.setBytesUsed(iov);
  buf.setTimeStamp(timestamp);
  timestampList.insert(timestamp);
}

InputRCV::InputRCV(istream &input, bool preload)
    : InputFile(input, 0, preload) {
  input.read(reinterpret_cast<char *>(&sld), sizeof(sld));

  if (sld.signature1 != VC1SequenceLayerData::magic1 ||
      sld.signature2 != VC1SequenceLayerData::magic2) {
    cout << "This is a raw stream of VC1!" << endl;
    format = V4L2_PIX_FMT_VC1_ANNEX_G;
    profile = 12;
    codecConfigSent = true;
    isRcv = false;
    input.clear();
    input.seekg(0);
  } else {
    /* headerC is serialized in bigendian byteorder. */
    uint32_t q = sld.headerC;
    q = ntohl(q);

    HeaderC hc = reinterpret_cast<HeaderC &>(q);
    profile = hc.profile;

    format = V4L2_PIX_FMT_VC1_ANNEX_L;

    codecConfigSent = false;
    isRcv = true;
  }
  left_bytes = 0;
}

bool InputRCV::eof() { return input.peek() == EOF; }

void InputRCV::prepare(Buffer &buf) {
  vector<iovec> iov = buf.getImageSize();
  if (!isRcv) {
    InputFile::prepare(buf);
    return;
  }
  if (codecConfigSent != false) {
    uint32_t cal_size = 0;
    uint32_t read_bytes = 0;
    char *read_addr;
    uint32_t offset = 0;
    VC1FrameLayerData *fld = static_cast<VC1FrameLayerData *>(iov[0].iov_base);
    if (left_bytes == 0) {
      input.read(reinterpret_cast<char *>(fld), sizeof(*fld));
      cal_size = fld->frameSize;
      read_addr = reinterpret_cast<char *>(fld->data);
      offset = sizeof(*fld);
    } else {
      cal_size = left_bytes;
      read_addr = static_cast<char *>(iov[0].iov_base);
    }
    read_bytes =
        iov[0].iov_len - offset > cal_size ? cal_size : iov[0].iov_len - offset;
    input.read(read_addr, read_bytes);
    iov[0].iov_len = input.gcount() + offset;
    left_bytes = cal_size > input.gcount() ? cal_size - input.gcount() : 0;
    buf.setEndOfFrame(left_bytes == 0);
  } else {
    memcpy(iov[0].iov_base, &sld, sizeof(sld));
    iov[0].iov_len = sizeof(sld);

    buf.setCodecConfig(true);
    codecConfigSent = true;
  }

  buf.setBytesUsed(iov);
}

InputAFBC::InputAFBC(istream &input, uint32_t format, size_t width,
                     size_t height, bool preload)
    : InputFile(input, format, width, height, 1, preload) {
  AFBCHeader header;
  input.read(reinterpret_cast<char *>(&header), sizeof(header));
  frameSize = header.frameSize;

  input.clear();
  input.seekg(0);
}

void InputAFBC::prepare(Buffer &buf) {
  if (eof()) {
    return;
  }
  vector<iovec> iov = buf.getImageSize();
  AFBCHeader header;

  if (!getPreload()) {
    input.read(reinterpret_cast<char *>(&header), sizeof(header));
  } else {
    readBuffer(reinterpret_cast<char *>(&header), sizeof(header));
  }

  if (header.version >
      AFBCHeader::VERSION)  // if (header.version != AFBCHeader::VERSION)
  {
    throw Exception("Incorrect AFBC header version. got=%u, exptected <=%d.",
                    header.version, AFBCHeader::VERSION);
  }

  if (header.headerSize < sizeof(header)) {
    throw Exception("AFBC header size too small. size=%u.", header.headerSize);
  }

  if (6 == header.version && 64 != header.headerSize) {
    throw Exception(
        "Incorrect AFBC headerSize, got=%u, exptected 64 in header.version=%d.",
        header.headerSize, header.version);
  }

  if (header.frameSize > iov[0].iov_len) {
    throw Exception(
        "AFBC buffer too small. header_size=%u, frame_size=%u, "
        "buffer_size=%zu.",
        header.headerSize, header.frameSize, iov[0].iov_len);
  }

  size_t skip = header.headerSize - sizeof(header);
  if (skip > 0) {
    if (!getPreload()) {
      input.ignore(skip);
    } else {
      ignoreBuffer(skip);
    }
  }

  int read_len = 0;
  if (!getPreload()) {
    input.read(reinterpret_cast<char *>(iov[0].iov_base), header.frameSize);
    read_len = input.gcount();
  } else {
    read_len =
        readBuffer(reinterpret_cast<char *>(iov[0].iov_base), header.frameSize);
  }
  if (read_len != header.frameSize) {
    throw Exception("Too few AFBC bytes read. expected=%u, got=%d.",
                    header.frameSize, read_len);
  }

  iov[0].iov_len = header.frameSize;

  buf.setBytesUsed(iov);

  bool tiled = header.param & AFBCHeader::PARAM_TILED_BODY;
  buf.setTiled(tiled);

  bool superblock = 2 < header.subsampling;
  buf.setSuperblock(superblock);

  if (header.blockSplit) {
    buf.setBlockSplit(true);
  }
}

bool InputAFBC::eof() {
  bool eof = false;
  if (!getPreload()) eof = (input.peek() == EOF);
  return eof;
}

InputFileFrame::InputFileFrame(istream &input, uint32_t format, size_t width,
                               size_t height, size_t strideAlign, bool preload)
    : InputFile(input, format, width, height, strideAlign, preload),
      nplanes(0) {
  Codec::getSize(format, width, height, strideAlign, nplanes, stride, size);
}

void InputFileFrame::prepare(Buffer &buf) {
  vector<iovec> iov = buf.getImageSize();

  if (nplanes != iov.size()) {
    throw Exception(
        "Frame format and buffer have different number of planes. format=%zu, "
        "buffer=%zu.",
        nplanes, iov.size());
  }

  for (size_t i = 0; i < nplanes; ++i) {
    if (size[i] > iov[i].iov_len) {
      throw Exception(
          "Frame plane is larger than buffer plane. plane=%zu, plane_size=%zu, "
          "buffer_size=%zu.",
          i, size[i], iov[0].iov_len);
    }

    if (!getPreload()) {
      input.read(static_cast<char *>(iov[i].iov_base), size[i]);
      if ((unsigned int)input.gcount() < iov[i].iov_len) {
        iseof = true;
      }
      iov[i].iov_len = input.gcount();
    } else {
      int read_len = readBuffer(static_cast<char *>(iov[i].iov_base), size[i]);
      if (read_len < iov[i].iov_len) {
        // printf("read all data from input buffer.\n");
      }
      iov[i].iov_len = read_len;
    }
  }

  buf.setBytesUsed(iov);
}

InputFileFrameWithROI::InputFileFrameWithROI(std ::istream &input,
                                             uint32_t format, size_t width,
                                             size_t height, size_t strideAlign,
                                             std ::istream &roi)
    : InputFileFrame(input, format, width, height, strideAlign), roi_is(roi) {
  roi_list = NULL;
  load_roi_cfg();
  prepared_frames = 0;
}

InputFileFrameWithROI::~InputFileFrameWithROI() {
  if (roi_list) {
    delete roi_list;
  }
}

void InputFileFrameWithROI::prepare(Buffer &buf) {
  v4l2_roi_list_t::iterator iter = roi_list->begin();
  v4l2_roi_list_t::iterator end = roi_list->end();
  for (; iter != end; iter++) {
    if (iter->pic_index == prepared_frames) {
      break;
    }
  }
  if (iter != end) {
    buf.setRoiCfg(*iter);
  } else {
    printf("no roiCfg value for pic_index %d.\n", prepared_frames);
  }
  InputFileFrame::prepare(buf);
  buf.setROIflag();
  prepared_frames++;
}

void InputFileFrameWithROI::load_roi_cfg() {
  int pic_index, qp, num_roi;
  struct v4l2_mvx_roi_regions roi;
  if (!roi_list) {
    roi_list = new v4l2_roi_list_t;
  }
  while (roi_is.getline(cfg_file_line_buf, CFG_FILE_LINE_SIZE)) {
    memset(&roi, 0, sizeof(struct v4l2_mvx_roi_regions));
    if (3 == sscanf(cfg_file_line_buf, "pic=%d qp=%d num_roi=%d", &pic_index,
                    &qp, &num_roi)) {
      roi.pic_index = pic_index;
      roi.qp = qp;
      roi.num_roi = num_roi;
      roi.roi_present = true;
      roi.qp_present = true;
    } else if (3 == sscanf(cfg_file_line_buf, "pic=%d num_roi=%d qp=%d",
                           &pic_index, &num_roi, &qp)) {
      roi.pic_index = pic_index;
      roi.qp = qp;
      roi.num_roi = num_roi;
      roi.roi_present = true;
      roi.qp_present = true;
    } else if (2 == sscanf(cfg_file_line_buf, "pic=%d num_roi=%d qp=%d",
                           &pic_index, &num_roi, &qp)) {
      roi.pic_index = pic_index;
      roi.qp = 0;
      roi.num_roi = num_roi;
      roi.roi_present = true;
      roi.qp_present = false;
    } else if (2 == sscanf(cfg_file_line_buf, "pic=%d qp=%d num_roi=%d",
                           &pic_index, &qp, &num_roi)) {
      roi.pic_index = pic_index;
      roi.qp = qp;
      roi.num_roi = 0;
      roi.roi_present = false;
      roi.qp_present = true;
    } else {
      cout << "parse ROI config file ERROR!" << endl;
    }
    if (roi.num_roi > V4L2_MVX_MAX_FRAME_REGIONS) {
      cout << "invalid n_regions value:" << roi.num_roi << endl;
    }
    if (roi.num_roi > 0) {
      char *sub_buf1 = cfg_file_line_buf;
      char *sub_buf2;
      int match = 0;
      for (int i = 0; i < roi.num_roi; i++) {
        sub_buf2 = strstr(sub_buf1, " roi=");
        if (sub_buf2 != NULL) {
          match = sscanf(sub_buf2, " roi={%hu,%hu,%hu,%hu,%hu}",
                         &roi.roi[i].mbx_left, &roi.roi[i].mbx_right,
                         &roi.roi[i].mby_top, &roi.roi[i].mby_bottom,
                         &roi.roi[i].qp_delta);
        }
        if (match != 5) {
          cout << "Error while parsing the ROI regions:" << match << endl;
        }
        cout << roi.roi[i].mbx_left << "," << roi.roi[i].mbx_right << ","
             << roi.roi[i].mby_top << "," << roi.roi[i].mby_bottom << ","
             << roi.roi[i].qp_delta << endl;
        sub_buf1 = sub_buf2 + 5;
      }
    }
    roi_list->push_back(roi);
  }
}

InputFileFrameWithEPR::InputFileFrameWithEPR(std ::istream &input,
                                             uint32_t format, size_t width,
                                             size_t height, size_t strideAlign,
                                             std ::istream &epr,
                                             uint32_t oformat)
    : InputFileFrame(input, format, width, height, strideAlign),
      epr_is(epr),
      outformat(oformat) {
  prepared_frames = 0;
  epr_list = NULL;
  load_epr_cfg();
  cur = epr_list->begin();
}

InputFileFrameWithEPR::~InputFileFrameWithEPR() {
  if (epr_list) {
    delete epr_list;
  }
}

void InputFileFrameWithEPR::erp_adjust_bpr_to_64_64(
    struct v4l2_buffer_general_rows_uncomp_body *uncomp_body, int qp_delta,
    uint32_t bpr_base_idx, uint32_t row_off, uint8_t force) {
  uncomp_body->bpr[bpr_base_idx].qp_delta =
      (uint16_t)(SET(V4L2_BLOCK_PARAM_RECORD_QP_DELTA_TOP_LEFT_16X16,
                     qp_delta) |
                 SET(V4L2_BLOCK_PARAM_RECORD_QP_DELTA_TOP_RIGHT_16X16,
                     qp_delta) |
                 SET(V4L2_BLOCK_PARAM_RECORD_QP_DELTA_BOT_LEFT_16X16,
                     qp_delta) |
                 SET(V4L2_BLOCK_PARAM_RECORD_QP_DELTA_BOT_RIGHT_16X16,
                     qp_delta));

  uncomp_body->bpr[bpr_base_idx].force = force;
  uncomp_body->bpr[bpr_base_idx + 1] = uncomp_body->bpr[bpr_base_idx];
  uncomp_body->bpr[bpr_base_idx + 1].force = force;
  uncomp_body->bpr[bpr_base_idx + row_off] = uncomp_body->bpr[bpr_base_idx];
  uncomp_body->bpr[bpr_base_idx + row_off].force = force;
  uncomp_body->bpr[bpr_base_idx + row_off + 1] = uncomp_body->bpr[bpr_base_idx];
  uncomp_body->bpr[bpr_base_idx + row_off + 1].force = force;
}

void InputFileFrameWithEPR::prepare(Buffer &buf) {
  /*if (prepared_frames % 2 == 0) {
      prepareEPR(buf);
  } else {
      InputFileFrame::prepare(buf);
  }
  prepared_frames++;*/
  if (epr_list->end() != cur && cur->pic_index == prepared_frames) {
    prepareEPR(buf);
    cur++;
  } else {
    InputFileFrame::prepare(buf);
    prepared_frames++;
  }
}

void InputFileFrameWithEPR::prepareEPR(Buffer &buf) {
  vector<iovec> iov = buf.getImageSize();
  v4l2_epr_list_t::iterator iter = cur;
  v4l2_epr_list_t::iterator end = epr_list->end();
  /*for (; iter != end; iter++) {
      if (iter->pic_index == prepared_frames / 2) {
          break;
      }
  }*/
  if (cur != end) {
    if (iter->qp_present) {
      cout << "qp:" << iter->qp.qp << endl;
      buf.setQPofEPR(iter->qp.qp);
    }
    if (iter->block_configs_present) {
      struct v4l2_buffer_general_block_configs block_configs;
      struct v4l2_core_buffer_header_general buffer_configs;
      unsigned int blk_cfg_size = 0;
      if (iter->block_configs.blk_cfg_type ==
          V4L2_BLOCK_CONFIGS_TYPE_ROW_UNCOMP) {
        blk_cfg_size += sizeof(block_configs.blk_cfgs.rows_uncomp);

        int max_cols = (getWidth() + 31) >> 5;
        int max_rows = (getHeight() + 31) >> 5;
        block_configs.blk_cfgs.rows_uncomp.n_cols_minus1 = max_cols - 1;
        block_configs.blk_cfgs.rows_uncomp.n_rows_minus1 = max_rows - 1;

        size_t bpru_body_size;
        if (outformat == V4L2_PIX_FMT_HEVC) {
          max_rows = (getHeight() + 127) >> 5;
          bpru_body_size = max_rows * max_cols *
                           sizeof(v4l2_buffer_general_rows_uncomp_body);
        } else {
          bpru_body_size = max_rows * max_cols *
                           sizeof(v4l2_buffer_general_rows_uncomp_body);
        }
        struct v4l2_buffer_general_rows_uncomp_body *uncomp_body =
            static_cast<struct v4l2_buffer_general_rows_uncomp_body *>(
                iov[0].iov_base);
        iov[0].iov_len = bpru_body_size;
        cout << "plane 0 size:" << bpru_body_size << endl;
        int n_cols = iter->block_configs.blk_cfgs.rows_uncomp.n_cols_minus1 + 1;
        int n_rows = iter->block_configs.blk_cfgs.rows_uncomp.n_rows_minus1 + 1;

        if (outformat != V4L2_PIX_FMT_HEVC) {
          for (int row = 0; row < n_rows; row++) {
            memcpy(&uncomp_body->bpr[row * max_cols],
                   &iter->bc_row_body.uncomp->bpr[row * n_cols],
                   n_cols * sizeof(uncomp_body->bpr[0]));
            for (int col = n_cols; col < max_cols; col++) {
              memcpy(&uncomp_body->bpr[(row * max_cols) + col],
                     &uncomp_body->bpr[(row * max_cols) + n_cols - 1],
                     sizeof(uncomp_body->bpr[0]));
            }
          }
          for (int row = n_rows; row < max_rows; row++) {
            memcpy(&uncomp_body->bpr[row * max_cols],
                   &uncomp_body->bpr[(n_rows - 1) * max_cols],
                   max_cols * sizeof(uncomp_body->bpr[0]));
          }
        } else {
          for (int row = 0; row < n_rows; row++) {
            for (int k = 0; k < n_cols; k++) {
              unsigned int quad_qp_delta =
                  iter->bc_row_body.uncomp->bpr[row * n_cols + k].qp_delta;
              int top_left =
                  (int)SGET(V4L2_BLOCK_PARAM_RECORD_QP_DELTA_TOP_LEFT_16X16,
                            quad_qp_delta);
              int top_right =
                  (int)SGET(V4L2_BLOCK_PARAM_RECORD_QP_DELTA_TOP_RIGHT_16X16,
                            quad_qp_delta);
              int bot_left =
                  (int)SGET(V4L2_BLOCK_PARAM_RECORD_QP_DELTA_BOT_LEFT_16X16,
                            quad_qp_delta);
              int bot_right =
                  (int)SGET(V4L2_BLOCK_PARAM_RECORD_QP_DELTA_BOT_RIGHT_16X16,
                            quad_qp_delta);
              uint8_t force =
                  iter->bc_row_body.uncomp->bpr[row * n_cols + k].force;
              // top left 64*64
              erp_adjust_bpr_to_64_64(uncomp_body, top_left,
                                      4 * row * max_cols + 4 * k, max_cols,
                                      force);
              // top right 64*64
              erp_adjust_bpr_to_64_64(uncomp_body, top_right,
                                      4 * row * max_cols + 4 * k + 2, max_cols,
                                      force);
              // bottom left 64*64
              erp_adjust_bpr_to_64_64(uncomp_body, bot_left,
                                      4 * row * max_cols + 2 * max_cols + 4 * k,
                                      max_cols, force);
              // bottom right 64*64
              erp_adjust_bpr_to_64_64(
                  uncomp_body, bot_right,
                  4 * row * max_cols + 2 * max_cols + 4 * k + 2, max_cols,
                  force);
            }
            // extend the end of a row
            for (int col = 4 * n_cols; col < max_cols; col++) {
              memcpy(&uncomp_body->bpr[(4 * row * max_cols) + col],
                     &uncomp_body->bpr[(4 * row * max_cols) + 4 * n_cols - 1],
                     sizeof(uncomp_body->bpr[0]));
              memcpy(
                  &uncomp_body->bpr[(4 * row * max_cols + max_cols) + col],
                  &uncomp_body
                       ->bpr[(4 * row * max_cols + max_cols) + 4 * n_cols - 1],
                  sizeof(uncomp_body->bpr[0]));
              memcpy(
                  &uncomp_body->bpr[(4 * row * max_cols + 2 * max_cols) + col],
                  &uncomp_body->bpr[(4 * row * max_cols + 2 * max_cols) +
                                    4 * n_cols - 1],
                  sizeof(uncomp_body->bpr[0]));
              memcpy(
                  &uncomp_body->bpr[(4 * row * max_cols + 3 * max_cols) + col],
                  &uncomp_body->bpr[(4 * row * max_cols + 3 * max_cols) +
                                    4 * n_cols - 1],
                  sizeof(uncomp_body->bpr[0]));
            }
          }
          for (int row = 4 * n_rows; row < max_rows; row++) {
            // extend the last row
            memcpy(&uncomp_body->bpr[row * max_cols],
                   &uncomp_body->bpr[(row - 4) * max_cols],
                   max_cols * sizeof(uncomp_body->bpr[0]));
          }
        }

        block_configs.blk_cfg_type = iter->block_configs.blk_cfg_type;
        blk_cfg_size += sizeof(block_configs.blk_cfg_type);
        blk_cfg_size += sizeof(block_configs.reserved);

        buffer_configs.type = V4L2_BUFFER_GENERAL_TYPE_BLOCK_CONFIGS;
        buffer_configs.buffer_size = bpru_body_size;
        buffer_configs.config_size = blk_cfg_size;
        memcpy(&buffer_configs.config, &block_configs, blk_cfg_size);
        memcpy(&buf.getBuffer().m.planes[0].reserved[0], &buffer_configs,
               sizeof(buffer_configs.buffer_size) +
                   sizeof(buffer_configs.type) +
                   sizeof(buffer_configs.config_size) + blk_cfg_size);
      }
    }
  }
  for (uint32_t i = 1; i < iov.size(); i++) {
    iov[i].iov_len = 0;
  }
  buf.setBytesUsed(iov);
  buf.setEPRflag();
}

void InputFileFrameWithEPR::read_row_cfg(char *buf, int row, int len,
                                         epr_config &config) {
  char *sub_buf1, *sub_buf2;
  int quad_qp_delta[4];
  struct v4l2_block_param_record *bpr = NULL;
  int n_cols = config.block_configs.blk_cfgs.rows_uncomp.n_cols_minus1 + 1;
  if (row == 0) {
    config.block_configs.blk_cfgs.rows_uncomp.n_cols_minus1 = len - 1;
  }
  bpr = &config.bc_row_body.uncomp->bpr[row * n_cols];
  sub_buf1 = buf;

  for (int i = 0; i < len; i++) {
    sub_buf2 = strstr(sub_buf1, " bpr=");
    // assert(sub_buf2);
    if (!sub_buf2) {
      printf(
          "Error : the real bpr number is less than num_bpr for picture =%d "
          "(%s))\n",
          config.pic_index, cfg_file_line_buf);
      return;
    }
    if (5 != sscanf(sub_buf2, " bpr={%d,%d,%d,%d,%hhi}", &quad_qp_delta[0],
                    &quad_qp_delta[1], &quad_qp_delta[2], &quad_qp_delta[3],
                    &bpr[i].force)) {
      printf("Error while parsing the row BPR[%d]\n", i);
    }
    const int qpd_min =
        -(1 << (V4L2_BLOCK_PARAM_RECORD_QP_DELTA_TOP_LEFT_16X16_SZ - 1));
    const int qpd_max =
        (1 << (V4L2_BLOCK_PARAM_RECORD_QP_DELTA_TOP_LEFT_16X16_SZ - 1)) - 1;
    if (quad_qp_delta[0] < qpd_min || quad_qp_delta[0] > qpd_max ||
        quad_qp_delta[1] < qpd_min || quad_qp_delta[1] > qpd_max ||
        quad_qp_delta[2] < qpd_min || quad_qp_delta[2] > qpd_max ||
        quad_qp_delta[3] < qpd_min || quad_qp_delta[3] > qpd_max) {
      printf(
          "Error qp_deltas (%d,%d,%d,%d) are out of range (min=%d, max=%d)\n",
          quad_qp_delta[0], quad_qp_delta[1], quad_qp_delta[2],
          quad_qp_delta[3], qpd_min, qpd_max);
    }
    bpr[i].qp_delta =
        SET(V4L2_BLOCK_PARAM_RECORD_QP_DELTA_TOP_LEFT_16X16, quad_qp_delta[0]);
    bpr[i].qp_delta |=
        SET(V4L2_BLOCK_PARAM_RECORD_QP_DELTA_TOP_RIGHT_16X16, quad_qp_delta[1]);
    bpr[i].qp_delta |=
        SET(V4L2_BLOCK_PARAM_RECORD_QP_DELTA_BOT_LEFT_16X16, quad_qp_delta[2]);
    bpr[i].qp_delta |=
        SET(V4L2_BLOCK_PARAM_RECORD_QP_DELTA_BOT_RIGHT_16X16, quad_qp_delta[3]);

    sub_buf1 = sub_buf2 + 5;
  }
  sub_buf2 = strstr(sub_buf1, " bpr=");
  if (sub_buf2) {
    printf(
        "Error : the real bpr number is larger than num_bpr for picture =%d "
        "(%s)\n",
        config.pic_index, cfg_file_line_buf);
  }

  for (int i = len; i < n_cols; i++) {
    bpr[i] = bpr[len - 1];
  }
}

void InputFileFrameWithEPR::read_efp_cfg(char *buf, int num_epr,
                                         epr_config *config) {
  char *sub_buf1;
  int epr_count = 0;
  // qp
  sub_buf1 = strstr(buf, " qp=");
  if (sub_buf1) {
    if (1 != sscanf(sub_buf1, " qp={%u}", &config->qp.qp)) {
      cout << "Error while parsing qp:" << sub_buf1 << endl;
    }
    config->qp_present = true;
    epr_count++;
  }
  if (epr_count != num_epr) {
    cout << "Error not parsed enough EPRs" << endl;
  }
}
void InputFileFrameWithEPR::load_epr_cfg() {
  unsigned int num;
  unsigned int num2;
  int pic_num;
  int last_pic_num = -1;  // -1 to handle first time
  int epr_num_row = 0;
  int epr_real_row = 0;
  if (!epr_list) {
    epr_list = new v4l2_epr_list_t;
  }
  const size_t max_bprf_body_size = ((getWidth() + 31) >> 5) *
                                    ((getHeight() + 31) >> 5) *
                                    sizeof(struct v4l2_block_param_record);
  epr_config config(max_bprf_body_size);
  while (epr_is.getline(cfg_file_line_buf, CFG_FILE_LINE_SIZE)) {
    if (1 != sscanf(cfg_file_line_buf, "pic=%d", &pic_num)) {
      cout << "Line:" << cfg_file_line_buf << endl;
      return;
    }
    if (pic_num < 0) {
      cout << "pic index must not be less than zero!" << endl;
      return;
    }
    if (last_pic_num != pic_num) {
      // not the first time
      if (last_pic_num != -1) {
        epr_list->push_back(config);
      }
      config.clear();
      epr_num_row = 0;
      epr_real_row = 0;
    }
    if (2 == sscanf(cfg_file_line_buf, "pic=%d num_efp=%d", &config.pic_index,
                    &num)) {
      read_efp_cfg(cfg_file_line_buf, num, &config);
    } else if (3 == sscanf(cfg_file_line_buf, "pic=%d num_row=%d type=%i",
                           &config.pic_index, &num, &num2)) {
      if (num == 0) {
        cout << "num_row must be greater than 0" << endl;
        return;
      }
      if (config.block_configs_present) {
        cout << " block_configs_present flag already set to region for picture!"
             << endl;
      }
      config.block_configs_present = true;

      if (num2 == V4L2_BLOCK_CONFIGS_TYPE_ROW_UNCOMP) {
        config.block_configs.blk_cfg_type = V4L2_BLOCK_CONFIGS_TYPE_ROW_UNCOMP;
        config.block_configs.blk_cfgs.rows_uncomp.n_rows_minus1 = num - 1;
      } else {
        cout << "Error - Unsupported block_param_rows_format" << endl;
        return;
      }
      epr_num_row = num;
    } else if (3 == sscanf(cfg_file_line_buf, "pic=%d row=%u num_bpr=%u",
                           &config.pic_index, &num, &num2)) {
      if (!config.block_configs_present) {
        cout << "Error - block_configs_present is not set for picture" << endl;
      }
      if (config.block_configs.blk_cfg_type ==
          V4L2_BLOCK_CONFIGS_TYPE_ROW_UNCOMP) {
        static unsigned int last_row = 0;
        if (num <= last_row && num != 0) {
          cout << "Error : current row number is less than last_row for picture"
               << endl;
        }
        while (num > (last_row + 1)) {
          // duplicate row
          int n_cols =
              config.block_configs.blk_cfgs.rows_uncomp.n_cols_minus1 + 1;
          memcpy(&config.bc_row_body.uncomp->bpr[(last_row + 1) * n_cols],
                 &config.bc_row_body.uncomp->bpr[last_row * n_cols],
                 n_cols * sizeof(struct v4l2_block_param_record));
          config.block_configs.blk_cfgs.rows_uncomp.n_rows_minus1++;
          last_row++;
        }
        epr_real_row++;
        read_row_cfg(cfg_file_line_buf, num, num2, config);
        last_row = num;
      } else {
        cout << "Error - block_configs_type is set to an unsupported value"
             << endl;
      }
    } else {
      cout << "parse config file ERROR:" << cfg_file_line_buf << endl;
    }
    last_pic_num = pic_num;
  }

  if (epr_real_row != epr_num_row) {
    cout << "Error: num_row [%d] in epr-cfg-file is not equal to the "
            "real-line-number"
         << endl;
  }
  epr_list->push_back(config);
}

InputFrame::InputFrame(uint32_t format, size_t width, size_t height,
                       size_t strideAlign, size_t nframes)
    : Input(format, width, height, strideAlign), nframes(nframes), count(0) {
  Codec::getSize(format, width, height, strideAlign, nplanes, stride, size);
}

void InputFrame::prepare(Buffer &buf) {
  vector<iovec> iov = buf.getImageSize();

  unsigned int color = (0xff << 24) | (count * 10);
  unsigned int rgba[4];
  for (int i = 0; i < 3; ++i) {
    rgba[i] = (color >> (i * 8)) & 0xff;
  }

  unsigned int yuv[3];
  rgb2yuv(yuv, rgba);

  unsigned int y = yuv[0];
  unsigned int u = yuv[1];
  unsigned int v = yuv[2];

  switch (format) {
    case V4L2_PIX_FMT_YUV420M: {
      if (iov.size() != 3) {
        throw Exception("YUV420 has 3 planes. planes=%zu.", iov.size());
      }

      /* Y plane. */
      if (iov[0].iov_len < size[0]) {
        throw Exception(
            "YUV420 Y plane has incorrect size. size=%zu, expected=%zu.",
            iov[0].iov_len, size[0]);
      }

      memset(iov[0].iov_base, y, size[0]);
      iov[0].iov_len = size[0];

      /* U plane. */
      if (iov[1].iov_len < size[1]) {
        throw Exception(
            "YUV420 U plane has incorrect size. size=%zu, expected=%zu.",
            iov[1].iov_len, size[1]);
      }

      memset(iov[1].iov_base, u, size[1]);
      iov[1].iov_len = size[1];

      /* V plane. */
      if (iov[2].iov_len < size[2]) {
        throw Exception(
            "YUV420 V plane has incorrect size. size=%zu, expected=%zu.",
            iov[2].iov_len, size[2]);
      }

      memset(iov[2].iov_base, v, size[2]);
      iov[2].iov_len = size[2];

      break;
    }
    case V4L2_PIX_FMT_NV12: {
      if (iov.size() != 2) {
        throw Exception("YUV420 NV12 has 2 planes. planes=%zu.", iov.size());
      }

      /* Y plane. */
      if (iov[0].iov_len < size[0]) {
        throw Exception(
            "YUV420 NV12 Y plane has incorrect size. size=%zu, expected=%zu.",
            iov[0].iov_len, size[0]);
      }

      memset(iov[0].iov_base, y, size[0]);
      iov[0].iov_len = size[0];

      /* UV plane. */
      if (iov[1].iov_len < size[1]) {
        throw Exception(
            "YUV420 NV12 UV plane has incorrect size. size=%zu, expected=%zu.",
            iov[1].iov_len, size[1]);
      }

      uint8_t *p = static_cast<uint8_t *>(iov[1].iov_base);
      for (size_t i = 0; i < iov[1].iov_len; i += 2) {
        *p++ = u;
        *p++ = v;
      }

      break;
    }
    case V4L2_PIX_FMT_NV21: {
      if (iov.size() != 2) {
        throw Exception("YUV420 NV12 has 2 planes. planes=%zu.", iov.size());
      }

      /* Y plane. */
      if (iov[0].iov_len < size[0]) {
        throw Exception(
            "YUV420 NV12 Y plane has incorrect size. size=%zu, expected=%zu.",
            iov[0].iov_len, size[0]);
      }

      memset(iov[0].iov_base, y, size[0]);
      iov[0].iov_len = size[0];

      /* UV plane. */
      if (iov[1].iov_len < size[1]) {
        throw Exception(
            "YUV420 NV12 UV plane has incorrect size. size=%zu, expected=%zu.",
            iov[1].iov_len, size[1]);
      }

      uint8_t *p = static_cast<uint8_t *>(iov[1].iov_base);
      for (size_t i = 0; i < iov[1].iov_len; i += 2) {
        *p++ = v;
        *p++ = u;
      }

      break;
    }
    case V4L2_PIX_FMT_YUYV: {
      if (iov.size() != 1) {
        throw Exception("YUYV has 1 plane. planes=%zu.", iov.size());
      }

      if (iov[0].iov_len < size[0]) {
        throw Exception(
            "YUYV plane 1 has incorrect size. size=%zu, expected=%zu.",
            iov[0].iov_len, size[0]);
      }

      uint8_t *p = static_cast<uint8_t *>(iov[0].iov_base);
      for (size_t i = 0; i < size[0]; i += 4) {
        *p++ = y;
        *p++ = u;
        *p++ = y;
        *p++ = v;
      }

      iov[0].iov_len = size[0];

      break;
    }
    case V4L2_PIX_FMT_UYVY: {
      if (iov.size() != 1) {
        throw Exception("UYVY has 1 plane. planes=%zu.", iov.size());
      }

      if (iov[0].iov_len < size[0]) {
        throw Exception(
            "UYVY plane 1 has incorrect size. size=%zu, expected=%zu.",
            iov[0].iov_len, size[0]);
      }

      uint8_t *p = static_cast<uint8_t *>(iov[0].iov_base);
      for (size_t i = 0; i < size[0]; i += 4) {
        *p++ = u;
        *p++ = y;
        *p++ = v;
        *p++ = y;
      }

      iov[0].iov_len = size[0];

      break;
    }
    case V4L2_PIX_FMT_Y210: {
      uint16_t y16 = y << 8;
      uint16_t u16 = u << 8;
      uint16_t v16 = v << 8;

      if (iov.size() != 1) {
        throw Exception("Y210 has 1 plane. planes=%zu.", iov.size());
      }

      if (iov[0].iov_len < size[0]) {
        throw Exception(
            "Y210 plane 1 has incorrect size. size=%zu, expected=%zu.",
            iov[0].iov_len, size[0]);
      }

      uint16_t *p = static_cast<uint16_t *>(iov[0].iov_base);
      for (size_t i = 0; i < (size[0] / sizeof(*p)); i += 4) {
        *p++ = y16;
        *p++ = u16;
        *p++ = y16;
        *p++ = v16;
      }

      iov[0].iov_len = size[0];

      break;
    }
    case V4L2_PIX_FMT_P010: {
      uint16_t y16 = y << 8;
      uint16_t u16 = u << 8;
      uint16_t v16 = v << 8;

      if (iov.size() != 2) {
        throw Exception("P010 has 2 planes. planes=%zu.", iov.size());
      }

      /* Y plane. */
      if (iov[0].iov_len < size[0]) {
        throw Exception(
            "P010 Y plane has incorrect size. size=%zu, expected=%zu.",
            iov[0].iov_len, size[0]);
      }

      uint16_t *p = static_cast<uint16_t *>(iov[0].iov_base);
      for (size_t i = 0; i < (size[0] / sizeof(*p)); ++i) {
        *p++ = y16;
      }

      iov[0].iov_len = size[0];

      /* UV plane. */
      if (iov[1].iov_len < size[1]) {
        throw Exception(
            "P010 UV plane has incorrect size. size=%zu, expected=%zu.",
            iov[1].iov_len, size[1]);
      }

      p = static_cast<uint16_t *>(iov[1].iov_base);
      for (size_t i = 0; i < (size[1] / sizeof(*p)); i += 2) {
        *p++ = u16;
        *p++ = v16;
      }

      iov[1].iov_len = size[1];

      break;
    }
    case V4L2_PIX_FMT_Y0L2: {
      if (iov.size() != 1) {
        throw Exception("Y0L2 has 1 plane. planes=%zu.", iov.size());
      }

      if (iov[0].iov_len < size[0]) {
        throw Exception(
            "Y0L2 plane 1 has incorrect size. size=%zu, expected=%zu.",
            iov[0].iov_len, size[0]);
      }

      uint64_t a = 3;
      uint64_t y10 = y << 2;
      uint64_t u10 = u << 2;
      uint64_t v10 = v << 2;
      uint64_t w = (a << 62) | (y10 << 52) | (v10 << 42) | (y10 << 32) |
                   (a << 30) | (y10 << 20) | (u10 << 10) | y10;
      uint8_t *p = static_cast<uint8_t *>(iov[0].iov_base);
      for (size_t i = 0; i < iov[0].iov_len; i += 8) {
        *p++ = w & 0xff;
        *p++ = (w >> 8) & 0xff;
        *p++ = (w >> 16) & 0xff;
        *p++ = (w >> 24) & 0xff;
        *p++ = (w >> 32) & 0xff;
        *p++ = (w >> 40) & 0xff;
        *p++ = (w >> 48) & 0xff;
        *p++ = (w >> 56) & 0xff;
      }

      iov[0].iov_len = size[0];

      break;
    }
    case DRM_FORMAT_ABGR8888: {
      if (iov.size() != 1) {
        throw Exception("RGBA has 1 plane. planes=%zu.", iov.size());
      }

      if (iov[0].iov_len < size[0]) {
        throw Exception(
            "RGBA plane 1 has incorrect size. size=%zu, expected=%zu.",
            iov[0].iov_len, size[0]);
      }

      uint8_t *p = static_cast<uint8_t *>(iov[0].iov_base);
      for (size_t i = 0; i < size[0]; i += 4) {
        *p++ = rgba[0];
        *p++ = rgba[1];
        *p++ = rgba[2];
        *p++ = rgba[3];
      }

      break;
    }
    case DRM_FORMAT_ARGB8888: {
      if (iov.size() != 1) {
        throw Exception("BGRA has 1 plane. planes=%zu.", iov.size());
      }

      if (iov[0].iov_len < size[0]) {
        throw Exception(
            "BGRA plane 1 has incorrect size. size=%zu, expected=%zu.",
            iov[0].iov_len, size[0]);
      }

      uint8_t *p = static_cast<uint8_t *>(iov[0].iov_base);
      for (size_t i = 0; i < size[0]; i += 4) {
        *p++ = rgba[2];
        *p++ = rgba[1];
        *p++ = rgba[0];
        *p++ = rgba[3];
      }

      break;
    }
    case DRM_FORMAT_BGRA8888: {
      if (iov.size() != 1) {
        throw Exception("ARGB has 1 plane. planes=%zu.", iov.size());
      }

      if (iov[0].iov_len < size[0]) {
        throw Exception(
            "ABGR plane 1 has incorrect size. size=%zu, expected=%zu.",
            iov[0].iov_len, size[0]);
      }

      uint8_t *p = static_cast<uint8_t *>(iov[0].iov_base);
      for (size_t i = 0; i < size[0]; i += 4) {
        *p++ = rgba[3];
        *p++ = rgba[0];
        *p++ = rgba[1];
        *p++ = rgba[2];
      }

      break;
    }
    case DRM_FORMAT_RGBA8888: {
      if (iov.size() != 1) {
        throw Exception("ABGR has 1 plane. planes=%zu.", iov.size());
      }

      if (iov[0].iov_len < size[0]) {
        throw Exception(
            "ABGR plane 1 has incorrect size. size=%zu, expected=%zu.",
            iov[0].iov_len, size[0]);
      }

      uint8_t *p = static_cast<uint8_t *>(iov[0].iov_base);
      for (size_t i = 0; i < size[0]; i += 4) {
        *p++ = rgba[3];
        *p++ = rgba[2];
        *p++ = rgba[1];
        *p++ = rgba[0];
      }

      break;
    }
    default:
      throw Exception("Unsupport input frame format.");
  }

  ++count;
  buf.setBytesUsed(iov);
  buf.setTimeStamp(count);
}

bool InputFrame::eof() { return count >= nframes; }

void InputFrame::rgb2yuv(unsigned int yuv[3], const unsigned int rgb[3]) {
  /* Y = Kr*R + Kg*G + Kb*B */
  /* U = (B-Y)/(1-Kb) = - R * Kr/(1-Kb) - G * Kg/(1-Kb) + B */
  /* V = (R-Y)/(1-Kr) = R - G * Kg/(1-Kr) - B * Kb/(1-Kr) */

  /* BT601 { 0.2990, 0.5870, 0.1140 } */
  /* BT709 { 0.2125, 0.7154, 0.0721 }; */

  float Kr = 0.299;
  float Kg = 0.587;
  float Kb = 0.114;

  float r = rgb[0] / 255.0;
  float g = rgb[1] / 255.0;
  float b = rgb[2] / 255.0;

  float y;
  float u;
  float v;

  /* RGB to YUV. */
  y = Kr * r + Kg * g + Kb * b;
  u = (b - y) / (1 - Kb);
  v = (r - y) / (1 - Kr);

  /* Map YUV to limited color space. */
  yuv[0] = y * 219 + 16;
  yuv[1] = u * 112 + 128;
  yuv[2] = v * 112 + 128;
}

Output::Output(uint32_t format) : IO(format), totalSize(0) { dir = 1; }

Output::~Output() { cout << "Total size " << totalSize << endl; }

void Output::prepare(Buffer &buf) { buf.clearBytesUsed(); }

void Output::finalize(Buffer &buf) {
  v4l2_buffer &b = buf.getBuffer();
  if (V4L2_TYPE_IS_MULTIPLANAR(b.type) && (b.length > 1) &&
      ((b.flags & V4L2_BUF_FLAG_MVX_BUFFER_FRAME_PRESENT) !=
           V4L2_BUF_FLAG_MVX_BUFFER_FRAME_PRESENT ||
       (b.flags & V4L2_BUF_FLAG_MVX_DECODE_ONLY) ==
           V4L2_BUF_FLAG_MVX_DECODE_ONLY)) {
    return;
  }

  timestamp = b.timestamp.tv_usec;

  vector<iovec> iov = buf.getBytesUsed();

  for (size_t i = 0; i < iov.size(); ++i) {
    write(iov[i].iov_base, iov[i].iov_len);
    totalSize += iov[i].iov_len;
  }
}

OutputFile::OutputFile(ostream &output, uint32_t format)
    : Output(format), output(output) {}

void OutputFile::write(void *ptr, size_t nbytes) {
  if (output.good()) {
    output.write(static_cast<char *>(ptr), nbytes);
    output.flush();
  }
}

OutputIVF::OutputIVF(ofstream &output, uint32_t format, uint16_t width,
                     uint16_t height)
    : OutputFile(output, format) {
  IVFHeader header(format, width, height);
  write(reinterpret_cast<char *>(&header), sizeof(header));
}

void OutputIVF::finalize(Buffer &buf) {
  vector<iovec> iov = buf.getBytesUsed();
  v4l2_buffer &b = buf.getBuffer();
  if (V4L2_TYPE_IS_MULTIPLANAR(b.type) && (b.length > 1) &&
      (b.flags & V4L2_BUF_FLAG_MVX_BUFFER_FRAME_PRESENT) !=
          V4L2_BUF_FLAG_MVX_BUFFER_FRAME_PRESENT) {
    return;
  }

  for (size_t i = 0; i < iov.size(); ++i) {
    size_t n = temp.size();
    temp.resize(temp.size() + iov[i].iov_len);

    char *p = static_cast<char *>(iov[i].iov_base);
    copy(p, p + iov[i].iov_len, &temp[n]);
  }

  if ((b.flags & V4L2_BUF_FLAG_KEYFRAME) || (b.flags & V4L2_BUF_FLAG_PFRAME) ||
      (b.flags & V4L2_BUF_FLAG_BFRAME)) {
    IVFFrame frame(temp.size(), b.timestamp.tv_usec);
    write(&frame, sizeof(frame));

    write(&temp[0], temp.size());
    totalSize += temp.size();
    temp.clear();
  }
}

OutputAFBC::OutputAFBC(std::ofstream &output, uint32_t format, bool tiled)
    : OutputFile(output, format), tiled(tiled) {}

void OutputAFBC::prepare(Buffer &buf) {
  buf.clearBytesUsed();
  buf.setTiled(tiled);
}

void OutputAFBC::finalize(Buffer &buf) {
  vector<iovec> iov = buf.getBytesUsed();
  v4l2_buffer &b = buf.getBuffer();
  if (V4L2_TYPE_IS_MULTIPLANAR(b.type) &&
      (b.flags & V4L2_BUF_FLAG_MVX_BUFFER_FRAME_PRESENT) !=
          V4L2_BUF_FLAG_MVX_BUFFER_FRAME_PRESENT) {
    return;
  }

  if (iov[0].iov_len > 0) {
    AFBCHeader header(buf.getFormat(), iov[0].iov_len, buf.getCrop(), tiled);
    write(&header, sizeof(header));
    OutputFile::finalize(buf);
  }
}

OutputAFBCInterlaced::OutputAFBCInterlaced(std::ofstream &output,
                                           uint32_t format, bool tiled)
    : OutputAFBC(output, format, tiled) {}

void OutputAFBCInterlaced::finalize(Buffer &buf) {
  vector<iovec> iov = buf.getBytesUsed();
  v4l2_buffer &b = buf.getBuffer();
  if (V4L2_TYPE_IS_MULTIPLANAR(b.type) &&
      (b.flags & V4L2_BUF_FLAG_MVX_BUFFER_FRAME_PRESENT) == 0) {
    return;
  }

  if (iov[0].iov_len == 0) {
    return;
  }

  size_t top_len = roundUp(iov[0].iov_len / 2, 32);
  AFBCHeader top_header(buf.getFormat(), top_len, buf.getCrop(), tiled,
                        AFBCHeader::FIELD_TOP);

  write(&top_header, sizeof(top_header));
  write(iov[0].iov_base, top_len);

  size_t bot_len = iov[0].iov_len - top_len;
  AFBCHeader bot_header(buf.getFormat(), bot_len, buf.getCrop(), tiled,
                        AFBCHeader::FIELD_BOTTOM);

  write(&bot_header, sizeof(bot_header));
  write(static_cast<char *>(iov[0].iov_base) + top_len, bot_len);
  totalSize += top_len + bot_len;
}

OutputFileWithMD5::OutputFileWithMD5(std::ofstream &output, uint32_t format,
                                     std::ofstream &output_md5,
                                     std::ifstream *md5ref)
    : OutputFile(output, format),
      output_md5(output_md5),
      input_ref_md5(md5ref)

{
  md5_check_result = true;
}

void OutputFileWithMD5::finalize(Buffer &buf) {
  MD5_CTX ctx;
  char str_hash[STR_HASH_SIZE];
  uint32_t data[HASH_DIGEST_LENGTH / sizeof(uint32_t)];
  uint8_t *hash = (uint8_t *)data;
  static char const slookup[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                 '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  int i;
  vector<iovec> iov;
  v4l2_buffer &b = buf.getBuffer();
  if (V4L2_TYPE_IS_MULTIPLANAR(b.type) && (b.length > 1) &&
      ((b.flags & V4L2_BUF_FLAG_MVX_BUFFER_FRAME_PRESENT) !=
           V4L2_BUF_FLAG_MVX_BUFFER_FRAME_PRESENT ||
       (b.flags & V4L2_BUF_FLAG_MVX_DECODE_ONLY) ==
           V4L2_BUF_FLAG_MVX_DECODE_ONLY)) {
    return;
  }
  if (getFormat() == V4L2_PIX_FMT_P010) {
    iov = buf.convert10Bit();
  } else {
    iov = buf.getBytesUsed();
  }
  OutputFile::finalize(buf);

  if (iov[0].iov_len) {
    MD5_Init(&ctx);
    for (i = 0; (size_t)i < iov.size(); ++i) {
      MD5_Update(&ctx, iov[i].iov_base, iov[i].iov_len);
    }
    MD5_Finalize(&ctx);
    MD5_GetHash(&ctx, hash);

    for (i = 0; i < HASH_DIGEST_LENGTH; ++i) {
      str_hash[i << 1] = slookup[hash[i] >> 4];
      str_hash[(i << 1) + 1] = slookup[hash[i] & 0xF];
    }
    str_hash[sizeof(str_hash) - 2] = '\r';
    str_hash[sizeof(str_hash) - 1] = '\n';

    output_md5.write(static_cast<char *>(str_hash), STR_HASH_SIZE);
    output_md5.flush();
#if 0
        printf("hash: %02x,%02x,%02x,%02x,"
                    "%02x,%02x,%02x,%02x,"
                    "%02x,%02x,%02x,%02x,"
                    "%02x,%02x,%02x,%02x.\n",
                    hash[0], hash[1], hash[2], hash[3],
                    hash[4], hash[5], hash[6], hash[7],
                    hash[8], hash[9], hash[10], hash[11],
                    hash[12], hash[13], hash[14], hash[15]);
#endif
  }

  if (input_ref_md5 != NULL) {
    bool md5_result = checkMd5(str_hash);
    if (!md5_result) {
      printf("[Test Result] Compare MD5 FAIL!!!-----\n");
      md5_check_result = false;
    } else {
      // printf("[Test Result] Compare MD5 PASS!!!-----\n");
    }
  }
}

bool OutputFileWithMD5::checkMd5(char *cur_str_hash) {
  char cmp_hash[STR_HASH_SIZE];
  bool result = true;
  input_ref_md5->getline(cmp_hash, STR_HASH_SIZE);
  // printf("--cur_str_hash: %s", cur_str_hash);
  // printf("--cmp_hash:     %s\n", cmp_hash);
  if (0 != memcmp(cur_str_hash, cmp_hash, sizeof(cmp_hash) - 2)) {
    result = false;
  }
  return result;
}

bool OutputFileWithMD5::getMd5CheckResult() { return md5_check_result; }

/****************************************************************************
 * Buffer
 ****************************************************************************/

Buffer::Buffer(const v4l2_format &format) : format(format) {
  isRoiCfg = false;
  qp = 0;
}

Buffer::Buffer(v4l2_buffer &buf, int fd, const v4l2_format &format)
    : buf(buf), format(format) {
  memset(ptr, 0, sizeof(ptr));
  if (buf.memory == V4L2_MEMORY_DMABUF) {
    bufferAllocator = CreateDmabufHeapBufferAllocator();
  }

  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    memcpy(planes, buf.m.planes, sizeof(planes[0]) * buf.length);
    this->buf.m.planes = planes;
  }
  isRoiCfg = false;
  qp = 0;
  memoryMap(fd);
}

Buffer::~Buffer() {
  memoryUnmap();
  if (buf.memory == V4L2_MEMORY_DMABUF && bufferAllocator != NULL) {
    close(dma_fd);
    FreeDmabufHeapBufferAllocator(bufferAllocator);
  }
}

v4l2_buffer &Buffer::getBuffer() { return buf; }

const v4l2_format &Buffer::getFormat() const { return format; }

void Buffer::setCrop(const v4l2_crop &crop) { this->crop = crop; }

const v4l2_crop &Buffer::getCrop() const { return crop; }

vector<iovec> Buffer::getImageSize() const {
  vector<iovec> iova;

  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    for (unsigned int i = 0; i < buf.length; ++i) {
      buf.m.planes[i].length = plane_length[i];
      buf.m.planes[i].data_offset = 0;
      iovec iov = {.iov_base = ptr[i], .iov_len = buf.m.planes[i].length};
      iova.push_back(iov);
    }
  } else {
    iovec iov = {.iov_base = ptr[0], .iov_len = buf.length};
    iova.push_back(iov);
  }

  return iova;
}

vector<iovec> Buffer::convert10Bit() {
  unsigned int i = 0;
  unsigned short *y, *tmp_uv, *u, *v;
  size_t y_size, uv_size;
  vector<iovec> iova;
  v4l2_plane &y_p = buf.m.planes[0];
  y = static_cast<unsigned short *>(ptr[0]) + y_p.data_offset;
  y_size = y_p.bytesused - y_p.data_offset;
  v4l2_plane &u_p = buf.m.planes[1];
  u = static_cast<unsigned short *>(ptr[1]) + u_p.data_offset;
  uv_size = u_p.bytesused - u_p.data_offset;
  // v4l2_plane &v_p = buf.m.planes[1];
  // v = static_cast<unsigned short *>(ptr[2]) + v_p.data_offset;
  v = u + uv_size / (2 * sizeof(short));
  // buf.length = 3;
  iovec iov;

  tmp_uv = static_cast<unsigned short *>(
      calloc(uv_size / sizeof(short), sizeof(short)));
  if (NULL == tmp_uv) {
    memset(y, 0xcc, y_size);
    iov = {.iov_base = y, .iov_len = y_p.bytesused - y_p.data_offset};
    iova.push_back(iov);
  } else {
    memcpy(tmp_uv, u, uv_size);
    memset(u, 0, uv_size);
    for (i = 0; i < y_size / sizeof(short); i++) {
      y[i] = y[i] >> 6;
    }
    iov = {.iov_base = y, .iov_len = y_p.bytesused - y_p.data_offset};
    iova.push_back(iov);
    for (i = 0; i < uv_size / (2 * sizeof(short)); i++) {
      u[i] = tmp_uv[2 * i] >> 6;
      v[i] = tmp_uv[2 * i + 1] >> 6;
    }
    iov = {.iov_base = u, .iov_len = (u_p.bytesused - u_p.data_offset) / 2};
    iova.push_back(iov);
    iov = {.iov_base = v, .iov_len = (u_p.bytesused - u_p.data_offset) / 2};
    iova.push_back(iov);
    free(tmp_uv);
  }
  return iova;
}
vector<iovec> Buffer::getBytesUsed() const {
  vector<iovec> iova;

  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    for (unsigned int i = 0; i < buf.length; ++i) {
      v4l2_plane &p = buf.m.planes[i];
      iovec iov;

      if (buf.memory == V4L2_MEMORY_MMAP || buf.memory == V4L2_MEMORY_USERPTR) {
        iov = {.iov_base = static_cast<char *>(ptr[i]) + p.data_offset,
               .iov_len = p.bytesused - p.data_offset};
      } else if (buf.memory == V4L2_MEMORY_DMABUF) {
        iov = {.iov_base = static_cast<char *>(ptr[0]) + p.data_offset,
               .iov_len = p.bytesused - p.data_offset};
      }

      if (p.bytesused < p.data_offset) {
        iov.iov_len = 0;
      }

      iova.push_back(iov);
    }
  } else {
    iovec iov = {.iov_base = ptr[0], .iov_len = buf.bytesused};

    /*
     * Single planar buffers has no support for offset, but for HEVC and VP9
     * encode we must find a way to relay the offset from the code.
     *
     * For MMAP we use the lower 12 bits (assuming 4k page size) to relay
     * the offset.
     *
     * For userptr the actual pointer is updated to point at the first byte
     * of the data.
     *
     * Because there is no offset 'bytesused' does not have to be adjusted
     * similar to multi planar buffers.
     */
    switch (buf.memory) {
      case V4L2_MEMORY_MMAP:
        iov.iov_base = static_cast<char *>(iov.iov_base) +
                       (buf.m.offset & ((1 << 12) - 1));
        break;
      case V4L2_MEMORY_USERPTR:
        iov.iov_base = reinterpret_cast<void *>(buf.m.userptr);
        break;
      default:
        break;
    }

    iova.push_back(iov);
  }

  return iova;
}

void Buffer::setBytesUsed(vector<iovec> &iov) {
  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    if (iov.size() > buf.length) {
      throw Exception(
          "iovec vector size is larger than V4L2 buffer number of planes. "
          "size=%zu, planes=%u",
          iov.size(), buf.length);
    }

    size_t i;
    for (i = 0; i < iov.size(); ++i) {
      buf.m.planes[i].bytesused = iov[i].iov_len;
      if (buf.memory == V4L2_MEMORY_DMABUF) {
        buf.m.planes[i].m.fd = dma_fd;
        if (buf.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE &&
            iov[i].iov_len != 0) {
          buf.m.planes[i].data_offset = plane_offset[i];
          buf.m.planes[i].bytesused += plane_offset[i];
          buf.m.planes[i].length = buf.m.planes[i].bytesused;
        }
      } else if (buf.memory == V4L2_MEMORY_USERPTR) {
        buf.m.planes[i].m.userptr = (unsigned long)(iov[i].iov_base);
      }
    }

    for (; i < buf.length; ++i) {
      buf.m.planes[i].bytesused = 0;
    }
  } else {
    buf.bytesused = 0;

    for (size_t i = 0; i < iov.size(); ++i) {
      buf.bytesused += iov[i].iov_len;

      if (buf.bytesused > buf.length) {
        throw Exception("V4L2 buffer size too small. length=%u.", buf.length);
      }

      if (buf.memory == V4L2_MEMORY_DMABUF) {
        buf.m.fd = dma_fd;
      } else if (buf.memory == V4L2_MEMORY_USERPTR) {
        buf.m.userptr = (unsigned long)(iov[i].iov_base);
      }
    }
  }
}

void Buffer::clearBytesUsed() {
  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    for (size_t i = 0; i < buf.length; ++i) {
      buf.m.planes[i].bytesused = 0;
      if (buf.memory == V4L2_MEMORY_DMABUF) {
        buf.m.planes[i].m.fd = dma_fd;
        buf.m.planes[i].data_offset = 0;
      } else if (buf.memory == V4L2_MEMORY_USERPTR) {
        buf.m.planes[i].m.userptr = (unsigned long)ptr[i];
        buf.m.planes[i].data_offset = 0;
      }
    }
  } else {
    buf.bytesused = 0;
    if (buf.memory == V4L2_MEMORY_DMABUF) {
      buf.m.fd = dma_fd;
    } else if (buf.memory == V4L2_MEMORY_USERPTR) {
      buf.m.userptr = (unsigned long)ptr[0];
    }
  }
}

void Buffer::resetVendorFlags() { buf.flags &= ~V4L2_BUF_FLAG_MVX_MASK; }

void Buffer::setCodecConfig(bool codecConfig) {
  buf.flags &= ~V4L2_BUF_FLAG_MVX_CODEC_CONFIG;
  buf.flags |= codecConfig ? V4L2_BUF_FLAG_MVX_CODEC_CONFIG : 0;
}

void Buffer::setTimeStamp(unsigned int timeUs) {
  buf.flags |= V4L2_BUF_FLAG_TIMESTAMP_COPY;
  buf.timestamp.tv_sec = timeUs / 1000000;
  buf.timestamp.tv_usec = timeUs % 1000000;
}

void Buffer::setEndOfFrame(bool eof) {
  buf.flags &= ~V4L2_BUF_FLAG_KEYFRAME;
  buf.flags |= eof ? V4L2_BUF_FLAG_KEYFRAME : 0;
}

void Buffer::setEndOfSubFrame(bool eosf) {
  buf.flags &= ~V4L2_BUF_FLAG_END_OF_SUB_FRAME;
  buf.flags |= eosf ? V4L2_BUF_FLAG_END_OF_SUB_FRAME : 0;
}

void Buffer::setRotation(int rotation) {
  if (rotation % 90 != 0) {
    return;
  }

  switch (rotation % 360) {
    case 90:
      buf.flags &= ~V4L2_BUF_FRAME_FLAG_ROTATION_MASK;
      buf.flags |= V4L2_BUF_FRAME_FLAG_ROTATION_90;
      break;
    case 180:
      buf.flags &= ~V4L2_BUF_FRAME_FLAG_ROTATION_MASK;
      buf.flags |= V4L2_BUF_FRAME_FLAG_ROTATION_180;
      break;
    case 270:
      buf.flags &= ~V4L2_BUF_FRAME_FLAG_ROTATION_MASK;
      buf.flags |= V4L2_BUF_FRAME_FLAG_ROTATION_270;
      break;
    default:
      break;
  }
  return;
}

void Buffer::setDownScale(int scale) {
  if (scale == 1) {
    return;
  }
  switch (scale) {
    case 2:
      buf.flags &= ~V4L2_BUF_FRAME_FLAG_SCALING_MASK;
      buf.flags |= V4L2_BUF_FRAME_FLAG_SCALING_2;
      break;
    case 4:
      buf.flags &= ~V4L2_BUF_FRAME_FLAG_SCALING_MASK;
      buf.flags |= V4L2_BUF_FRAME_FLAG_SCALING_4;
      break;
    default:
      printf("didnot support this scale factor :%d", scale);
      break;
  }
  return;
}

void Buffer::setMirror(int mirror) {
  if (mirror == 0) {
    return;
  } else {
    if (mirror == 1) {
      buf.flags &= ~V4L2_BUF_FRAME_FLAG_MIRROR_MASK;
      buf.flags |= V4L2_BUF_FRAME_FLAG_MIRROR_HORI;
    } else if (mirror == 2) {
      buf.flags &= ~V4L2_BUF_FRAME_FLAG_MIRROR_MASK;
      buf.flags |= V4L2_BUF_FRAME_FLAG_MIRROR_VERT;
    }
  }
  return;
}

void Buffer::setEndOfStream(bool eos) {
  buf.flags &= ~V4L2_BUF_FLAG_LAST;
  buf.flags |= eos ? V4L2_BUF_FLAG_LAST : 0;
}

void Buffer::setROIflag() {
  buf.flags &= ~V4L2_BUF_FLAG_MVX_BUFFER_ROI;
  buf.flags |= V4L2_BUF_FLAG_MVX_BUFFER_ROI;
}

void Buffer::setEPRflag() {
  buf.flags &= ~V4L2_BUF_FLAG_MVX_BUFFER_EPR;
  buf.flags |= V4L2_BUF_FLAG_MVX_BUFFER_EPR;
}

void Buffer::update(v4l2_buffer &b) {
  buf = b;

  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    buf.m.planes = planes;
    for (size_t i = 0; i < buf.length; ++i) {
      buf.m.planes[i] = b.m.planes[i];
    }
  }
}

void Buffer::memoryMap(int fd) {
  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    for (uint32_t i = 0; i < buf.length; ++i) {
      v4l2_plane &p = buf.m.planes[i];

      if (p.length > 0) {
        if (buf.memory == V4L2_MEMORY_MMAP) {
          ptr[i] = mmap(NULL, p.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                        p.m.mem_offset);
        } else if (buf.memory == V4L2_MEMORY_USERPTR) {
          ptr[i] = mmap(NULL, p.length, PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        }
        if (ptr[i] == MAP_FAILED) {
          throw Exception("Failed to mmap multi memory.");
        }
        plane_length[i] = p.length;
        total_length += p.length;
      }
    }

    if (buf.memory == V4L2_MEMORY_DMABUF) {
      bool cpu_access_need = true;
      dma_fd = DmabufHeapAllocSystem(bufferAllocator, cpu_access_need,
                                     total_length, 0, 0);
      ptr[0] = mmap(NULL, total_length, PROT_READ | PROT_WRITE, MAP_SHARED,
                    dma_fd, 0);
      plane_offset[0] = 0;

      for (uint32_t j = 1; j < buf.length; ++j) {
        v4l2_plane &p = buf.m.planes[j];
        int k;
        int offset = 0;
        for (k = j; k > 0; k--) {
          offset += plane_length[j - k];
        }
        plane_offset[j] = offset;
        if (p.length > 0) {
          ptr[j] = static_cast<char *>(ptr[0]) + offset;
        }
      }
    }
  } else {
    if (buf.length > 0) {
      if (buf.memory == V4L2_MEMORY_MMAP) {
        ptr[0] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                      buf.m.offset);
        if (ptr[0] == MAP_FAILED) {
          throw Exception("Failed to mmap memory.");
        }
      } else if (buf.memory == V4L2_MEMORY_DMABUF) {
        total_length = buf.length;
        bool cpu_access_need = true;
        dma_fd = DmabufHeapAllocSystem(bufferAllocator, cpu_access_need,
                                       total_length, 0, 0);
        ptr[0] = mmap(NULL, total_length, PROT_READ | PROT_WRITE, MAP_SHARED,
                      dma_fd, 0);
      } else if (buf.memory == V4L2_MEMORY_USERPTR) {
        ptr[0] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
      }
    }
  }
}

void Buffer::memoryUnmap() {
  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    if (buf.memory == V4L2_MEMORY_MMAP || buf.memory == V4L2_MEMORY_USERPTR) {
      for (uint32_t i = 0; i < buf.length; ++i) {
        if (ptr[i] != 0) {
          munmap(ptr[i], buf.m.planes[i].length);
        }
      }
    } else {
      munmap(ptr[0], total_length);
    }
  } else {
    if (ptr[0]) {
      munmap(ptr[0], buf.length);
    }
  }
}

size_t Buffer::getLength(unsigned int plane) {
  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    if (buf.length >= plane) {
      return 0;
    }

    return buf.m.planes[plane].length;
  } else {
    if (plane > 0) {
      return 0;
    }

    return buf.length;
  }
}

void Buffer::setInterlaced(bool interlaced) {
  buf.field = interlaced ? V4L2_FIELD_SEQ_TB : V4L2_FIELD_NONE;
}

void Buffer::setTiled(bool tiled) {
  buf.flags &= ~(V4L2_BUF_FLAG_MVX_AFBC_TILED_HEADERS |
                 V4L2_BUF_FLAG_MVX_AFBC_TILED_BODY);
  if (tiled) {
    buf.flags |= V4L2_BUF_FLAG_MVX_AFBC_TILED_HEADERS;
    buf.flags |= V4L2_BUF_FLAG_MVX_AFBC_TILED_BODY;
  }
}

void Buffer::setBlockSplit(bool split) {
  buf.flags &= ~(V4L2_BUF_FLAG_MVX_AFBC_BLOCK_SPLIT);
  if (split) {
    buf.flags |= V4L2_BUF_FLAG_MVX_AFBC_BLOCK_SPLIT;
  }
}

void Buffer::setRoiCfg(struct v4l2_mvx_roi_regions roi) {
  roi_cfg = roi;
  isRoiCfg = true;
}

void Buffer::setSuperblock(bool superblock) {
  buf.flags &= ~(V4L2_BUF_FLAG_MVX_AFBC_32X8_SUPERBLOCK);
  if (superblock) {
    buf.flags |= V4L2_BUF_FLAG_MVX_AFBC_32X8_SUPERBLOCK;
  }
}

/****************************************************************************
 * Transcoder, decoder, encoder
 ****************************************************************************/

Codec::Codec(const char *dev, enum v4l2_buf_type inputType,
             enum v4l2_buf_type outputType, ostream &log, bool nonblock)
    : input(fd, inputType, log),
      output(fd, outputType, log),
      log(log),
      csweo(false),
      fps(0),
      bps(0),
      minqp(0),
      maxqp(0),
      fixedqp(0),
      nonblock(nonblock) {
  openDev(dev);
  timestart_us = 0;
  timeend_us = 0;
  avgfps = 0;
}

Codec::Codec(const char *dev, Input &input, enum v4l2_buf_type inputType,
             Output &output, enum v4l2_buf_type outputType, ostream &log,
             bool nonblock)
    : input(fd, input, inputType, log),
      output(fd, output, outputType, log),
      log(log),
      csweo(false),
      fps(0),
      bps(0),
      minqp(0),
      maxqp(0),
      fixedqp(0),
      nonblock(nonblock) {
  openDev(dev);
  timestart_us = 0;
  timeend_us = 0;
  avgfps = 0;
}

Codec::~Codec() {
  freeBuffers();
  closeDev();
}

int Codec::stream() {
  /* Set NALU. */
  if (isVPx(input.io->getFormat())) {
    input.setNALU(NALU_FORMAT_ONE_NALU_PER_BUFFER);
  }

  if (input.io->getFormat() == V4L2_PIX_FMT_VC1_ANNEX_L) {
    input.setNALU(NALU_FORMAT_ONE_NALU_PER_BUFFER);
  }
  if (input.io->getNaluFormat() == NALU_FORMAT_ONE_NALU_PER_BUFFER ||
      input.io->getNaluFormat() == NALU_FORMAT_ONE_BYTE_LENGTH_FIELD ||
      input.io->getNaluFormat() == NALU_FORMAT_TWO_BYTE_LENGTH_FIELD ||
      input.io->getNaluFormat() == NALU_FORMAT_FOUR_BYTE_LENGTH_FIELD) {
    input.setNALU((NaluFormat)input.io->getNaluFormat());
  }
  if ((input.io->getFormat() == V4L2_PIX_FMT_VC1_ANNEX_L) ||
      (input.io->getFormat() == V4L2_PIX_FMT_VC1_ANNEX_G)) {
    struct v4l2_control control;
    int profile = 0xff;

    switch (input.io->getProfile()) {
      case 0: {
        profile = 0;
        break;
      }
      case 4: {
        profile = 1;
        break;
      }
      case 12: {
        profile = 2;
        break;
      }
      default: {
        throw Exception("Unsupported VC1 profile.\n");
      }
    }

    log << "VC1 decoding profile( " << profile << " )" << endl;

    memset(&control, 0, sizeof(control));

    control.id = V4L2_CID_MVE_VIDEO_VC1_PROFILE;
    control.value = profile;

    if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
      throw Exception("Failed to set profile=%u for fmt: %u .", profile,
                      input.io->getFormat());
    }
  }

  /* Add VPx file header. */
  if (isVPx(output.io->getFormat())) {
    output.setNALU(NALU_FORMAT_ONE_NALU_PER_BUFFER);
  }

  try {
    if (input.io->getPreload()) {
      input.io->preloadBuffer(input.type);
    }

    queryCapabilities();
    /* enumerateFormats(); */
    enumerateFramesizes(output.io->getFormat());
    setFormats();
    subscribeEvents();
    allocateBuffers();
    queueBuffers();
    streamon();

    if (nonblock) {
      runPoll();
    } else {
      runThreads();
    }
    streamoff();
  } catch (Exception &e) {
    cerr << "Error: " << e.what() << endl;
    return 1;
  }

  return 0;
}

uint32_t Codec::to4cc(const string &str) {
  if (str.compare("yuv420_afbc_8") == 0) {
    return v4l2_fourcc('Y', '0', 'A', '8');
  } else if (str.compare("yuv420_afbc_10") == 0) {
    return v4l2_fourcc('Y', '0', 'A', 'A');
  } else if (str.compare("yuv422_afbc_8") == 0) {
    return v4l2_fourcc('Y', '2', 'A', '8');
  } else if (str.compare("yuv422_afbc_10") == 0) {
    return v4l2_fourcc('Y', '2', 'A', 'A');
  } else if (str.compare("yuv420") == 0) {
    return V4L2_PIX_FMT_YUV420M;
  } else if (str.compare("yuv420_nv12") == 0) {
    return V4L2_PIX_FMT_NV12;
  } else if (str.compare("yuv420_nv21") == 0) {
    return V4L2_PIX_FMT_NV21;
  } else if (str.compare("yuv420_p010") == 0) {
    return V4L2_PIX_FMT_P010;
  } else if (str.compare("yuv420_y0l2") == 0) {
    return V4L2_PIX_FMT_Y0L2;
  } else if (str.compare("yuv422_yuy2") == 0) {
    return V4L2_PIX_FMT_YUYV;
  } else if (str.compare("yuv422_uyvy") == 0) {
    return V4L2_PIX_FMT_UYVY;
  } else if (str.compare("yuv422_y210") == 0) {
    return V4L2_PIX_FMT_Y210;
  } else if (str.compare("rgba") == 0) {
    return DRM_FORMAT_ABGR8888;
  } else if (str.compare("bgra") == 0) {
    return DRM_FORMAT_ARGB8888;
  } else if (str.compare("argb") == 0) {
    return DRM_FORMAT_BGRA8888;
  } else if (str.compare("abgr") == 0) {
    return DRM_FORMAT_RGBA8888;
  } else if (str.compare("avs2") == 0) {
    return V4L2_PIX_FMT_AVS2;
  } else if (str.compare("avs") == 0) {
    return V4L2_PIX_FMT_AVS;
  } else if (str.compare("h263") == 0) {
    return V4L2_PIX_FMT_H263;
  } else if (str.compare("h264") == 0) {
    return V4L2_PIX_FMT_H264;
  } else if (str.compare("h264_mvc") == 0) {
    return V4L2_PIX_FMT_H264_MVC;
  } else if (str.compare("h264_no_sc") == 0) {
    return V4L2_PIX_FMT_H264_NO_SC;
  } else if (str.compare("hevc") == 0) {
    return V4L2_PIX_FMT_HEVC;
  } else if (str.compare("mjpeg") == 0) {
    return V4L2_PIX_FMT_MJPEG;
  } else if (str.compare("jpeg") == 0) {
    return V4L2_PIX_FMT_JPEG;
  } else if (str.compare("mpeg2") == 0) {
    return V4L2_PIX_FMT_MPEG2;
  } else if (str.compare("mpeg4") == 0) {
    return V4L2_PIX_FMT_MPEG4;
  } else if (str.compare("rv") == 0) {
    return V4L2_PIX_FMT_RV;
  } else if (str.compare("vc1") == 0) {
    return V4L2_PIX_FMT_VC1_ANNEX_G;
  } else if (str.compare("vc1_l") == 0) {
    return V4L2_PIX_FMT_VC1_ANNEX_L;
  } else if (str.compare("vp8") == 0) {
    return V4L2_PIX_FMT_VP8;
  } else if (str.compare("vp9") == 0) {
    return V4L2_PIX_FMT_VP9;
  } else {
    throw Exception("Not a valid format '%s'.\n", str.c_str());
  }

  return 0;
}

bool Codec::isVPx(uint32_t format) {
  return format == V4L2_PIX_FMT_VP8 || format == V4L2_PIX_FMT_VP9;
}

bool Codec::isAFBC(uint32_t format) {
  switch (format) {
    case V4L2_PIX_FMT_YUV420_AFBC_8:
    case V4L2_PIX_FMT_YUV420_AFBC_10:
    case V4L2_PIX_FMT_YUV422_AFBC_8:
    case V4L2_PIX_FMT_YUV422_AFBC_10:
      return true;
    default:
      return false;
  }
}

size_t Codec::getBytesUsed(v4l2_buffer &buf) {
  size_t size = 0;

  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    for (uint32_t i = 0; i < buf.length; ++i) {
      size += buf.m.planes[i].bytesused;
    }
  } else {
    size = buf.bytesused;
  }

  return size;
}

void Codec::openDev(const char *dev) {
  int flags = O_RDWR;

  log << "Opening '" << dev << "'." << endl;

  if (nonblock) {
    flags |= O_NONBLOCK;
  }

  /* Open the video device in read/write mode. */
  fd = open(dev, flags);
  if (fd < 0) {
    throw Exception("Failed to open device.");
  }
}

void Codec::closeDev() {
  log << "Closing fd " << fd << "." << endl;
  close(fd);
  fd = -1;
}

void Codec::queryCapabilities() {
  struct v4l2_capability cap;
  int ret;

  /* Query capabilities. */
  ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
  if (ret != 0) {
    throw Exception("Failed to query for capabilities");
  }

  if ((cap.capabilities & (V4L2_CAP_VIDEO_M2M | V4L2_CAP_VIDEO_M2M_MPLANE)) ==
      0) {
    throw Exception("Device is missing m2m support.");
  }
}

void Codec::enumerateFormats() {
  input.enumerateFormats();
  output.enumerateFormats();
}

void Codec::Port::enumerateFormats() {
  struct v4l2_fmtdesc fmtdesc;
  int ret;

  fmtdesc.index = 0;
  fmtdesc.type = type;

  while (1) {
    ret = ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc);
    if (ret != 0) {
      break;
    }

    log << "fmt: index=" << fmtdesc.index << ", type=" << fmtdesc.type
        << " , flags=" << hex << fmtdesc.flags
        << ", pixelformat=" << fmtdesc.pixelformat
        << ", description=" << fmtdesc.description << endl;

    fmtdesc.index++;
  }

  printf("\n");
}

void Codec::enumerateFramesizes(uint32_t format) {
  struct v4l2_frmsizeenum frmsize;

  frmsize.index = 0;
  frmsize.pixel_format = format;

  int ret = ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
  if (ret != 0) {
    throw Exception("Failed to enumerate frame sizes. ret=%d.\n", ret);
  }

  log << "Enumerate frame size."
      << " index=" << frmsize.index << ", pixel_format=" << hex
      << frmsize.pixel_format << dec;

  switch (frmsize.type) {
    case V4L2_FRMIVAL_TYPE_DISCRETE:
      break;
    case V4L2_FRMIVAL_TYPE_CONTINUOUS:
    case V4L2_FRMIVAL_TYPE_STEPWISE:
      log << ", min_width=" << frmsize.stepwise.min_width
          << ", max_width=" << frmsize.stepwise.max_width
          << ", step_width=" << frmsize.stepwise.step_width
          << ", min_height=" << frmsize.stepwise.min_height
          << ", max_height=" << frmsize.stepwise.max_height
          << ", step_height=" << frmsize.stepwise.step_height;
      break;
    default:
      throw Exception("Unsupported enumerate frame size type. type=%d.\n",
                      frmsize.type);
  }

  log << endl;
}

const v4l2_format &Codec::Port::getFormat() {
  /* Get and print format. */
  format.type = type;
  int ret = ioctl(fd, VIDIOC_G_FMT, &format);
  if (ret != 0) {
    throw Exception("Failed to get format.");
  }

  return format;
}

void Codec::Port::tryFormat(v4l2_format &format) {
  int ret = ioctl(fd, VIDIOC_TRY_FMT, &format);
  if (ret != 0) {
    throw Exception("Failed to try format.");
  }
}

void Codec::Port::setFormat(v4l2_format &format) {
  int ret = ioctl(fd, VIDIOC_S_FMT, &format);
  if (ret != 0) {
    throw Exception("Failed to set format.");
  }

  this->format = format;
}

void Codec::Port::getTrySetFormat() {
  size_t width = 0, height = 0;

  v4l2_format fmt = getFormat();
  if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
    struct v4l2_pix_format_mplane &f = fmt.fmt.pix_mp;

    f.pixelformat = io->getFormat();
    f.width = io->getWidth();
    f.height = io->getHeight();
    f.num_planes = 3;
    // f.field = interlaced ? V4L2_FIELD_SEQ_TB : V4L2_FIELD_NONE;

    for (int i = 0; i < 3; ++i) {
      f.plane_fmt[i].bytesperline = 0;
      f.plane_fmt[i].sizeimage = 0;
    }
    if (V4L2_TYPE_IS_OUTPUT(type) &&
        (f.pixelformat == V4L2_PIX_FMT_YUV420_AFBC_8 ||
         f.pixelformat == V4L2_PIX_FMT_YUV420_AFBC_10 ||
         f.pixelformat == V4L2_PIX_FMT_YUV422_AFBC_8 ||
         f.pixelformat == V4L2_PIX_FMT_YUV422_AFBC_10)) {
      f.plane_fmt[0].sizeimage = io->getFrameSize();
      log << "getTrySetFormat. set size image to " << io->getFrameSize()
          << endl;
    }
  } else {
    struct v4l2_pix_format &f = fmt.fmt.pix;

    f.pixelformat = io->getFormat();
    f.width = io->getWidth();
    f.height = io->getHeight();
    f.bytesperline = 0;
    f.sizeimage = 1 * 1024 * 1024;
    // f.field = interlaced ? V4L2_FIELD_SEQ_TB : V4L2_FIELD_NONE;
  }

  /* Try format. */
  tryFormat(fmt);

  if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
    struct v4l2_pix_format_mplane &f = fmt.fmt.pix_mp;
    width = f.width;
    height = f.height;
  } else {
    struct v4l2_pix_format &f = fmt.fmt.pix;
    width = f.width;
    height = f.height;
  }
  // for dsl frame case, this is not suitable, remove this.
  if (V4L2_TYPE_IS_OUTPUT(type) &&
      (width != io->getWidth() || height != io->getHeight())) {
    // throw Exception("Selected resolution is not supported for this format
    // width:%d, io width:%d", width, io->getWidth());
  }

  setFormat(fmt);

  printFormat(fmt);
}

void Codec::setFormats() {
  input.getTrySetFormat();
  output.getTrySetFormat();
}

void Codec::Port::printFormat(const struct v4l2_format &format) {
  if (V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
    const struct v4l2_pix_format_mplane &f = format.fmt.pix_mp;

    log << "Format:" << dec << " type=" << format.type
        << ", format=" << f.pixelformat << ", width=" << f.width
        << ", height=" << f.height << ", nplanes=" << int(f.num_planes)
        << ", bytesperline=[" << f.plane_fmt[0].bytesperline << ", "
        << f.plane_fmt[1].bytesperline << ", " << f.plane_fmt[2].bytesperline
        << "]"
        << ", sizeimage=[" << f.plane_fmt[0].sizeimage << ", "
        << f.plane_fmt[1].sizeimage << ", " << f.plane_fmt[2].sizeimage << "]"
        << ", interlaced:" << f.field << endl;
  } else {
    const struct v4l2_pix_format &f = format.fmt.pix;

    log << "Format:" << dec << " type=" << format.type
        << ", format=" << f.pixelformat << ", width=" << f.width
        << ", height=" << f.height << ", sizeimage=" << f.sizeimage
        << ", bytesperline=" << f.bytesperline << ", interlaced:" << f.field
        << endl;
  }
}

const v4l2_crop Codec::Port::getCrop() {
  v4l2_crop crop = {.type = type};

  int ret = ioctl(fd, VIDIOC_G_CROP, &crop);
  if (ret != 0) {
    throw Exception("Failed to get crop.");
  }

  return crop;
}

void Codec::Port::setInterlaced(bool interlaced) {
  this->interlaced = interlaced;
}

void Codec::Port::tryEncStopCmd(bool tryStop) { this->tryEncStop = tryStop; }

void Codec::Port::tryDecStopCmd(bool tryStop) { this->tryDecStop = tryStop; }

v4l2_mvx_color_desc Codec::getColorDesc() {
  v4l2_mvx_color_desc color;

  int ret = ioctl(fd, VIDIOC_G_MVX_COLORDESC, &color);
  if (ret != 0) {
    throw Exception("Failed to get color description.");
  }

  return color;
}

void Codec::printColorDesc(const v4l2_mvx_color_desc &color) {
  log << "Color desc. range=" << static_cast<unsigned int>(color.range)
      << ", primaries=" << static_cast<unsigned int>(color.primaries)
      << ", transfer=" << static_cast<unsigned int>(color.transfer)
      << ", matrix=" << static_cast<unsigned int>(color.matrix);

  if (color.flags & V4L2_MVX_COLOR_DESC_DISPLAY_VALID) {
    log << ", display={"
        << "r={x=" << color.display.r.x << ", y=" << color.display.r.y << "}"
        << ", g={x=" << color.display.g.x << ", y=" << color.display.g.y << "}"
        << ", b={x=" << color.display.b.x << ", y=" << color.display.b.y << "}"
        << ", w={x=" << color.display.w.x << ", y=" << color.display.w.y << "}";
  }

  if (color.flags & V4L2_MVX_COLOR_DESC_CONTENT_VALID) {
    log << ", luminance_min=" << color.display.luminance_min * 0.00002
        << ", lumiance_max=" << color.display.luminance_max * 0.00002 << "}"
        << ", content={luminance_max=" << color.content.luminance_max * 0.00002
        << ", luminance_average=" << color.content.luminance_average * 0.00002
        << "}";
  }

  log << endl;
}

void Codec::subscribeEvents() {
  subscribeEvents(V4L2_EVENT_EOS);
  subscribeEvents(V4L2_EVENT_SOURCE_CHANGE);
  subscribeEvents(V4L2_EVENT_MVX_COLOR_DESC);
}

void Codec::subscribeEvents(uint32_t event) {
  struct v4l2_event_subscription sub = {.type = event, .id = 0};
  int ret;

  ret = ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
  if (ret != 0) {
    throw Exception("Failed to subscribe for event.");
  }
}

void Codec::unsubscribeEvents() { unsubscribeEvents(V4L2_EVENT_ALL); }

void Codec::unsubscribeEvents(uint32_t event) {
  struct v4l2_event_subscription sub;
  int ret;

  sub.type = event;
  ret = ioctl(fd, VIDIOC_UNSUBSCRIBE_EVENT, &sub);
  if (ret != 0) {
    throw Exception("Failed to unsubscribe for event.");
  }
}

void Codec::allocateBuffers() {
  int intput_count = 6;
  if (input.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
    intput_count = INPUT_NUM_BUFFERS;
  }
  input.allocateBuffers(intput_count);
  output.allocateBuffers(6);
}

void Codec::Port::allocateBuffers(size_t count) {
  struct v4l2_requestbuffers reqbuf;
  uint32_t i;
  int ret;

  /* Free existing meta buffer. */
  freeBuffers();

  /* Request new buffer to be allocated. */
  reqbuf.count = io->needDoubleCount() ? count * 2 : count;
  reqbuf.type = type;
  if (memory_type == V4L2_MEMORY_MMAP) {
    reqbuf.memory = V4L2_MEMORY_MMAP;
  } else if (memory_type == V4L2_MEMORY_DMABUF) {
    reqbuf.memory = V4L2_MEMORY_DMABUF;
  } else if (memory_type == V4L2_MEMORY_USERPTR) {
    reqbuf.memory = V4L2_MEMORY_USERPTR;
  }
  if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE && count != 0) {
    reqbuf.count = reqbuf.count + OUTPUT_EXTRA_NUM_BUFFERS;
  }

  ret = ioctl(fd, VIDIOC_REQBUFS, &reqbuf);
  if (ret != 0) {
    throw Exception("Failed to request buffers.");
  }

  log << "Request buffers."
      << " type=" << reqbuf.type << ", count=" << reqbuf.count
      << ", memory=" << reqbuf.memory << endl;

  /* Reset number of buffers queued to driver. */
  pending = 0;

  /* Query each buffer and create a new meta buffer. */
  for (i = 0; i < reqbuf.count; ++i) {
    v4l2_buffer buf;
    struct v4l2_plane planes[VIDEO_MAX_PLANES];

    buf.type = type;
    if (memory_type == V4L2_MEMORY_MMAP) {
      buf.memory = V4L2_MEMORY_MMAP;
    } else if (memory_type == V4L2_MEMORY_DMABUF) {
      buf.memory = V4L2_MEMORY_DMABUF;
    } else if (memory_type == V4L2_MEMORY_USERPTR) {
      buf.memory = V4L2_MEMORY_USERPTR;
    }
    buf.index = i;
    buf.length = 3;
    buf.m.planes = planes;

    ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
    if (ret != 0) {
      throw Exception("Failed to query buffer. ret=%d, errno=%d", ret, errno);
    }

    printBuffer(buf, "Query");

    buffers[buf.index] = new Buffer(buf, fd, format);
  }
}

void Codec::freeBuffers() {
  input.freeBuffers();
  output.freeBuffers();
}

void Codec::Port::freeBuffers() {
  while (!buffers.empty()) {
    BufferMap::iterator it = buffers.begin();
    delete (it->second);
    buffers.erase(it);
  }
}

unsigned int Codec::Port::getBufferCount() {
  struct v4l2_control control;

  control.id = V4L2_TYPE_IS_OUTPUT(type) ? V4L2_CID_MIN_BUFFERS_FOR_OUTPUT
                                         : V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
  if (-1 == ioctl(fd, VIDIOC_G_CTRL, &control)) {
    throw Exception("Failed to get minimum buffers.");
  }

  return control.value;
}

void Codec::queueBuffers() {
  output.queueBuffers();
  input.queueBuffers();
}

void Codec::Port::queueBuffers() {
  for (BufferMap::iterator it = buffers.begin(); it != buffers.end(); ++it) {
    Buffer &buffer = *(it->second);
    if (!io->eof()) {
      /* Remove vendor custom flags. */
      buffer.resetVendorFlags();
      io->prepare(buffer);
      buffer.setEndOfStream(io->eof());
      queueBuffer(buffer);
    }
  }
}

void Codec::Port::controlFramerate() {
  struct timeval curtime;
  gettimeofday(&curtime, NULL);
  uint64_t curTimestamp = curtime.tv_sec * 1000000ll + curtime.tv_usec;
  if (lastTimestamp == 0) {
    lastTimestamp = curTimestamp;
  } else {
    uint64_t diffTime = curTimestamp - lastTimestamp;
    if (diffTime >= intervalTime) {
      remainTime += diffTime - intervalTime;
    } else if (diffTime + remainTime >= intervalTime) {
      remainTime = diffTime + remainTime - intervalTime;
    } else {
      usleep(intervalTime - (diffTime + remainTime));
      remainTime = 0;
    }
    gettimeofday(&curtime, NULL);
    uint64_t sentTimestamp = curtime.tv_sec * 1000000ll + curtime.tv_usec;
    // printf("---------queue buffer[%d]. last time: %lu us, cur time: %lu us,
    // diffTime: %lu us, remainTime: %lu us, intervalTime: %lu
    // us--------------\n", frames_processed, lastTimestamp/1000l,
    // curTimestamp/1000l, diffTime, remainTime, intervalTime);
    lastTimestamp = sentTimestamp;
  }
}

void Codec::Port::queueBuffer(Buffer &buf) {
  v4l2_buffer &b = buf.getBuffer();
  int ret;
  buf.setInterlaced(interlaced);
  buf.setRotation(rotation);
  buf.setMirror(mirror);
  buf.setDownScale(scale);

  if (buf.getRoiCfgflag() && getBytesUsed(b) != 0) {
    struct v4l2_mvx_roi_regions roi = buf.getRoiCfg();
    ret = ioctl(fd, VIDIOC_S_MVX_ROI_REGIONS, &roi);
    if (ret != 0) {
      throw Exception("Failed to queue roi param.");
    }
  }

  if (buf.getQPofEPR() > 0) {
    int qp = buf.getQPofEPR();
    ret = ioctl(fd, VIDIOC_S_MVX_QP_EPR, &qp);
    if (ret != 0) {
      throw Exception("Failed to queue roi param.");
    }
    buf.setQPofEPR(0);
  }
  /* Mask buffer offset. */
  if (!V4L2_TYPE_IS_MULTIPLANAR(b.type)) {
    switch (b.memory) {
      case V4L2_MEMORY_MMAP:
        b.m.offset &= ~((1 << 12) - 1);
        break;
      default:
        break;
    }
  }
  // encoder specfied frames count to be processed
  if (io->getDir() == 0 && frames_count > 0 &&
      frames_processed >= frames_count - 1 && !buf.isGeneralBuffer()) {
    if (frames_processed >= frames_count) {
      buf.clearBytesUsed();
      buf.resetVendorFlags();
      frames_processed--;
    }
    buf.setEndOfStream(true);
  }

  // printBuffer(b, "->");

  ret = ioctl(fd, VIDIOC_QBUF, &b);
  if (ret != 0) {
    throw Exception("Failed to queue buffer.");
  }
  if (io->getDir() == 0 && V4L2_TYPE_IS_MULTIPLANAR(b.type) &&
      !buf.isGeneralBuffer()) {
    frames_processed++;
    // printf("-------------queueBuffer yuv frames: %d, frames_count:
    // %d-------------\n", frames_processed, frames_count);
  }

  ++pending;
}

Buffer &Codec::Port::dequeueBuffer() {
  v4l2_plane planes[VIDEO_MAX_PLANES];
  v4l2_buffer buf;
  buf.m.planes = planes;
  int ret;

  buf.type = type;
  if (memory_type == V4L2_MEMORY_MMAP) {
    buf.memory = V4L2_MEMORY_MMAP;
  } else if (memory_type == V4L2_MEMORY_DMABUF) {
    buf.memory = V4L2_MEMORY_DMABUF;
  } else if (memory_type == V4L2_MEMORY_USERPTR) {
    buf.memory = V4L2_MEMORY_USERPTR;
  }
  buf.length = 3;

  ret = ioctl(fd, VIDIOC_DQBUF, &buf);
  if (ret != 0) {
    throw Exception("Failed to dequeue buffer. type=%u, memory=%u", buf.type,
                    buf.memory);
  }

  --pending;
  // printBuffer(buf, "<-");

  Buffer &buffer = *(buffers.at(buf.index));
  buffer.update(buf);

  buffer.setCrop(getCrop());

  return buffer;
}

void Codec::Port::printBuffer(const v4l2_buffer &buf, const char *prefix) {
  log << prefix << ": "
      << "type=" << buf.type << ", index=" << buf.index
      << ", sequence=" << buf.sequence << ", timestamp={"
      << buf.timestamp.tv_sec << ", " << buf.timestamp.tv_usec << "}"
      << ", flags=" << hex << buf.flags << dec;

  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    const char *delim;

    log << ", num_planes=" << buf.length;

    delim = "";
    log << ", bytesused=[";
    for (unsigned int i = 0; i < buf.length; ++i) {
      log << delim << buf.m.planes[i].bytesused;
      delim = ", ";
    }
    log << "]";

    delim = "";
    log << ", length=[";
    for (unsigned int i = 0; i < buf.length; ++i) {
      log << delim << buf.m.planes[i].length;
      delim = ", ";
    }
    log << "]";

    delim = "";
    log << ", offset=[";
    for (unsigned int i = 0; i < buf.length; ++i) {
      log << delim << buf.m.planes[i].data_offset;
      delim = ", ";
    }
    log << "]";
  } else {
    log << ", bytesused=" << buf.bytesused << ", length=" << buf.length;
  }

  log << endl;
}

void Codec::streamon() {
  input.streamon();
  output.streamon();
}

void Codec::Port::streamon() {
  log << "Stream on " << dec << type << endl;

  int ret = ioctl(fd, VIDIOC_STREAMON, &type);
  if (ret != 0) {
    throw Exception("Failed to stream on.");
  }
}

void Codec::streamoff() {
  input.streamoff();
  output.streamoff();
}

void Codec::Port::streamoff() {
  log << "Stream off " << dec << type << endl;

  int ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
  if (ret != 0) {
    throw Exception("Failed to stream off.");
  }
}

void Codec::Port::sendEncStopCommand() {
  v4l2_encoder_cmd cmd = {.cmd = V4L2_ENC_CMD_STOP};

  if (tryEncStop) {
    if (0 != ioctl(fd, VIDIOC_TRY_ENCODER_CMD, &cmd)) {
      throw Exception("Failed to send try encoder stop command.");
    }
  }

  if (0 != ioctl(fd, VIDIOC_ENCODER_CMD, &cmd)) {
    throw Exception("Failed to send encoding stop command.");
  }
}

void Codec::Port::sendDecStopCommand() {
  v4l2_decoder_cmd cmd = {.cmd = V4L2_DEC_CMD_STOP};

  if (tryDecStop) {
    if (0 != ioctl(fd, VIDIOC_TRY_DECODER_CMD, &cmd)) {
      throw Exception("Failed to send try decoder stop command.");
    }
  }

  if (0 != ioctl(fd, VIDIOC_DECODER_CMD, &cmd)) {
    throw Exception("Failed to send decoding stop command.");
  }
}

void Codec::Port::setH264DecIntBufSize(uint32_t ibs) {
  log << "setH264DecIntBufSize( " << ibs << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_INTBUF_SIZE;
  control.value = ibs;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set H264 ibs=%u.", ibs);
  }
}

void Codec::Port::setDecFrameReOrdering(uint32_t fro) {
  log << "setDecFrameReOrdering( " << fro << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_FRAME_REORDERING;
  control.value = fro;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set decoding fro=%u.", fro);
  }
}

void Codec::Port::setDecIgnoreStreamHeaders(uint32_t ish) {
  log << "setDecIgnoreStreamHeaders( " << ish << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_IGNORE_STREAM_HEADERS;
  control.value = ish;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set decoding ish=%u.", ish);
  }
}

void Codec::Port::setNALU(NaluFormat nalu) {
  log << "Set NALU " << nalu << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_NALU_FORMAT;
  control.value = nalu;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set NALU. nalu=%u.", nalu);
  }
}

void Codec::Port::setEncFramerate(uint32_t frame_rate) {
  log << "setEncFramerate( " << frame_rate << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_FRAME_RATE;
  control.value = frame_rate;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set frame_rate=%u.", frame_rate);
  }
}

void Codec::Port::setEncBitrate(uint32_t bit_rate) {
  log << "setEncBitrate( " << bit_rate << " )" << endl;
  log << "setRctype( " << rc_type << " )" << endl;
  if (bit_rate == 0 && rc_type == 0) {
    return;
  }
  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_BITRATE;
  control.value = bit_rate;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set bit_rate=%u.", bit_rate);
  }
}

void Codec::Port::setRateControl(struct v4l2_rate_control *rc) {
  log << "setRateControl( " << rc->rc_type << ",";
  log << rc->target_bitrate << "," << rc->maximum_bitrate << ")" << endl;

  int ret = ioctl(fd, VIDIOC_S_MVX_RATE_CONTROL, rc);
  if (ret != 0) {
    throw Exception("Failed to set rate control.");
  }

  return;
}

void Codec::Port::setEncPFrames(uint32_t pframes) {
  log << "setEncPFrames( " << pframes << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_P_FRAMES;
  control.value = pframes;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set pframes=%u.", pframes);
  }
}

void Codec::Port::setEncBFrames(uint32_t bframes) {
  log << "setEncBFrames( " << bframes << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_B_FRAMES;
  control.value = bframes;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set bframes=%u.", bframes);
  }
}

void Codec::Port::setEncSliceSpacing(uint32_t spacing) {
  log << "setEncSliceSpacing( " << spacing << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB;
  control.value = spacing;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set slice spacing=%u.", spacing);
  }

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE;
  control.value = spacing != 0;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set slice mode.");
  }
}

void Codec::Port::setH264EncForceChroma(uint32_t fmt) {
  log << "setH264EncForceChroma( " << fmt << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_FORCE_CHROMA_FORMAT;
  control.value = fmt;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set H264 chroma fmt=%u.", fmt);
  }
}

void Codec::Port::setH264EncBitdepth(uint32_t bd) {
  log << "setH264EncBitdepth( " << bd << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_BITDEPTH_LUMA;
  control.value = bd;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set H264 luma bd=%u.", bd);
  }

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_BITDEPTH_CHROMA;
  control.value = bd;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set H264 chroma bd=%u.", bd);
  }
}

void Codec::Port::setH264EncIntraMBRefresh(uint32_t period) {
  log << "setH264EncIntraMBRefresh( " << period << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB;
  control.value = period;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set H264 period=%u.", period);
  }
}

void Codec::Port::setEncProfile(uint32_t profile) {
  log << "setEncProfile( " << profile << " )" << endl;

  bool setProfile = false;
  struct v4l2_control control;

  memset(&control, 0, sizeof(control));

  if (io->getFormat() == to4cc("h264")) {
    setProfile = true;
    control.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE;
    control.value = profile;
  } else if (io->getFormat() == to4cc("hevc")) {
    setProfile = true;
    control.id = V4L2_CID_MVE_VIDEO_H265_PROFILE;
    control.value = profile;
  }

  if (setProfile) {
    if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
      throw Exception("Failed to set profile=%u for fmt: %u .", profile,
                      io->getFormat());
    }
  } else {
    log << "Profile cannot be set for this codec" << endl;
  }
}

void Codec::Port::setEncLevel(uint32_t level) {
  log << "setEncLevel( " << level << " )" << endl;

  bool setLevel = false;
  struct v4l2_control control;

  memset(&control, 0, sizeof(control));

  if (io->getFormat() == to4cc("h264")) {
    setLevel = true;
    control.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL;
    control.value = level;
  } else if (io->getFormat() == to4cc("hevc")) {
    setLevel = true;
    control.id = V4L2_CID_MVE_VIDEO_H265_LEVEL;
    control.value = level;
  }

  if (setLevel) {
    if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
      throw Exception("Failed to set level=%u for fmt: %u .", level,
                      io->getFormat());
    }
  } else {
    log << "Level cannot be set for this codec" << endl;
  }
}

void Codec::Port::setEncConstrainedIntraPred(uint32_t cip) {
  log << "setEncConstrainedIntraPred( " << cip << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_CONSTR_IPRED;
  control.value = cip;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set encoding cip=%u.", cip);
  }
}

void Codec::Port::setH264EncEntropyMode(uint32_t ecm) {
  log << "setH264EncEntropyMode( " << ecm << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE;
  control.value = ecm;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set H264 ecm=%u.", ecm);
  }
}

void Codec::Port::setH264EncGOPType(uint32_t gop) {
  log << "setH264EncGOPType( " << gop << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_GOP_TYPE;
  control.value = gop;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set H264 gop=%u.", gop);
  }
}

void Codec::Port::setH264EncMinQP(uint32_t minqp) {
  log << "setH264EncMinQP( " << minqp << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_H264_MIN_QP;
  control.value = minqp;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set H264 minqp=%u.", minqp);
  }
}

void Codec::Port::setH264EncMaxQP(uint32_t maxqp) {
  log << "setH264EncMaxQP( " << maxqp << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE;
  control.value = 1;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to enable/disable rate control.");
  }

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_H264_MAX_QP;
  control.value = maxqp;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set H264 maxqp=%u.", maxqp);
  }
}

void Codec::Port::setH264EncFixedQP(uint32_t fqp) {
  log << "setH264EncFixedQP( " << fqp << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP;
  control.value = fqp;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set H264 I frame fqp=%u.", fqp);
  }

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP;
  control.value = fqp;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set H264 P frame fqp=%u.", fqp);
  }

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP;
  control.value = fqp;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set H264 B frame fqp=%u.", fqp);
  }

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE;
  control.value = 0;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to enable/disable rate control.");
  }
}

void Codec::Port::setH264EncFixedQPI(uint32_t fqp) {
  log << "setH264EncFixedQPI( " << fqp << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP;
  control.value = fqp;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set H264 I frame fqp=%u.", fqp);
  }
}

void Codec::Port::setH264EncFixedQPP(uint32_t fqp) {
  log << "setH264EncFixedQPP( " << fqp << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP;
  control.value = fqp;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set H264 P frame fqp=%u.", fqp);
  }
}

void Codec::Port::setH264EncFixedQPB(uint32_t fqp) {
  log << "setH264EncFixedQPB( " << fqp << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP;
  control.value = fqp;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set H264 B frame fqp=%u.", fqp);
  }
}

void Codec::Port::setH264EncBandwidth(uint32_t bw) {
  log << "setH264EncBandwidth( " << bw << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_BANDWIDTH_LIMIT;
  control.value = bw;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set H264 bw=%u.", bw);
  }
}

void Codec::Port::setHEVCEncEntropySync(uint32_t es) {
  log << "setHEVCEncEntropySync( " << es << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_ENTROPY_SYNC;
  control.value = es;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set HEVC es=%u.", es);
  }
}

void Codec::Port::setHEVCEncTemporalMVP(uint32_t tmvp) {
  log << "setHEVCEncTemporalMVP( " << tmvp << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_TEMPORAL_MVP;
  control.value = tmvp;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set HEVC tmvp=%u.", tmvp);
  }
}

void Codec::Port::setEncStreamEscaping(uint32_t sesc) {
  log << "setEncStreamEscaping( " << sesc << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_STREAM_ESCAPING;
  control.value = sesc;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set encoding sesc=%u.", sesc);
  }
}

void Codec::Port::setEncHorizontalMVSearchRange(uint32_t hmvsr) {
  log << "setEncHorizontalMVSearchRange( " << hmvsr << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_MV_H_SEARCH_RANGE;
  control.value = hmvsr;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set encoding hmvsr=%u.", hmvsr);
  }
}

void Codec::Port::setEncVerticalMVSearchRange(uint32_t vmvsr) {
  log << "setEncVerticalMVSearchRange( " << vmvsr << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_VIDEO_MV_V_SEARCH_RANGE;
  control.value = vmvsr;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set encoding vmvsr=%u.", vmvsr);
  }
}

void Codec::Port::setVP9EncTileCR(uint32_t tcr) {
  log << "setVP9EncTileCR( " << tcr << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_TILE_COLS;
  control.value = tcr;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set VP9 tile cols=%u.", tcr);
  }

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_TILE_ROWS;
  control.value = tcr;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set VP9 tile rows=%u.", tcr);
  }
}

void Codec::Port::setJPEGEncRefreshInterval(uint32_t r) {
  log << "setJPEGEncRefreshInterval( " << r << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_JPEG_RESTART_INTERVAL;
  control.value = r;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set JPEG refresh interval=%u.", r);
  }
}

void Codec::Port::setJPEGEncQuality(uint32_t q) {
  log << "setJPEGEncQuality( " << q << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
  control.value = q;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set JPEG compression quality=%u.", q);
  }
}

void Codec::Port::setRotation(int rotation) { this->rotation = rotation; }

void Codec::Port::setMirror(int mirror) { this->mirror = mirror; }

void Codec::Port::setDownScale(int scale) { this->scale = scale; }

void Codec::Port::setDSLFrame(int width, int height) {
  log << "setDSLFrame( " << width << " ," << height << ")" << endl;

  struct v4l2_mvx_dsl_frame dsl_frame;
  memset(&dsl_frame, 0, sizeof(dsl_frame));
  dsl_frame.width = width;
  dsl_frame.height = height;
  int ret = ioctl(fd, VIDIOC_S_MVX_DSL_FRAME, &dsl_frame);
  if (ret != 0) {
    throw Exception("Failed to set DSL frame width/height.");
  }

  return;
}

void Codec::Port::setDSLRatio(int hor, int ver) {
  log << "setDSLRatio( " << hor << " ," << ver << ")" << endl;

  struct v4l2_mvx_dsl_ratio dsl_ratio;
  memset(&dsl_ratio, 0, sizeof(dsl_ratio));
  dsl_ratio.hor = hor;
  dsl_ratio.ver = ver;
  int ret = ioctl(fd, VIDIOC_S_MVX_DSL_RATIO, &dsl_ratio);
  if (ret != 0) {
    throw Exception("Failed to set DSL frame hor/ver.");
  }

  return;
}

void Codec::Port::setDSLMode(int mode) {
  log << "setDSLMode(" << mode << ")" << endl;
  int dsl_pos_mode = mode;
  int ret = ioctl(fd, VIDIOC_S_MVX_DSL_MODE, &dsl_pos_mode);
  if (ret != 0) {
    throw Exception("Failed to set dsl mode.");
  }
}

void Codec::Port::setLongTermRef(uint32_t mode, uint32_t period) {
  log << "setLongTermRef( " << mode << " ," << period << ")" << endl;
  struct v4l2_mvx_long_term_ref ltr;
  memset(&ltr, 0, sizeof(ltr));
  ltr.mode = mode;
  ltr.period = period;
  int ret = ioctl(fd, VIDIOC_S_MVX_LONG_TERM_REF, &ltr);
  if (ret != 0) {
    throw Exception("Failed to set long term mode/period.");
  }
}

void Codec::Port::setFWTimeout(int timeout) {
  log << "setFWTimeout( " << timeout << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_WATCHDOG_TIMEOUT;
  control.value = timeout;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set firmware timeout=%u.", timeout);
  }
}

void Codec::Port::setProfiling(int enable) {
  log << "setProfiling( " << enable << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_PROFILING;
  control.value = enable;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set profiling=%u.", enable);
  }
}

void Codec::Port::setFrameCount(int frames) { this->frames_count = frames; }

void Codec::Port::setFramerate(int framerate) {
  this->fps = framerate;
  if (framerate != 0) {
    this->intervalTime = 1 * 1000 * 1000 / framerate;  // us
  } else {
    this->intervalTime = 0;
  }
}

void Codec::Port::setCropLeft(int left) {
  log << "setCropLeft( " << left << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_CROP_LEFT;
  control.value = left;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set crop left=%u.", left);
  }
}

void Codec::Port::setCropRight(int right) {
  log << "setCropRight( " << right << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_CROP_RIGHT;
  control.value = right;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set crop right=%u.", right);
  }
}

void Codec::Port::setCropTop(int top) {
  log << "setCropTop( " << top << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_CROP_TOP;
  control.value = top;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set crop top=%u.", top);
  }
}

void Codec::Port::setCropBottom(int bottom) {
  log << "setCropBottom( " << bottom << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_CROP_BOTTOM;
  control.value = bottom;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set crop bottom=%u.", bottom);
  }
}

void Codec::Port::setVuiColourDesc(struct v4l2_mvx_color_desc *color) {
  log << "setVuiColourDesc( " << color->content.luminance_average << ",";
  log << color->content.luminance_max << ")" << endl;

  int ret = ioctl(fd, VIDIOC_S_MVX_COLORDESC, color);
  if (ret != 0) {
    throw Exception("Failed to set color description.");
  }

  return;
}

void Codec::Port::setSeiUserData(struct v4l2_sei_user_data *sei_user_data) {
  log << "setSeiUserData( " << sei_user_data->user_data << ")" << endl;

  int ret = ioctl(fd, VIDIOC_S_MVX_SEI_USERDATA, sei_user_data);
  if (ret != 0) {
    throw Exception("Failed to set color description.");
  }

  return;
}

void Codec::Port::setHRDBufferSize(int size) {
  log << "setHRDBufferSize( " << size << " )" << endl;

  struct v4l2_control control;

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MVE_VIDEO_HRD_BUFFER_SIZE;
  control.value = size;

  if (-1 == ioctl(fd, VIDIOC_S_CTRL, &control)) {
    throw Exception("Failed to set crop bottom=%u.", size);
  }
}

void Codec::Port::notifySourceChange() { isSourceChange = true; }

void Codec::runPoll() {
  bool eos = false;
  struct timeval timestart, timeend;
  uint64_t frames_processed = 0;

  while (!eos) {
    struct pollfd p = {.fd = fd, .events = POLLPRI};

    if (input.pending > 0) {
      p.events |= POLLOUT;
    }

    if (output.pending > 0) {
      p.events |= POLLIN;
    }

    int ret = poll(&p, 1, 1200000);

    if (ret < 0) {
      throw Exception("Poll returned error code.");
    }

    if (p.revents & POLLERR) {
      throw Exception("Poll returned error event.");
    }

    if (ret == 0) {
      throw Exception("Poll timed out.");
    }

    if (p.revents & POLLOUT) {
      input.handleBuffer();
    }
    if (p.revents & POLLIN) {
      if (csweo) {
        log << "Changing settings while encoding." << endl;
        if (fps != 0) {
          output.setEncFramerate(fps);
          fps = 0;
        }
        if (bps != 0) {
          /* output.setEncBitrate(bps); */
          output.setEncBitrate(0); /* only for coverage */
          bps = 0;
        }
        /* Set maxQP before minQP, otherwise FW rejects */
        if (maxqp != 0) {
          output.setH264EncMaxQP(maxqp);
          maxqp = 0;
        }
        if (minqp != 0) {
          output.setH264EncMinQP(minqp);
          minqp = 0;
        }
        if (fixedqp != 0) {
          output.setH264EncFixedQP(fixedqp);
          fixedqp = 0;
        }

        csweo = false;
      }

      eos = output.handleBuffer();
      checkOutputTimestamp(output.io->getCurTimestamp());
      output.io->resetCurTimestamp();

      if (timestart_us == 0) {
        if (input.type == V4L2_BUF_TYPE_VIDEO_OUTPUT &&
            getOutputFramesProcessed() >= 1) {
          gettimeofday(&timestart, NULL);
          timestart_us = timestart.tv_sec * 1000000ll + timestart.tv_usec;
          printf("-----Decoder. set timestart_us: %lu us---------\n",
                 timestart_us);
        } else if (input.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE &&
                   getInputFramesProcessed() >= 1) {
          gettimeofday(&timestart, NULL);
          timestart_us = timestart.tv_sec * 1000000ll + timestart.tv_usec;
          printf("-----Encoder. set timestart_us: %lu us---------\n",
                 timestart_us);
        }
      }
    }
    if (p.revents & POLLPRI) {
      handleEvent();
    }
  }

  gettimeofday(&timeend, NULL);
  timeend_us = timeend.tv_sec * 1000000ll + timeend.tv_usec;

  if (input.type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
    frames_processed = getOutputFramesProcessed() - 1;
    avgfps = (((double)(frames_processed * 1000.0 * 1000.0)) /
              (double)(timeend_us - timestart_us));
    printf(
        "-----[Test Result] MVX Decode Done. frames_processed: %lu, cost time: "
        "%lu us.\n",
        frames_processed, timeend_us - timestart_us);
  } else if (input.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
    frames_processed = getInputFramesProcessed() - 1;
    avgfps = (((double)(frames_processed * 1000.0 * 1000.0)) /
              (double)(timeend_us - timestart_us));
    printf(
        "-----[Test Result] MVX Encode Done. frames_processed: %lu, cost time: "
        "%lu us.\n",
        frames_processed, timeend_us - timestart_us);
  }
}

void Codec::checkOutputTimestamp(uint64_t timestamp) {
  if (timestamp == 0) return;

  std::set<uint64_t> timestampList = input.io->timestampList;
  std::set<uint64_t>::iterator it = timestampList.find(timestamp);
  if (it != timestampList.end()) {
    timestampList.erase(it);
  } else {
    cerr << "Incorrect timestamp: " << timestamp
         << ", don't find it in input timestamp list." << endl;
  }
}

void Codec::runThreads() {
  int ret;
  void *retval;

  ret = pthread_create(&input.tid, NULL, runThreadInput, this);
  if (ret != 0) {
    throw Exception("Failed to create input thread.");
  }

  ret = pthread_create(&output.tid, NULL, runThreadOutput, this);
  if (ret != 0) {
    throw Exception("Failed to create output thread.");
  }

  pthread_join(input.tid, &retval);
  pthread_join(output.tid, &retval);
}

void *Codec::runThreadInput(void *arg) {
  Codec *_this = static_cast<Codec *>(arg);
  bool eos = false;

  while (!eos) eos = _this->input.handleBuffer();

  return NULL;
}

void *Codec::runThreadOutput(void *arg) {
  Codec *_this = static_cast<Codec *>(arg);
  bool eos = false;

  while (!eos) eos = _this->output.handleBuffer();

  return NULL;
}

bool Codec::Port::handleBuffer() {
  Buffer &buffer = dequeueBuffer();
  io->finalize(buffer);
  v4l2_buffer &b = buffer.getBuffer();
  if (io->eof()) {
    if (tryDecStop) {
      sendDecStopCommand();
    }
    return true;
  }

  /* EOS on capture port. */
  if (!V4L2_TYPE_IS_OUTPUT(b.type) && b.flags & V4L2_BUF_FLAG_LAST) {
    if (io->getDir() == 1 && V4L2_TYPE_IS_MULTIPLANAR(b.type) &&
        (b.flags & V4L2_BUF_FLAG_MVX_BUFFER_FRAME_PRESENT) ==
            V4L2_BUF_FLAG_MVX_BUFFER_FRAME_PRESENT) {
      frames_processed++;
      // printf("-------------dequeueBuffer yuv frames: %d-------------\n",
      // frames_processed);
    }
    log << "Capture EOS." << endl;
    return true;
  }

  /* input source change. we should only handle this on decode
   * output:V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE*/
  if (!V4L2_TYPE_IS_OUTPUT(b.type) && V4L2_TYPE_IS_MULTIPLANAR(b.type) &&
      (getBytesUsed(b) == 0 ||
       (b.flags & V4L2_BUF_FLAG_MVX_BUFFER_FRAME_PRESENT) !=
           V4L2_BUF_FLAG_MVX_BUFFER_FRAME_PRESENT) &&
      (b.flags & V4L2_BUF_FLAG_ERROR) == 0) {
    if (isSourceChange) {
      log << "source changed. should reset output stream." << endl;
      handleResolutionChange();
      isSourceChange = false;
      return false;
    }
  }

  /* Remove vendor custom flags. */
  // decoder specfied frames count to be processed
  if (io->getDir() == 1 && V4L2_TYPE_IS_MULTIPLANAR(b.type) &&
      (b.flags & V4L2_BUF_FLAG_MVX_BUFFER_FRAME_PRESENT) ==
          V4L2_BUF_FLAG_MVX_BUFFER_FRAME_PRESENT) {
    frames_processed++;
    // printf("-------------dequeueBuffer yuv frames: %d-------------\n",
    // frames_processed);
  }

  buffer.resetVendorFlags();
  if (io->getDir() == 1 && frames_count > 0 &&
      frames_processed >= frames_count) {
    buffer.clearBytesUsed();
    buffer.setEndOfStream(true);
    queueBuffer(buffer);
    return true;
  } else {
    io->prepare(buffer);
    buffer.setEndOfStream(io->eof());
  }

  if (fps > 0 && V4L2_TYPE_IS_MULTIPLANAR(b.type)) {
    controlFramerate();
  }

  queueBuffer(buffer);

  if (io->eof()) {
    sendEncStopCommand();
  }

  return false;
}

void Codec::Port::handleResolutionChange() {
  streamoff();
  allocateBuffers(0);
  getTrySetFormat();
  allocateBuffers(getBufferCount());
  // queueBuffers();
  streamon();
  queueBuffers();
}

bool Codec::handleEvent() {
  struct v4l2_event event;
  int ret;

  ret = ioctl(fd, VIDIOC_DQEVENT, &event);
  if (ret != 0) {
    throw Exception("Failed to dequeue event.");
  }

  log << "Event. type=" << event.type << "." << endl;

  if (event.type == V4L2_EVENT_MVX_COLOR_DESC) {
    v4l2_mvx_color_desc color = getColorDesc();
    printColorDesc(color);
  }

  if (event.type == V4L2_EVENT_SOURCE_CHANGE &&
      (event.u.src_change.changes & V4L2_EVENT_SRC_CH_RESOLUTION)) {
    output.notifySourceChange();
  }

  if (event.type == V4L2_EVENT_EOS) {
    return true;
  }

  return false;
}

void Codec::getStride(uint32_t format, size_t &nplanes, size_t stride[3][2]) {
  switch (format) {
    case V4L2_PIX_FMT_YUV420M:
      nplanes = 3;
      stride[0][0] = 4;
      stride[0][1] = 4;
      stride[1][0] = 2;
      stride[1][1] = 2;
      stride[2][0] = 2;
      stride[2][1] = 2;
      break;
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV21:
      nplanes = 2;
      stride[0][0] = 4;
      stride[0][1] = 4;
      stride[1][0] = 4;
      stride[1][1] = 2;
      stride[2][0] = 0;
      stride[2][1] = 0;
      break;
    case V4L2_PIX_FMT_P010:
      nplanes = 2;
      stride[0][0] = 8;
      stride[0][1] = 4;
      stride[1][0] = 8;
      stride[1][1] = 2;
      stride[2][0] = 0;
      stride[2][1] = 0;
      break;
    case V4L2_PIX_FMT_Y0L2:
      nplanes = 1;
      stride[0][0] = 16;
      stride[0][1] = 2;
      stride[1][0] = 0;
      stride[1][1] = 0;
      stride[2][0] = 0;
      stride[2][1] = 0;
      break;
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_UYVY:
      nplanes = 1;
      stride[0][0] = 8;
      stride[0][1] = 4;
      stride[1][0] = 0;
      stride[1][1] = 0;
      stride[2][0] = 0;
      stride[2][1] = 0;
      break;
    case V4L2_PIX_FMT_Y210:
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_RGBA8888:
    case DRM_FORMAT_BGRA8888:
      nplanes = 1;
      stride[0][0] = 16;
      stride[0][1] = 4;
      stride[1][0] = 0;
      stride[1][1] = 0;
      stride[2][0] = 0;
      stride[2][1] = 0;
      break;
    default:
      throw Exception("Unsupported buffer format.");
  }
}

size_t Codec::getSize(uint32_t format, size_t width, size_t height,
                      size_t strideAlign, size_t &nplanes, size_t stride[3],
                      size_t size[3]) {
  size_t s[3][2];
  size_t frameSize = 0;

  getStride(format, nplanes, s);

  for (int i = 0; i < 3; ++i) {
    stride[i] = roundUp(divRoundUp(width * s[i][0], 4), strideAlign);
    size[i] = divRoundUp(height * stride[i] * s[i][1], 4);
    frameSize += size[i];
  }

  return frameSize;
}

Decoder::Decoder(const char *dev, Input &input, Output &output, bool nonblock,
                 ostream &log)
    : Codec(dev, input, V4L2_BUF_TYPE_VIDEO_OUTPUT, output,
            V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, log, nonblock) {
  naluFmt = 0;
}

void Decoder::setH264IntBufSize(uint32_t ibs) {
  output.setH264DecIntBufSize(ibs);
}

void Decoder::setInterlaced(bool interlaced) {
  output.setInterlaced(interlaced);
}

void Decoder::setFrameReOrdering(uint32_t fro) {
  output.setDecFrameReOrdering(fro);
}

void Decoder::setIgnoreStreamHeaders(uint32_t ish) {
  output.setDecIgnoreStreamHeaders(ish);
}

void Decoder::tryStopCmd(bool tryStop) { input.tryDecStopCmd(tryStop); }

void Decoder::setNaluFormat(int nalu) {
  input.io->setNaluFormat(nalu);
  naluFmt = nalu;
}

void Decoder::setRotation(int rotation) { output.setRotation(rotation); }

void Decoder::setDownScale(int scale) { output.setDownScale(scale); }

void Decoder::setFrameCount(int frames) { output.setFrameCount(frames); }

void Decoder::setFramerate(int fps) {
  output.setEncFramerate(fps << 16);
  output.setFramerate(fps);
}

void Decoder::setDSLFrame(int width, int height) {
  output.setDSLFrame(width, height);
}

void Decoder::setDSLRatio(int hor, int ver) { output.setDSLRatio(hor, ver); }

void Decoder::setDSLMode(int mode) { output.setDSLMode(mode); }

void Decoder::setFWTimeout(int timeout) { output.setFWTimeout(timeout); }

void Decoder::setProfiling(int enable) { output.setProfiling(enable); }

Encoder::Encoder(const char *dev, Input &input, Output &output, bool nonblock,
                 ostream &log)
    : Codec(dev, input, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, output,
            V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, log, nonblock) {
  this->output.setEncFramerate(30 << 16);
  // this->output.setEncBitrate(input.getWidth() * input.getHeight() * 30 / 2);
}

void Encoder::changeSWEO(uint32_t csweo) { this->csweo = (csweo == 1); }

void Encoder::setFramerate(uint32_t fps) {
  if (!csweo) {
    output.setEncFramerate(fps << 16);
  } else {
    this->fps = fps << 16;
  }
  input.setFramerate(fps);
}

void Encoder::setBitrate(uint32_t bps) {
  if (!csweo) {
    output.setEncBitrate(bps);
  } else {
    output.setEncBitrate(bps);
    this->bps = bps - 500;
  }
}

void Encoder::setPFrames(uint32_t pframes) { output.setEncPFrames(pframes); }

void Encoder::setBFrames(uint32_t bframes) { output.setEncBFrames(bframes); }

void Encoder::setSliceSpacing(uint32_t spacing) {
  output.setEncSliceSpacing(spacing);
}

void Encoder::setHorizontalMVSearchRange(uint32_t hmvsr) {
  output.setEncHorizontalMVSearchRange(hmvsr);
}

void Encoder::setVerticalMVSearchRange(uint32_t vmvsr) {
  output.setEncVerticalMVSearchRange(vmvsr);
}

void Encoder::setH264ForceChroma(uint32_t fmt) {
  output.setH264EncForceChroma(fmt);
}

void Encoder::setH264Bitdepth(uint32_t bd) { output.setH264EncBitdepth(bd); }

void Encoder::setH264IntraMBRefresh(uint32_t period) {
  output.setH264EncIntraMBRefresh(period);
}

void Encoder::setProfile(uint32_t profile) { output.setEncProfile(profile); }

void Encoder::setLevel(uint32_t level) { output.setEncLevel(level); }

void Encoder::setConstrainedIntraPred(uint32_t cip) {
  output.setEncConstrainedIntraPred(cip);
}

void Encoder::setH264EntropyCodingMode(uint32_t ecm) {
  output.setH264EncEntropyMode(ecm);
}

void Encoder::setH264GOPType(uint32_t gop) { output.setH264EncGOPType(gop); }

void Encoder::setH264MinQP(uint32_t minqp) {
  if (!csweo) {
    output.setH264EncMinQP(minqp);
  } else {
    this->minqp = minqp;
  }
}

void Encoder::setH264MaxQP(uint32_t maxqp) {
  if (!csweo) {
    output.setH264EncMaxQP(maxqp);
  } else {
    this->maxqp = maxqp;
  }
}

void Encoder::setH264FixedQP(uint32_t fqp) {
  if (!csweo) {
    output.setH264EncFixedQP(fqp);
  } else {
    output.setH264EncFixedQP(fqp);
    this->fixedqp = fqp + 2;
  }
}

void Encoder::setH264FixedQPI(uint32_t fqp) { output.setH264EncFixedQPI(fqp); }

void Encoder::setH264FixedQPP(uint32_t fqp) { output.setH264EncFixedQPP(fqp); }

void Encoder::setH264FixedQPB(uint32_t fqp) { output.setH264EncFixedQPB(fqp); }

void Encoder::setH264Bandwidth(uint32_t bw) { output.setH264EncBandwidth(bw); }

void Encoder::setVP9TileCR(uint32_t tcr) { output.setVP9EncTileCR(tcr); }

void Encoder::setJPEGRefreshInterval(uint32_t r) {
  output.setJPEGEncRefreshInterval(r);
}

void Encoder::setJPEGQuality(uint32_t q) { output.setJPEGEncQuality(q); }

void Encoder::setHEVCEntropySync(uint32_t es) {
  output.setHEVCEncEntropySync(es);
}

void Encoder::setHEVCTemporalMVP(uint32_t tmvp) {
  output.setHEVCEncTemporalMVP(tmvp);
}

void Encoder::setStreamEscaping(uint32_t sesc) {
  output.setEncStreamEscaping(sesc);
}

void Encoder::tryStopCmd(bool tryStop) { input.tryEncStopCmd(tryStop); }

void Encoder::setMirror(int mirror) { input.setMirror(mirror); }

void Encoder::setFrameCount(int frames) { input.setFrameCount(frames); }

void Encoder::setRateControl(const std::string &rc, int target_bitrate,
                             int maximum_bitrate) {
  struct v4l2_rate_control v4l2_rc;
  memset(&v4l2_rc, 0, sizeof(v4l2_rc));
  if (rc.compare("standard") == 0) {
    v4l2_rc.rc_type = V4L2_OPT_RATE_CONTROL_MODE_STANDARD;
  } else if (rc.compare("constant") == 0) {
    v4l2_rc.rc_type = V4L2_OPT_RATE_CONTROL_MODE_CONSTANT;
  } else if (rc.compare("variable") == 0) {
    v4l2_rc.rc_type = V4L2_OPT_RATE_CONTROL_MODE_VARIABLE;
  } else if (rc.compare("cvbr") == 0) {
    v4l2_rc.rc_type = V4L2_OPT_RATE_CONTROL_MODE_C_VARIABLE;
  } else if (rc.compare("off") == 0) {
    v4l2_rc.rc_type = V4L2_OPT_RATE_CONTROL_MODE_OFF;
  } else {
    v4l2_rc.rc_type = V4L2_OPT_RATE_CONTROL_MODE_OFF;
  }
  if (v4l2_rc.rc_type) {
    v4l2_rc.target_bitrate = target_bitrate;
  }
  if (v4l2_rc.rc_type == V4L2_OPT_RATE_CONTROL_MODE_C_VARIABLE) {
    v4l2_rc.maximum_bitrate = maximum_bitrate;
  }
  output.setRateControl(&v4l2_rc);
}

void Encoder::setCropLeft(int left) { input.setCropLeft(left); }

void Encoder::setCropRight(int right) { input.setCropRight(right); }

void Encoder::setCropTop(int top) { input.setCropTop(top); }

void Encoder::setCropBottom(int bottom) { input.setCropBottom(bottom); }

void Encoder::setVuiColourDesc(struct v4l2_mvx_color_desc *color) {
  input.setVuiColourDesc(color);
}

void Encoder::setSeiUserData(struct v4l2_sei_user_data *sei_user_data) {
  input.setSeiUserData(sei_user_data);
}

void Encoder::setHRDBufferSize(int size) { input.setHRDBufferSize(size); }

void Encoder::setLongTermRef(uint32_t mode, uint32_t period) {
  input.setLongTermRef(mode, period);
}

void Encoder::setFWTimeout(int timeout) { input.setFWTimeout(timeout); }

void Encoder::setProfiling(int enable) { input.setProfiling(enable); }

Info::Info(const char *dev, ostream &log)
    : Codec(dev, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_BUF_TYPE_VIDEO_CAPTURE,
            log, true) {}

void Info::enumerateFormats() {
  try {
    Codec::enumerateFormats();
  } catch (Exception &e) {
    cerr << "Error: " << e.what() << endl;
  }
}
