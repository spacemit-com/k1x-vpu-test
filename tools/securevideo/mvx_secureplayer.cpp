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
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>

#include "mvx-v4l2-controls.h"
#include "mvx_securevideo.hpp"

using namespace std;
using namespace MVXSecure;

/****************************************************************************
 * Classes
 ****************************************************************************/

class BufferV4L2 {
 public:
  BufferV4L2(const int &fd, const v4l2_buf_type type, const unsigned int index);

  const v4l2_buffer &getBuffer();
  unsigned int getNumPlanes() const;
  void checkPlanes(unsigned int plane) const;
  unsigned int getLength(unsigned int plane = 0) const;
  void setLength(const unsigned int length, const unsigned int plane = 0);
  unsigned int getBytesUsed(const unsigned int plane = 0) const;
  void setBytesUsed(const unsigned int bytesused, const unsigned int plane = 0);
  void setEOS();
  bool isQueued() const;
  void clear();
  void query();
  void queue();
  void dequeue(v4l2_buffer &buf);
  void print(const char *prefix);
  int getFd(const unsigned int plane = 0) const;
  void setFd(const int fd, const unsigned int plane = 0,
             const unsigned offset = 0);
  void *map(const unsigned int plane = 0);
  void unmap(void *p, const unsigned int plane = 0);

 private:
  const int &fd;
  v4l2_buffer buf;
  v4l2_plane planes[VIDEO_MAX_PLANES];
  bool queued;
};

class Port {
 public:
  typedef std::map<unsigned int, BufferV4L2 *> BufferMap;

  Port(const int &fd, Session &session, const v4l2_buf_type type);

  const v4l2_format &getFormat(bool refresh = false);
  void setFormat(uint32_t pixelformat, unsigned int numPlanes = 0,
                 unsigned int width = 0, unsigned int height = 0);
  void requestBuffers(unsigned int count, v4l2_memory memory);
  void queueBuffers();
  BufferV4L2 *dequeueBuffer();
  BufferV4L2 *getBuffer(unsigned int index);
  size_t getBufferCount() const;
  size_t getBuffersPending() const;
  void streamOn();
  void streamOff();
  void portSettingsChanged();

 private:
  const int &fd;
  Session &session;
  v4l2_format format;
  BufferMap buffers;
};

class Codec {
 public:
  Codec(const v4l2_buf_type input, const v4l2_buf_type output);
  ~Codec();

  Port &getPort(const v4l2_buf_type type);
  Port &getPort(const unsigned int index);
  void setSecureVideo(bool securevideo);
  pollfd cpoll();

 private:
  Session session;
  int fd;
  std::vector<Port> ports;
};

/****************************************************************************
 * BufferV4L2
 ****************************************************************************/

BufferV4L2::BufferV4L2(const int &fd, const v4l2_buf_type type,
                       const unsigned int index)
    : fd(fd), queued(false) {
  buf.type = type;
  buf.index = index;

  if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
    buf.length = VIDEO_MAX_PLANES;
    buf.m.planes = planes;
  }

  query();
  print("Construct");
}

const v4l2_buffer &BufferV4L2::getBuffer() { return buf; }

unsigned int BufferV4L2::getNumPlanes() const {
  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    return buf.length;
  } else {
    return 1;
  }
}

void BufferV4L2::checkPlanes(unsigned int plane) const {
  if (plane >= getNumPlanes()) {
    throw Exception(
        "Failed to get buffer length. Plane out of range. plane=%u.", plane);
  }
}

unsigned int BufferV4L2::getLength(const unsigned int plane) const {
  checkPlanes(plane);

  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    return buf.m.planes[plane].length;
  } else {
    return buf.length;
  }
}

void BufferV4L2::setLength(const unsigned int length,
                           const unsigned int plane) {
  checkPlanes(plane);

  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    buf.m.planes[plane].length = length;
  } else {
    buf.length = length;
  }
}

unsigned int BufferV4L2::getBytesUsed(const unsigned int plane) const {
  checkPlanes(plane);

  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    return buf.m.planes[plane].bytesused;
  } else {
    return buf.bytesused;
  }
}

void BufferV4L2::setBytesUsed(const unsigned int bytesused,
                              const unsigned int plane) {
  checkPlanes(plane);

  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    buf.m.planes[plane].bytesused = bytesused;
  } else {
    buf.bytesused = bytesused;
  }
}

void BufferV4L2::setEOS() { buf.flags |= V4L2_BUF_FLAG_LAST; }

bool BufferV4L2::isQueued() const { return queued; }

void BufferV4L2::clear() {
  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    for (uint32_t i = 0; i < buf.length; ++i) {
      v4l2_plane &p = buf.m.planes[i];

      p.bytesused = 0;
    }
  } else {
    buf.bytesused = 0;
  }

  buf.flags = 0;

  queued = false;
}

void BufferV4L2::query() {
  int ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
  if (ret != 0) {
    throw Exception("Failed to query buffer. ret=%d, type=%d, index=%u.", ret,
                    buf.type, buf.index);
  }

  queued = (buf.flags & V4L2_BUF_FLAG_QUEUED) != 0;
}

void BufferV4L2::queue() {
  print("Queue");

  int ret = ioctl(fd, VIDIOC_QBUF, &buf);
  if (ret != 0) {
    throw Exception("Failed to queue buffer. ret=%d, type=%d, index=%u.", ret,
                    buf.type, buf.index);
  }

  queued = true;
}

void BufferV4L2::dequeue(v4l2_buffer &buf) {
  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    this->buf = buf;
    this->buf.m.planes = this->planes;
    memcpy(this->planes, buf.m.planes, sizeof(*buf.m.planes) * buf.length);
  } else {
    this->buf = buf;
  }

  queued = false;
  print("Dequeue");
}

void BufferV4L2::print(const char *prefix) {
  cout << prefix << ": ";

  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    cout << "type=" << buf.type << ", index=" << buf.index
         << ", memory=" << buf.memory << ", length=" << buf.length
         << ", planes=[";

    for (unsigned int i = 0; i < buf.length; ++i) {
      v4l2_plane &plane = buf.m.planes[i];

      if (i > 0) {
        cout << ", ";
      }

      cout << "{bytesused=" << plane.bytesused << ", length=" << plane.length
           << ", data_offset=" << plane.data_offset;

      switch (buf.memory) {
        case V4L2_MEMORY_DMABUF:
          cout << ", fd=" << plane.m.fd;
          break;
        default:
          break;
      }

      cout << "}";
    }

    cout << "]" << endl;
  } else {
    cout << "type=" << buf.type << ", index=" << buf.index
         << ", bytesused=" << buf.bytesused << ", memory=" << buf.memory
         << ", length=" << buf.length;

    switch (buf.memory) {
      case V4L2_MEMORY_DMABUF:
        cout << ", fd=" << buf.m.fd;
        break;
      default:
        break;
    }

    cout << endl;
  }
}

int BufferV4L2::getFd(const unsigned int plane) const {
  checkPlanes(plane);

  if (buf.memory != V4L2_MEMORY_DMABUF) {
    throw Exception(
        "Failed to get file descriptor. Illegal memory type. memory=%d.",
        buf.memory);
  }

  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    return buf.m.planes[plane].m.fd;
  } else {
    return buf.m.fd;
  }
}

void BufferV4L2::setFd(int fd, const unsigned int plane,
                       const unsigned offset) {
  checkPlanes(plane);

  if (buf.memory != V4L2_MEMORY_DMABUF) {
    throw Exception(
        "Failed to set file descriptor. Illegal memory type. memory=%d.",
        buf.memory);
  }

  if (V4L2_TYPE_IS_MULTIPLANAR(buf.type)) {
    buf.m.planes[plane].m.fd = fd;
    buf.m.planes[plane].data_offset = offset;
  } else {
    buf.m.fd = fd;
  }
}

void *BufferV4L2::map(const unsigned int plane) {
  checkPlanes(plane);

  int fd = getFd(plane);
  unsigned int length = getLength();
  void *p = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (p == 0) {
    throw Exception("Failed to mmap buffer.");
  }

  return p;
}

void BufferV4L2::unmap(void *p, const unsigned int plane) {
  unsigned int length = getLength(plane);
  munmap(p, length);
}

/****************************************************************************
 * Port
 ****************************************************************************/

Port::Port(const int &fd, Session &session, const v4l2_buf_type type)
    : fd(fd), session(session) {
  format.type = type;
}

const v4l2_format &Port::getFormat(bool refresh) {
  if (refresh) {
    int ret = ioctl(fd, VIDIOC_S_FMT, &format);
    if (ret != 0) {
      throw Exception("Failed to get format.");
    }
  }

  return format;
}

void Port::setFormat(uint32_t pixelformat, unsigned int numPlanes,
                     unsigned int width, unsigned int height) {
  if (V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
    v4l2_pix_format_mplane &f = format.fmt.pix_mp;

    f.pixelformat = pixelformat;
    f.width = width;
    f.height = height;
    f.num_planes = numPlanes;
  } else {
    v4l2_pix_format &f = format.fmt.pix;

    f.pixelformat = pixelformat;
    f.width = width;
    f.height = height;
    f.sizeimage = 4 * 1024 * 1024;
  }

  int ret = ioctl(fd, VIDIOC_S_FMT, &format);
  if (ret != 0) {
    throw Exception("Failed to set format.");
  }
}

void Port::requestBuffers(unsigned int count, v4l2_memory memory) {
  v4l2_requestbuffers reqbuf = {
      .count = count, .type = format.type, .memory = memory};

  int ret = ioctl(fd, VIDIOC_REQBUFS, &reqbuf);
  if (ret != 0) {
    throw Exception("Failed to request buffers.");
  }

  /* Free buffers if the buffers count is decreased. */
  for (size_t i = buffers.size(); i > reqbuf.count; --i) {
    size_t index = i - 1;
    BufferMap::iterator it = buffers.find(index);
    if (it == buffers.end()) {
      throw Exception("Failed to find buffer. index=%zu.", index);
    }

    delete it->second;
    buffers.erase(it);
  }

  /* Allocate new buffers if count is increased. */
  for (size_t i = buffers.size(); i < reqbuf.count; ++i) {
    BufferV4L2 *buffer = new BufferV4L2(fd, v4l2_buf_type(format.type), i);

    /* TODO add class for DMA buffer handling. */
    if (memory == V4L2_MEMORY_DMABUF) {
      for (unsigned int plane = 0; plane < buffer->getNumPlanes(); ++plane) {
        unsigned int length = 4 * 1024 * 1024;

        BufferION::BufferType bufferType = V4L2_TYPE_IS_OUTPUT(format.type)
                                               ? BufferION::BUFFER_TYPE_INPUT
                                               : BufferION::BUFFER_TYPE_OUTPUT;
        BufferION *secureBuffer = session.allocateBuffer(bufferType, length);

        buffer->setFd(secureBuffer->getFileDescriptor(), plane);
        buffer->setLength(length, plane);
      }
    }

    buffers[buffer->getBuffer().index] = buffer;
  }
}

void Port::queueBuffers() {
  for (size_t i = 0; i < getBufferCount(); ++i) {
    BufferV4L2 *buffer = getBuffer(i);
    buffer->queue();
  }
}

BufferV4L2 *Port::dequeueBuffer() {
  v4l2_buffer buf;
  v4l2_plane planes[VIDEO_MAX_PLANES];

  buf.type = format.type;

  if (V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
    buf.length = VIDEO_MAX_PLANES;
    buf.m.planes = planes;
  }

  int ret = ioctl(fd, VIDIOC_DQBUF, &buf);
  if (ret != 0) {
    throw Exception("Failed to dequeue buffer. ret=%d, type=%d.", ret,
                    buf.type);
  }

  BufferV4L2 *buffer = buffers.at(buf.index);
  buffer->dequeue(buf);

  return buffer;
}

BufferV4L2 *Port::getBuffer(unsigned int index) { return buffers.at(index); }

size_t Port::getBufferCount() const { return buffers.size(); }

size_t Port::getBuffersPending() const {
  size_t pending = 0;

  for (BufferMap::const_iterator it = buffers.begin(); it != buffers.end();
       ++it) {
    if (it->second->isQueued()) {
      pending++;
    }
  }

  return pending;
}

void Port::streamOn() {
  cout << "Stream on. type=" << format.type << "." << endl;

  int ret = ioctl(fd, VIDIOC_STREAMON, &format.type);
  if (ret != 0) {
    throw Exception("Failed to stream on. type=%d.", format.type);
  }
}

void Port::streamOff() {
  cout << "Stream off. type=" << format.type << "." << endl;

  int ret = ioctl(fd, VIDIOC_STREAMOFF, &format.type);
  if (ret != 0) {
    throw Exception("Failed to stream on. type=%d.", format.type);
  }

  for (BufferMap::const_iterator it = buffers.begin(); it != buffers.end();
       ++it) {
    it->second->clear();
  }
}

void Port::portSettingsChanged() {
  streamOff();
  requestBuffers(6, V4L2_MEMORY_DMABUF);
  queueBuffers();
  streamOn();
}

/****************************************************************************
 * Codec
 ****************************************************************************/

Codec::Codec(const v4l2_buf_type input, const v4l2_buf_type output) : fd(-1) {
  fd = open("/dev/video0", O_RDWR);
  if (fd < 0) {
    throw Exception("Failed to open device.");
  }

  for (int i = 0; i < 2; ++i) {
    const v4l2_buf_type type[2] = {input, output};
    ports.push_back(Port(fd, session, type[i]));
  }
}

Codec::~Codec() {
  if (fd > 0) {
    close(fd);
  }
}

Port &Codec::getPort(const v4l2_buf_type type) {
  if (V4L2_TYPE_IS_OUTPUT(type)) {
    return ports[0];
  } else {
    return ports[1];
  }
}

Port &Codec::getPort(const unsigned int index) {
  if (index >= ports.size()) {
    throw Exception("Port index out of range. index=%u.", index);
  }

  return ports[index];
}

void Codec::setSecureVideo(bool securevideo) {
  struct v4l2_control ctrl = {.id = V4L2_CID_MVE_VIDEO_SECURE_VIDEO,
                              .value = securevideo};

  if (ioctl(fd, VIDIOC_S_CTRL, &ctrl) != 0) {
    throw Exception("Failed to enable/disable secure video.");
  }
}

pollfd Codec::cpoll() {
  pollfd p = {.fd = fd, .events = POLLPRI};
  short int events[] = {POLLOUT, POLLIN};

  for (size_t i = 0; i < 2; ++i) {
    if (ports[i].getBuffersPending() > 0) {
      p.events |= events[i];
    }
  }

  int ret = poll(&p, 1, 10000);
  if (ret < 0) {
    throw Exception("Poll returned error code.");
  }

  if (p.revents & POLLERR) {
    throw Exception("Poll returned error event.");
  }

  if (ret == 0) {
    throw Exception("Poll timed out.");
  }

  return p;
}

/****************************************************************************
 * main
 ****************************************************************************/

static size_t fillSecureBuffer(ifstream &ifs, BufferV4L2 &buffer) {
  if (ifs.eof()) {
    return 0;
  }

  const unsigned int plane = 0;
  unsigned int length = buffer.getLength(plane);

  void *map = buffer.map(plane);
  ifs.read(static_cast<char *>(map), length);
  buffer.unmap(map, plane);

  buffer.setBytesUsed(ifs.gcount(), plane);
  if (ifs.eof()) {
    buffer.setEOS();
  }

  return ifs.gcount();
}

static void help(const char *exe) {
  cerr << "Usage: " << exe << " BITSTREAM" << endl << endl;
  cerr << "Arguments:" << endl;
  cerr << "    -h --help  This help message." << endl;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    cerr << "Error: Missing argument." << endl;
    help(argv[0]);
    return 1;
  }

  string arg(argv[1]);

  if (arg == "-h" || arg == "--help") {
    help(argv[0]);
    return 1;
  }

  ifstream ifs(arg);
  if (!ifs) {
    cerr << "Error: Failed to open '" << arg << "'." << endl;
    return 1;
  }

  /* Instantiate codec and get references to input and output ports. */
  Codec codec(V4L2_BUF_TYPE_VIDEO_OUTPUT, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  Port &input = codec.getPort(0);
  Port &output = codec.getPort(1);
  Port *ports[] = {&input, &output};

  /* Enable secure video. */
  codec.setSecureVideo(true);

  /* Set input and output formats. */
  input.setFormat(V4L2_PIX_FMT_H264);
  output.setFormat(V4L2_PIX_FMT_YUV420M, 3, 1920, 1080);

  /* Request input- and output buffers. */
  for (int i = 0; i < 2; ++i) {
    ports[i]->requestBuffers(1, V4L2_MEMORY_DMABUF);
  }

  /* Queue empty output buffers. */
  for (size_t i = 0; i < output.getBufferCount(); ++i) {
    BufferV4L2 *buffer = output.getBuffer(i);
    buffer->clear();
    buffer->queue();
  }

  /* Fill and queue input buffers. */
  for (size_t i = 0; i < input.getBufferCount(); ++i) {
    BufferV4L2 *buffer = input.getBuffer(i);
    size_t n = fillSecureBuffer(ifs, *buffer);
    if (n > 0) {
      buffer->queue();
    }
  }

  /* Stream on both ports. */
  for (int i = 0; i < 2; ++i) {
    ports[i]->streamOn();
  }

  while (true) {
    pollfd p = codec.cpoll();

    if (p.revents & POLLOUT) {
      cout << "Dequeue input buffer." << endl;
      BufferV4L2 *buffer = input.dequeueBuffer();
      size_t n = fillSecureBuffer(ifs, *buffer);
      if (n > 0) {
        buffer->queue();
      }
    }

    if (p.revents & POLLIN) {
      cout << "Dequeue output buffer." << endl;
      BufferV4L2 *buffer = output.dequeueBuffer();
      const v4l2_buffer &buf = buffer->getBuffer();

      if (buf.flags & V4L2_BUF_FLAG_LAST) {
        cout << "Output EOS." << endl;
        break;
      }

      if (buffer->getBytesUsed() == 0 &&
          (buf.flags & V4L2_BUF_FLAG_ERROR) == 0) {
        cout << "Port settings changed." << endl;
        output.portSettingsChanged();
      } else {
        buffer->clear();
        buffer->queue();
      }
    }
  }

  return 0;
}
