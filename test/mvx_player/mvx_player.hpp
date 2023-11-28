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

#ifndef __MVX_PLAYER_H__
#define __MVX_PLAYER_H__

/****************************************************************************
 * Includes
 ****************************************************************************/

#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cmath>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

#include "dmabufheap/BufferAllocatorWrapper.h"
#include "mvx-v4l2-controls.h"
#include "reader/parser.h"
#include "reader/read_util.h"
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
 * Buffer
 ****************************************************************************/

class Buffer {
 public:
  Buffer(const v4l2_format &format);
  Buffer(v4l2_buffer &buf, int fd, const v4l2_format &format);
  virtual ~Buffer();

  v4l2_buffer &getBuffer();
  const v4l2_format &getFormat() const;
  void setCrop(const v4l2_crop &crop);
  const v4l2_crop &getCrop() const;
  std::vector<iovec> getImageSize() const;
  std::vector<iovec> getBytesUsed() const;
  void setBytesUsed(std::vector<iovec> &iov);
  void clearBytesUsed();
  void resetVendorFlags();
  void setCodecConfig(bool codecConfig);
  void setTimeStamp(unsigned int timeUs);
  void setEndOfFrame(bool eof);
  void setEndOfStream(bool eos);
  void update(v4l2_buffer &buf);
  void setInterlaced(bool interlaced);
  void setTiled(bool tiled);
  void setBlockSplit(bool split);
  void setRotation(int rotation);
  void setMirror(int mirror);
  void setDownScale(int scale);
  void setEndOfSubFrame(bool eos);
  std::vector<iovec> convert10Bit();
  void setRoiCfg(struct v4l2_mvx_roi_regions roi);
  bool getRoiCfgflag() { return isRoiCfg; }
  struct v4l2_mvx_roi_regions getRoiCfg() {
    return roi_cfg;
  };
  void setSuperblock(bool superblock);
  void setROIflag();
  void setEPRflag();
  void setQPofEPR(int data) { qp = data; };
  int getQPofEPR() { return qp; }
  bool isGeneralBuffer() {
    return (buf.flags & V4L2_BUF_FLAG_MVX_BUFFER_EPR) ==
           V4L2_BUF_FLAG_MVX_BUFFER_EPR;
  };

 private:
  void memoryMap(int fd);
  void memoryUnmap();
  size_t getLength(unsigned int plane);

  void *ptr[VIDEO_MAX_PLANES];
  v4l2_buffer buf;
  v4l2_plane planes[VIDEO_MAX_PLANES];
  const v4l2_format &format;
  v4l2_crop crop;
  bool isRoiCfg;
  struct v4l2_mvx_roi_regions roi_cfg;
  int qp;
  int dma_fd;
  BufferAllocator *bufferAllocator;
  int total_length;
  int plane_offset[VIDEO_MAX_PLANES];
  int plane_length[VIDEO_MAX_PLANES];
};

/****************************************************************************
 * Input and output
 ****************************************************************************/

#pragma pack(push, 1)
class IVFHeader {
 public:
  IVFHeader();
  IVFHeader(uint32_t codec, uint16_t width, uint16_t height);

  uint32_t signature;
  uint16_t version;
  uint16_t length;
  uint32_t codec;
  uint16_t width;
  uint16_t height;
  uint32_t frameRate;
  uint32_t timeScale;
  uint32_t frameCount;
  uint32_t padding;

  static const uint32_t signatureDKIF;
};

class IVFFrame {
 public:
  IVFFrame();
  IVFFrame(uint32_t size, uint64_t timestamp);

  uint32_t size;
  uint64_t timestamp;
};

/* STRUCT_C (for details see specification SMPTE-421M) */
struct HeaderC {
  uint32_t reserved : 28;
  uint32_t profile : 4;
};

/* Sequence Layer Data (for details see specification SMPTE-421M) */
class VC1SequenceLayerData {
 public:
  VC1SequenceLayerData();

  uint32_t numFrames : 24;
  uint8_t signature1;
  uint32_t signature2;
  uint32_t headerC;
  uint32_t restOfSLD[6];

  static const uint8_t magic1;
  static const uint32_t magic2;
};

/* Frame Layer Data (for details see specification SMPTE-421M) */
class VC1FrameLayerData {
 public:
  VC1FrameLayerData();

  uint32_t frameSize : 24;
  uint32_t reserved : 7;
  uint32_t key : 1;
  uint32_t timestamp;
  uint8_t data[];
};

class AFBCHeader {
 public:
  AFBCHeader();
  AFBCHeader(const v4l2_format &format, size_t frameSize, const v4l2_crop &crop,
             bool tiled, const int field = FIELD_NONE);

  uint32_t magic;
  uint16_t headerSize;
  uint16_t version;
  uint32_t frameSize;
  uint8_t numComponents;
  uint8_t subsampling;
  uint8_t yuvTransform;
  uint8_t blockSplit;
  uint8_t yBits;
  uint8_t cbBits;
  uint8_t crBits;
  uint8_t alphaBits;
  uint16_t mbWidth;
  uint16_t mbHeight;
  uint16_t width;
  uint16_t height;
  uint8_t cropLeft;
  uint8_t cropTop;
  uint8_t param;
  uint8_t fileMessage;

  static const uint32_t MAGIC = 0x43424641;
  static const uint16_t VERSION = 5;  // 5: version 1.2; 3: version 1.0;
  static const uint8_t PARAM_TILED_BODY = 0x00000001;
  static const uint8_t PARAM_TILED_HEADER = 0x00000002;
  static const uint8_t PARAM_32X8_SUPERBLOCK = 0x00000004;
  static const int FIELD_NONE = 0;
  static const int FIELD_TOP = 1;
  static const int FIELD_BOTTOM = 2;
};
#pragma pack(pop)

class IO {
 public:
  IO(uint32_t format, bool preload = 0, size_t width = 0, size_t height = 0,
     size_t strideAlign = 0);
  virtual ~IO() {}

  virtual void preloadBuffer(v4l2_buf_type type) {}
  virtual void prepare(Buffer &buf) {}
  virtual void finalize(Buffer &buf) {}
  virtual bool eof() { return false; }
  virtual void setNaluFormat(int nalu) {}
  virtual int getNaluFormat() { return 0; }
  virtual bool needDoubleCount() { return false; };
  virtual uint64_t getCurTimestamp() { return timestamp; }
  virtual void resetCurTimestamp() { timestamp = 0; }

  uint32_t getFormat() const { return format; }
  uint8_t getProfile() const { return profile; }
  size_t getWidth() const { return width; }
  size_t getHeight() const { return height; }
  size_t getStrideAlign() const { return strideAlign; }
  int getDir() { return dir; }
  bool getPreload() { return isPreload; }
  uint32_t getFrameSize() const { return frameSize; }

 protected:
  uint32_t format;
  uint8_t profile;
  size_t width;
  size_t height;
  size_t strideAlign;
  int dir;  // 0 for input; 1 for output
  bool isPreload;

 public:
  std::set<uint64_t> timestampList;
  unsigned int timestamp;
  uint32_t frameSize;
};

class Input : public IO {
 public:
  Input(uint32_t format, bool preload = 0, size_t width = 0, size_t height = 0,
        size_t strideAlign = 0);

  virtual void preloadBuffer(v4l2_buf_type type) {}
  virtual void prepare(Buffer &buf) {}
  virtual void finalize(Buffer &buf) {}
  virtual void setNaluFormat(int nalu) {}
  virtual int getNaluFormat() { return 0; }
};

class InputFile : public Input {
 public:
  InputFile(std::istream &input, uint32_t format, bool preload = 0);
  virtual ~InputFile();

  virtual void preloadBuffer(v4l2_buf_type type);
  virtual void prepare(Buffer &buf);
  virtual bool eof();
  virtual void setNaluFormat(int nalu) { naluFmt = nalu; }
  virtual int getNaluFormat() { return naluFmt; }

 protected:
  InputFile(std::istream &input, uint32_t format, size_t width, size_t height,
            size_t strideAlign, bool preload = 0);
  int readBuffer(char *dest, int len);
  void ignoreBuffer(int len);
  std::istream &input;
  char *inputBuf;
  char *inputPreBuf;
  int offset;
  int state;
  int curlen;
  bool iseof;
  int naluFmt;
  uint32_t remaining_bytes;
  start_code_reader *reader;

 public:
  int read_pos;
  int total_len;
};

class InputIVF : public InputFile {
 public:
  InputIVF(std::istream &input, uint32_t informat, bool preload = 0);

  virtual void prepare(Buffer &buf);
  virtual bool eof();

 protected:
  uint32_t left_bytes;
  uint64_t timestamp;
};

class InputRCV : public InputFile {
 public:
  InputRCV(std::istream &input, bool preload = 0);

  virtual void prepare(Buffer &buf);
  virtual bool eof();

 private:
  bool codecConfigSent;
  VC1SequenceLayerData sld;
  uint32_t left_bytes;
  bool isRcv;
};

class InputAFBC : public InputFile {
 public:
  InputAFBC(std::istream &input, uint32_t format, size_t width, size_t height,
            bool preload);

  virtual void prepare(Buffer &buf);
  virtual bool eof();
};

class InputFileFrame : public InputFile {
 public:
  InputFileFrame(std::istream &input, uint32_t format, size_t width,
                 size_t height, size_t strideAlign, bool preload = 0);

  virtual void prepare(Buffer &buf);

 protected:
  size_t nplanes;
  size_t stride[3];
  size_t size[3];
};

struct epr_config {
  unsigned int pic_index;

  struct v4l2_buffer_general_block_configs block_configs;
  struct v4l2_buffer_param_qp qp;

  bool block_configs_present;
  bool qp_present;

  size_t bc_row_body_size;
  union {
    char *_bc_row_body_data;
    struct v4l2_buffer_general_rows_uncomp_body *uncomp;
  } bc_row_body;

  epr_config(const size_t size = 0) {
    pic_index = 0;
    qp.qp = 0;
    clear();
    allocate_bprf(size);
  };
  epr_config(const epr_config &other)
      : pic_index(other.pic_index),
        block_configs(other.block_configs),
        qp(other.qp),
        block_configs_present(other.block_configs_present),
        qp_present(other.qp_present) {
    allocate_bprf(other.bc_row_body_size);

    if (other.bc_row_body_size > 0) {
      std::copy(other.bc_row_body._bc_row_body_data,
                other.bc_row_body._bc_row_body_data + other.bc_row_body_size,
                bc_row_body._bc_row_body_data);
    }
  };
  ~epr_config() {
    if (bc_row_body_size > 0) {
      delete[] bc_row_body._bc_row_body_data;
    }
  };
  epr_config &operator=(epr_config other) {
    swap(*this, other);
    return *this;
  };
  friend void swap(epr_config &a, epr_config &b) {
    using std::swap;

    swap(a.pic_index, b.pic_index);
    swap(a.block_configs, b.block_configs);
    swap(a.qp, b.qp);
    swap(a.block_configs_present, b.block_configs_present);
    swap(a.qp_present, b.qp_present);

    swap(a.bc_row_body_size, b.bc_row_body_size);
    swap(a.bc_row_body._bc_row_body_data, b.bc_row_body._bc_row_body_data);
  };
  void clear(void) {
    block_configs_present = false;
    qp_present = false;
  }

 private:
  void allocate_bprf(size_t size) {
    bc_row_body_size = size;
    if (size > 0) {
      bc_row_body._bc_row_body_data = new char[size];
    } else {
      bc_row_body._bc_row_body_data = NULL;
    }
  };
};

typedef std::list<epr_config> v4l2_epr_list_t;
typedef std::list<v4l2_mvx_roi_regions> v4l2_roi_list_t;

class InputFileFrameWithROI : public InputFileFrame {
 public:
  InputFileFrameWithROI(std::istream &input, uint32_t format, size_t width,
                        size_t height, size_t strideAlign, std::istream &roi);
  virtual void prepare(Buffer &buf);
  virtual ~InputFileFrameWithROI();

 private:
  void load_roi_cfg();
  std::istream &roi_is;
  v4l2_roi_list_t *roi_list;
  unsigned int prepared_frames;
};

class InputFileFrameWithEPR : public InputFileFrame {
 public:
  InputFileFrameWithEPR(std::istream &input, uint32_t format, size_t width,
                        size_t height, size_t strideAlign, std::istream &epr,
                        uint32_t oformat);
  virtual ~InputFileFrameWithEPR();
  virtual void prepare(Buffer &buf);
  void prepareEPR(Buffer &buf);
  virtual bool needDoubleCount() { return true; };

 private:
  std::istream &epr_is;
  v4l2_epr_list_t *epr_list;
  unsigned int prepared_frames;
  v4l2_epr_list_t::iterator cur;
  uint32_t outformat;
  void load_epr_cfg();
  void read_efp_cfg(char *buf, int num_epr, struct epr_config *config);
  void read_row_cfg(char *buf, int row, int len, struct epr_config &config);
  void erp_adjust_bpr_to_64_64(
      struct v4l2_buffer_general_rows_uncomp_body *uncomp_body, int qp_delta,
      uint32_t bpr_base_idx, uint32_t row_off, uint8_t force);
};

class InputFrame : public Input {
 public:
  InputFrame(uint32_t format, size_t width, size_t height, size_t strideAlign,
             size_t nframes);

  virtual void prepare(Buffer &buf);
  virtual bool eof();

 private:
  void rgb2yuv(unsigned int yuv[3], const unsigned int rgb[3]);

  size_t nplanes;
  size_t stride[3];
  size_t size[3];
  size_t nframes;
  size_t count;
};

class Output : public IO {
 public:
  Output(uint32_t format);
  virtual ~Output();

  virtual void prepare(Buffer &buf);
  virtual void finalize(Buffer &buf);
  virtual void write(void *ptr, size_t nbytes) {}
  virtual bool getMd5CheckResult() { return true; }

 protected:
  size_t totalSize;
};

class OutputFile : public Output {
 public:
  OutputFile(std::ostream &output, uint32_t format);

  virtual void write(void *ptr, size_t nbytes);
  virtual bool getMd5CheckResult() { return true; }

 private:
  std::ostream &output;
};

class OutputIVF : public OutputFile {
 public:
  OutputIVF(std::ofstream &output, uint32_t format, uint16_t width,
            uint16_t height);

  virtual void finalize(Buffer &buf);

 private:
  std::vector<char> temp;
};

class OutputAFBC : public OutputFile {
 public:
  OutputAFBC(std::ofstream &output, uint32_t format, bool tiled);
  virtual void prepare(Buffer &buf);
  virtual void finalize(Buffer &buf);

 protected:
  bool tiled;
};

class OutputAFBCInterlaced : public OutputAFBC {
 public:
  OutputAFBCInterlaced(std::ofstream &output, uint32_t format, bool tiled);
  virtual void finalize(Buffer &buf);
};

#define HASH_DIGEST_LENGTH 16
#define STR_HASH_SIZE (HASH_DIGEST_LENGTH * 2 + 2)

class OutputFileWithMD5 : public OutputFile {
 public:
  OutputFileWithMD5(std::ofstream &output, uint32_t format,
                    std::ofstream &output_md5, std::ifstream *md5ref);
  virtual void finalize(Buffer &buf);
  virtual bool getMd5CheckResult();
  bool checkMd5(char *cur_str_hash);

 private:
  std::ofstream &output_md5;
  std::ifstream *input_ref_md5;
  bool md5_check_result;
};
/****************************************************************************
 * Codec, Decoder, Encoder
 ****************************************************************************/

class Codec {
 public:
  typedef std::map<uint32_t, Buffer *> BufferMap;

  Codec(const char *dev, enum v4l2_buf_type inputType,
        enum v4l2_buf_type outputType, std::ostream &log, bool nonblock);
  Codec(const char *dev, Input &input, enum v4l2_buf_type inputType,
        Output &output, enum v4l2_buf_type outputType, std::ostream &log,
        bool nonblock);
  virtual ~Codec();

  int stream();

  static uint32_t to4cc(const std::string &str);
  static bool isVPx(uint32_t format);
  static bool isAFBC(uint32_t format);
  static void getStride(uint32_t format, size_t &nplanes, size_t stride[3][2]);
  static size_t getSize(uint32_t format, size_t width, size_t height,
                        size_t strideAlign, size_t &nplanes, size_t stride[3],
                        size_t size[3]);

 protected:
  enum NaluFormat {
    NALU_FORMAT_START_CODES,
    NALU_FORMAT_ONE_NALU_PER_BUFFER,
    NALU_FORMAT_ONE_BYTE_LENGTH_FIELD,
    NALU_FORMAT_TWO_BYTE_LENGTH_FIELD,
    NALU_FORMAT_FOUR_BYTE_LENGTH_FIELD
  };

  class Port {
   public:
    Port(int &fd, enum v4l2_buf_type type, std::ostream &log)
        : fd(fd),
          type(type),
          log(log),
          interlaced(false),
          tryEncStop(false),
          tryDecStop(false),
          mirror(0),
          scale(1),
          frames_processed(0),
          frames_count(0),
          isSourceChange(false),
          lastTimestamp(0),
          intervalTime(0),
          remainTime(0),
          memory_type(V4L2_MEMORY_DMABUF) {}
    Port(int &fd, IO &io, v4l2_buf_type type, std::ostream &log)
        : fd(fd),
          io(&io),
          type(type),
          log(log),
          pending(0),
          tid(0),
          interlaced(false),
          tryEncStop(false),
          tryDecStop(false),
          mirror(0),
          scale(1),
          frames_processed(0),
          frames_count(0),
          isSourceChange(false),
          lastTimestamp(0),
          intervalTime(0),
          remainTime(0),
          memory_type(V4L2_MEMORY_DMABUF) {}

    void enumerateFormats();
    const v4l2_format &getFormat();
    void tryFormat(v4l2_format &format);
    void setFormat(v4l2_format &format);
    void getTrySetFormat();
    void printFormat(const struct v4l2_format &format);
    const v4l2_crop getCrop();

    void allocateBuffers(size_t count);
    void freeBuffers();
    unsigned int getBufferCount();
    void queueBuffers();
    void queueBuffer(Buffer &buf);
    Buffer &dequeueBuffer();
    void printBuffer(const v4l2_buffer &buf, const char *prefix);

    bool handleBuffer();
    void handleResolutionChange();

    void streamon();
    void streamoff();

    void sendEncStopCommand();
    void sendDecStopCommand();

    void setH264DecIntBufSize(uint32_t ibs);
    void setNALU(NaluFormat nalu);
    void setEncFramerate(uint32_t fps);
    void setEncBitrate(uint32_t bps);
    void setEncPFrames(uint32_t pframes);
    void setEncBFrames(uint32_t bframes);
    void setEncSliceSpacing(uint32_t spacing);
    void setH264EncForceChroma(uint32_t fmt);
    void setH264EncBitdepth(uint32_t bd);
    void setH264EncIntraMBRefresh(uint32_t period);
    void setEncProfile(uint32_t profile);
    void setEncLevel(uint32_t level);
    void setEncConstrainedIntraPred(uint32_t cip);
    void setH264EncEntropyMode(uint32_t ecm);
    void setH264EncGOPType(uint32_t gop);
    void setH264EncMinQP(uint32_t minqp);
    void setH264EncMaxQP(uint32_t maxqp);
    void setH264EncFixedQP(uint32_t fqp);
    void setH264EncFixedQPI(uint32_t fqp);
    void setH264EncFixedQPP(uint32_t fqp);
    void setH264EncFixedQPB(uint32_t fqp);
    void setH264EncBandwidth(uint32_t bw);
    void setHEVCEncEntropySync(uint32_t es);
    void setHEVCEncTemporalMVP(uint32_t tmvp);
    void setEncStreamEscaping(uint32_t sesc);
    void setEncHorizontalMVSearchRange(uint32_t hmvsr);
    void setEncVerticalMVSearchRange(uint32_t vmvsr);
    void setVP9EncTileCR(uint32_t tcr);
    void setJPEGEncQuality(uint32_t q);
    void setJPEGEncRefreshInterval(uint32_t r);
    void setInterlaced(bool interlaced);
    void setRotation(int rotation);
    void setMirror(int mirror);
    void setDownScale(int scale);
    void tryEncStopCmd(bool tryStop);
    void tryDecStopCmd(bool tryStop);
    void setDecFrameReOrdering(uint32_t fro);
    void setDecIgnoreStreamHeaders(uint32_t ish);
    bool isEncoder();
    void setFrameCount(int frames);
    void setRateControl(struct v4l2_rate_control *rc);
    void setCropLeft(int left);
    void setCropRight(int right);
    void setCropTop(int top);
    void setCropBottom(int bottom);
    void setVuiColourDesc(struct v4l2_mvx_color_desc *color);
    void setSeiUserData(struct v4l2_sei_user_data *sei_user_data);
    void setHRDBufferSize(int size);
    void setDSLFrame(int width, int height);
    void setDSLRatio(int hor, int ver);
    void setLongTermRef(uint32_t mode, uint32_t period);
    void setDSLMode(int mode);
    int getFramesProcessed() { return frames_processed; }
    void notifySourceChange();
    void setFramerate(int framerate);
    void controlFramerate();
    void setFWTimeout(int timeout);
    void setProfiling(int enable);

    int &fd;
    IO *io;
    v4l2_buf_type type;
    v4l2_format format;
    std::ostream &log;
    BufferMap buffers;
    size_t pending;
    pthread_t tid;
    FILE *roi_cfg;

   private:
    int rotation;
    bool interlaced;
    bool tryEncStop;
    bool tryDecStop;
    int mirror;
    int scale;
    int frames_processed;
    int frames_count;
    int rc_type;
    bool isSourceChange;
    int fps;
    uint64_t lastTimestamp;
    uint64_t intervalTime;
    uint64_t remainTime;
    uint32_t memory_type;
  };

  static size_t getBytesUsed(v4l2_buffer &buf);
  void enumerateFormats();

  Port input;
  Port output;
  int fd;
  std::ostream &log;
  bool csweo;
  uint32_t fps;
  uint32_t bps;
  uint32_t minqp;
  uint32_t maxqp;
  uint32_t fixedqp;
  float avgfps;

 private:
  void openDev(const char *dev);
  void closeDev();

  void queryCapabilities();
  void enumerateFramesizes(uint32_t format);
  void setFormats();

  v4l2_mvx_color_desc getColorDesc();
  void printColorDesc(const v4l2_mvx_color_desc &color);

  void subscribeEvents();
  void subscribeEvents(uint32_t event);
  void unsubscribeEvents();
  void unsubscribeEvents(uint32_t event);

  void allocateBuffers();
  void freeBuffers();
  void queueBuffers();

  void streamon();
  void streamoff();

  void runPoll();
  void runThreads();
  static void *runThreadInput(void *arg);
  static void *runThreadOutput(void *arg);
  bool handleEvent();
  void checkOutputTimestamp(uint64_t timestamp);

  bool nonblock;

  uint64_t timestart_us;
  uint64_t timeend_us;

 public:
  int getInputFramesProcessed() { return input.getFramesProcessed(); }
  int getOutputFramesProcessed() { return output.getFramesProcessed(); }
  float getAverageFramerate() { return avgfps; }
};

class Decoder : public Codec {
 public:
  Decoder(const char *dev, Input &input, Output &output, bool nonblock = true,
          std::ostream &log = std::cout);
  void setH264IntBufSize(uint32_t ibs);
  void setInterlaced(bool interlaced);
  void setFrameReOrdering(uint32_t fro);
  void setIgnoreStreamHeaders(uint32_t ish);
  void tryStopCmd(bool tryStop);
  void setNaluFormat(int nalu);
  void setRotation(int rotation);
  void setDownScale(int scale);
  void setFrameCount(int frames);
  void setDSLFrame(int width, int height);
  void setDSLRatio(int hor, int ver);
  void setDSLMode(int mode);
  void setFramerate(int fps);
  void setFWTimeout(int timeout);
  void setProfiling(int enable);

 private:
  int naluFmt;
};

class Encoder : public Codec {
 public:
  Encoder(const char *dev, Input &input, Output &output, bool nonblock = true,
          std::ostream &log = std::cout);
  void changeSWEO(uint32_t csweo);
  void setFramerate(uint32_t fps);
  void setBitrate(uint32_t bps);
  void setPFrames(uint32_t pframes);
  void setBFrames(uint32_t bframes);
  void setSliceSpacing(uint32_t spacing);
  void setConstrainedIntraPred(uint32_t cip);
  void setH264ForceChroma(uint32_t fmt);
  void setH264Bitdepth(uint32_t bd);
  void setH264IntraMBRefresh(uint32_t period);
  void setProfile(uint32_t profile);
  void setLevel(uint32_t level);
  void setH264EntropyCodingMode(uint32_t ecm);
  void setH264GOPType(uint32_t gop);
  void setH264MinQP(uint32_t minqp);
  void setH264MaxQP(uint32_t maxqp);
  void setH264FixedQP(uint32_t fqp);
  void setH264FixedQPI(uint32_t fqp);
  void setH264FixedQPP(uint32_t fqp);
  void setH264FixedQPB(uint32_t fqp);
  void setH264Bandwidth(uint32_t bw);
  void setVP9TileCR(uint32_t tcr);
  void setJPEGRefreshInterval(uint32_t ri);
  void setJPEGQuality(uint32_t q);
  void setHEVCEntropySync(uint32_t es);
  void setHEVCTemporalMVP(uint32_t tmvp);
  void setStreamEscaping(uint32_t sesc);
  void setHorizontalMVSearchRange(uint32_t hmvsr);
  void setVerticalMVSearchRange(uint32_t vmvsr);
  void tryStopCmd(bool tryStop);
  void setMirror(int mirror);
  void setFrameCount(int frames);
  void setRateControl(const std::string &rc, int target_bitrate,
                      int maximum_bitrate);
  void setCropLeft(int left);
  void setCropRight(int right);
  void setCropTop(int top);
  void setCropBottom(int bottom);
  void setVuiColourDesc(struct v4l2_mvx_color_desc *color);
  void setSeiUserData(struct v4l2_sei_user_data *sei_user_data);
  void setHRDBufferSize(int size);
  void setLongTermRef(uint32_t mode, uint32_t period);
  void setFWTimeout(int timeout);
  void setProfiling(int enable);
};

class Info : public Codec {
 public:
  Info(const char *dev, std::ostream &log = std::cout);
  void enumerateFormats();
};

#endif /* __MVX_PLAYER_H__ */
