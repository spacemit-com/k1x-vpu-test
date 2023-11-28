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

#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

using namespace std;

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

/*****************************************************************************
 * Buffer user pointer
 *****************************************************************************/

class BufferUserPtr {
 public:
  void queryBuffer(int fd, v4l2_buf_type type, uint32_t index);
  void resize();

  void clear();
  void queue(int fd);
  void dequeue(v4l2_buffer &b);

  static void printBuffer(const v4l2_buffer &buf, const char *prefix);

  v4l2_buffer vbuf;
  v4l2_plane vplanes[VIDEO_MAX_PLANES];
  vector<char> userptr[VIDEO_MAX_PLANES];
};

void BufferUserPtr::queryBuffer(int fd, v4l2_buf_type type, uint32_t index) {
  vbuf.type = type;
  vbuf.index = index;
  vbuf.length = VIDEO_MAX_PLANES;
  vbuf.m.planes = vplanes;

  int ret = ioctl(fd, VIDIOC_QUERYBUF, &vbuf);
  if (ret != 0) {
    throw Exception("Failed to query buffer.");
  }

  printBuffer(vbuf, "Query");
}

void BufferUserPtr::resize() {
  for (unsigned int i = 0; i < VIDEO_MAX_PLANES; ++i) {
    userptr[i].clear();
  }

  if (V4L2_TYPE_IS_MULTIPLANAR(vbuf.type)) {
    for (uint32_t i = 0; i < vbuf.length; ++i) {
      userptr[i].resize(vbuf.m.planes[i].length);
    }
  } else {
    userptr[0].resize(vbuf.length);
  }
}

void BufferUserPtr::clear() {
  if (V4L2_TYPE_IS_MULTIPLANAR(vbuf.type)) {
    for (unsigned int i = 0; i < min(vbuf.length, (uint32_t)VIDEO_MAX_PLANES);
         ++i) {
      v4l2_plane &plane = vbuf.m.planes[i];

      plane.bytesused = 0;
      plane.m.userptr = (unsigned long)&userptr[i][0];
      plane.data_offset = 0;
    }
  } else {
    vbuf.bytesused = 0;
    vbuf.m.userptr = (unsigned long)&userptr[0][0];
  }
}

void BufferUserPtr::queue(int fd) {
  printBuffer(vbuf, "->");

  int ret = ioctl(fd, VIDIOC_QBUF, &vbuf);
  if (ret != 0) {
    throw Exception("Failed to queue buffer.");
  }
}

void BufferUserPtr::dequeue(v4l2_buffer &b) {
  vbuf = b;

  if (V4L2_TYPE_IS_MULTIPLANAR(vbuf.type)) {
    vbuf.m.planes = vplanes;
    for (uint32_t i = 0; i < min(vbuf.length, (uint32_t)VIDEO_MAX_PLANES);
         ++i) {
      vbuf.m.planes[i] = b.m.planes[i];
    }
  }

  printBuffer(vbuf, "<-");
}

void BufferUserPtr::printBuffer(const v4l2_buffer &buf, const char *prefix) {
  cout << prefix << ": "
       << "type=" << buf.type << ", index=" << buf.index
       << ", sequence=" << buf.sequence << ", flags=" << hex << buf.flags
       << dec;

  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    const char *delim;

    cout << ", num_planes=" << buf.length;

    delim = "";
    cout << ", bytesused=[";
    for (unsigned int i = 0; i < buf.length; ++i) {
      cout << delim << buf.m.planes[i].bytesused;
      delim = ", ";
    }
    cout << "]";

    delim = "";
    cout << ", length=[";
    for (unsigned int i = 0; i < buf.length; ++i) {
      cout << delim << buf.m.planes[i].length;
      delim = ", ";
    }
    cout << "]";

    delim = "";
    cout << ", offset=[";
    for (unsigned int i = 0; i < buf.length; ++i) {
      cout << delim << buf.m.planes[i].data_offset;
      delim = ", ";
    }
    cout << "]";

    delim = "";
    cout << ", userptr=[";
    for (unsigned int i = 0; i < buf.length; ++i) {
      cout << delim << hex << buf.m.planes[i].m.userptr << dec;
      delim = ", ";
    }
    cout << "]";
  } else {
    cout << ", bytesused=" << buf.bytesused << ", length=" << buf.length;
  }

  cout << endl;
}

/*****************************************************************************
 * Port
 *****************************************************************************/

class Port {
 public:
  Port(int &fd, v4l2_buf_type type, const char *filename);

  void getSetFormat(uint32_t pixelformat, uint32_t sizeimage = 0);
  void requestBuffers(unsigned int count);
  unsigned int getBufferCount();
  void queueBuffer(BufferUserPtr &buf);
  BufferUserPtr &dequeueBuffer();
  void fillAndQueue(BufferUserPtr &buf);
  void clearAndQueue(BufferUserPtr &buf);
  void dumpBuffer(BufferUserPtr &buf);
  void streamOn();
  void streamOff();

  unsigned int pending;
  vector<BufferUserPtr> buffers;

 private:
  v4l2_format getFormat();
  void setFormat(v4l2_format &format);

  int &fd;
  v4l2_buf_type type;
  fstream file;
};

Port::Port(int &fd, v4l2_buf_type type, const char *filename)
    : pending(0), fd(fd), type(type) {
  ios::openmode flags = V4L2_TYPE_IS_OUTPUT(type) ? ios::in : ios::out;
  file.open(filename, flags);
}

void Port::getSetFormat(uint32_t pixelformat, uint32_t sizeimage) {
  v4l2_format format = getFormat();

  if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
    format.fmt.pix_mp.pixelformat = pixelformat;
  } else {
    format.fmt.pix.pixelformat = pixelformat;
    format.fmt.pix.sizeimage = sizeimage;
  }

  setFormat(format);
}

v4l2_format Port::getFormat() {
  v4l2_format format = {.type = type};
  int ret = ioctl(fd, VIDIOC_G_FMT, &format);
  if (ret != 0) {
    throw Exception("Failed to get format.");
  }

  return format;
}

void Port::setFormat(v4l2_format &format) {
  int ret = ioctl(fd, VIDIOC_S_FMT, &format);
  if (ret != 0) {
    throw Exception("Failed to set format.");
  }
}

void Port::requestBuffers(unsigned int count) {
  /* Request new buffer to be allocated. */
  v4l2_requestbuffers reqbuf;
  reqbuf.count = count;
  reqbuf.type = type;
  reqbuf.memory = V4L2_MEMORY_USERPTR;
  int ret = ioctl(fd, VIDIOC_REQBUFS, &reqbuf);
  if (ret != 0) {
    throw Exception("Failed to request buffers.");
  }

  buffers.resize(reqbuf.count);

  /* Query each buffer and create a new meta buffer. */
  for (uint32_t i = 0; i < reqbuf.count; ++i) {
    buffers[i].queryBuffer(fd, type, i);
    buffers[i].resize();
  }
}

unsigned int Port::getBufferCount() {
  v4l2_control control;
  control.id = V4L2_TYPE_IS_OUTPUT(type) ? V4L2_CID_MIN_BUFFERS_FOR_OUTPUT
                                         : V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;

  int ret = ioctl(fd, VIDIOC_G_CTRL, &control);
  if (ret != 0) {
    throw Exception("Failed to get minimum buffers.");
  }

  return control.value;
}

void Port::queueBuffer(BufferUserPtr &buf) {
  buf.queue(fd);
  ++pending;
}

BufferUserPtr &Port::dequeueBuffer() {
  v4l2_buffer vbuf;
  v4l2_plane planes[VIDEO_MAX_PLANES];

  vbuf.type = type;
  vbuf.m.planes = planes;
  vbuf.length = VIDEO_MAX_PLANES;

  int ret = ioctl(fd, VIDIOC_DQBUF, &vbuf);
  if (ret != 0) {
    throw Exception("Failed to dequeue buffer. type=%u, memory=%u", vbuf.type,
                    vbuf.memory);
  }

  --pending;

  BufferUserPtr &buf = buffers[vbuf.index];
  buf.dequeue(vbuf);

  return buf;
}

void Port::fillAndQueue(BufferUserPtr &buf) {
  buf.clear();
  file.read((char *)buf.vbuf.m.userptr, buf.vbuf.length);
  buf.vbuf.bytesused = file.gcount();

  if (file.eof()) {
    buf.vbuf.flags |= V4L2_BUF_FLAG_LAST;
  }

  if (buf.vbuf.bytesused > 0) {
    queueBuffer(buf);
  }
}

void Port::clearAndQueue(BufferUserPtr &buf) {
  buf.clear();
  queueBuffer(buf);
}

void Port::dumpBuffer(BufferUserPtr &buf) {
  for (uint32_t i = 0; i < buf.vbuf.length; ++i) {
    file.write((char *)buf.vbuf.m.planes[i].m.userptr,
               buf.vbuf.m.planes[i].bytesused);
  }
}

void Port::streamOn() {
  int ret = ioctl(fd, VIDIOC_STREAMON, &type);
  if (ret != 0) {
    throw Exception("Failed to stream on.");
  }
}

void Port::streamOff() {
  int ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
  if (ret != 0) {
    throw Exception("Failed to stream off.");
  }

  pending = 0;
}

/*****************************************************************************
 * Decoder
 *****************************************************************************/

class Decoder {
 public:
  Decoder(const char *input, const char *output);
  virtual ~Decoder();

  void run();

 private:
  void clearAndQueue();

  int fd;
  Port input;
  Port output;
};

Decoder::Decoder(const char *input, const char *output)
    : input(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, input),
      output(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, output) {
  fd = open("/dev/video0", O_RDWR);
  if (fd < 0) {
    throw Exception("Failed to open device.");
  }
}

Decoder::~Decoder() { close(fd); }

void Decoder::run() {
  /* Get and set format. */
  input.getSetFormat(V4L2_PIX_FMT_H264, 4 * 1024 * 1024);
  output.getSetFormat(V4L2_PIX_FMT_YUV420M);

  /* Request buffers. */
  input.requestBuffers(6);
  output.requestBuffers(6);

  /* Fill and queue input buffers. */
  for (size_t i = 0; i < input.buffers.size(); ++i) {
    input.fillAndQueue(input.buffers[i]);
  }

  clearAndQueue();

  /* Stream on. */
  input.streamOn();
  output.streamOn();

  /* Run dequeue loop. */
  while (true) {
    struct pollfd p = {.fd = fd, .events = POLLPRI};

    if (input.pending > 0) {
      p.events |= POLLOUT;
    }

    if (output.pending > 0) {
      p.events |= POLLIN;
    }

    int ret = poll(&p, 1, 60000);

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
      BufferUserPtr &buf = input.dequeueBuffer();
      input.fillAndQueue(buf);
    }

    if (p.revents & POLLIN) {
      BufferUserPtr &buf = output.dequeueBuffer();

      if (buf.vbuf.flags & V4L2_BUF_FLAG_LAST) {
        cout << "EOS" << endl;
        break;
      }

      if (buf.vbuf.m.planes[0].bytesused == 0) {
        cout << "Resolution changed." << endl;
        output.streamOff();
        output.requestBuffers(0);
        output.requestBuffers(output.getBufferCount());
        clearAndQueue();
        output.streamOn();
      } else {
        output.dumpBuffer(buf);
        output.clearAndQueue(buf);
      }
    }
  }
}

void Decoder::clearAndQueue() {
  /* Clear and queue output buffers. */
  for (size_t i = 0; i < output.buffers.size(); ++i) {
    output.clearAndQueue(output.buffers[i]);
  }
}

/*****************************************************************************
 * Main functions
 *****************************************************************************/

static void help(const char *exe) {
  cout << "Usage: " << exe << " <INPUT> <OUTPUT>" << endl;
  cout << endl;
  cout << "    INPUT   Input H264 stream." << endl;
  cout << "    OUTPUT  Output YUV420 stream." << endl;
}

int main(int argc, char **argv) {
  if (argc <= 2) {
    help(argv[0]);
    return 1;
  }

  Decoder decoder(argv[1], argv[2]);
  decoder.run();

  return 0;
}
