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

#include <fstream>

#include "mvx_argparse.h"
#include "mvx_player.hpp"

using namespace std;

int main(int argc, const char *argv[]) {
  int ret;
  mvx_argparse argp;
  uint32_t inputFormat;
  uint32_t outputFormat;
  const char *md5_filename = NULL;
  ofstream *md5_os = NULL;
  const char *md5ref_filename = NULL;
  ifstream *md5ref_is = NULL;

  mvx_argp_construct(&argp);
  mvx_argp_add_opt(&argp, '\0', "dev", true, 1, "/dev/video0", "Device.");
  mvx_argp_add_opt(&argp, 'i', "inputformat", true, 1, "h264", "Pixel format.");
  mvx_argp_add_opt(&argp, 'o', "outputformat", true, 1, "yuv420",
                   "Output pixel format.");
  mvx_argp_add_opt(&argp, 'f', "format", true, 1, "ivf",
                   "Input container format. [ivf, rcv, raw]\n\t\tFor ivf input "
                   "format will be taken from IVF header.");
  mvx_argp_add_opt(&argp, 's', "stride", true, 1, "1", "Stride alignment.");
  mvx_argp_add_opt(&argp, 'y', "intbuf", true, 1, "1000000",
                   "Limit of intermediate buffer size");
  mvx_argp_add_opt(&argp, 'm', "md5", true, 1, NULL, "Output md5 file");
  mvx_argp_add_opt(&argp, 'e', "md5ref", true, 1, NULL, "Input ref md5 file");
  mvx_argp_add_opt(&argp, 'u', "nalu", true, 1, "0",
                   "Nalu format, START_CODES (0) and ONE_NALU_PER_BUFFER (1), "
                   "ONE_BYTE_LENGTH_FIELD (2), TWO_BYTE_LENGTH_FIELD (3), "
                   "FOUR_BYTE_LENGTH_FIELD (3).");
  mvx_argp_add_opt(&argp, 'r', "rotate", true, 1, "0",
                   "Rotation, 0 | 90 | 180 | 270");
  mvx_argp_add_opt(&argp, 'd', "downscale", true, 1, "1",
                   "Down Scale, 1 | 2 | 4");
  mvx_argp_add_opt(&argp, 'v', "fps", true, 1, "24", "Frame rate.");
  mvx_argp_add_opt(&argp, 0, "dsl_ratio_hor", true, 1, "0",
                   "Horizontal downscale ratio, [1, 256]");
  mvx_argp_add_opt(&argp, 0, "dsl_ratio_ver", true, 1, "0",
                   "Vertical downscale ratio, [1, 128]");
  mvx_argp_add_opt(&argp, 0, "dsl_frame_width", true, 1, "0",
                   "Downscaled frame width in pixels");
  mvx_argp_add_opt(&argp, 0, "dsl_frame_height", true, 1, "0",
                   "Downscaled frame height in pixels");
  mvx_argp_add_opt(
      &argp, 0, "dsl_pos_mode", true, 1, "0",
      "Flexible Downscaled original position mode [0, 2], only availble in "
      "high precision mode."
      "\t\tValue: 0 [default:x_original=(x_resized + 0.5)/scale - 0.5]"
      "\t\tValue: 1 [x_original=x_reized/scale]"
      "\t\tValue: 2 [x_original=(x_resized+0.5)/scale]");
  mvx_argp_add_opt(&argp, 0, "frames", true, 1, "0", "nr of frames to process");
  mvx_argp_add_opt(&argp, 0, "fro", true, 1, "1",
                   "Frame reordering 1 is on (default), 0 is off");
  mvx_argp_add_opt(&argp, 0, "ish", true, 1, "0",
                   "Ignore Stream Headers 1 is on, 0 is off (default)");
  mvx_argp_add_opt(&argp, 0, "trystop", true, 0, "0",
                   "Try if Decoding Stop Command exixts");
  mvx_argp_add_opt(&argp, 0, "one_frame_per_packet", true, 0, "0",
                   "Each input buffer contains one frame.");
  mvx_argp_add_opt(&argp, 'n', "interlaced", true, 0, "0",
                   "Frames are interlaced");
  mvx_argp_add_opt(&argp, '\0', "tiled", true, 0, "disabled",
                   "Use tiles for AFBC formats.");
  mvx_argp_add_opt(&argp, 0, "preload", true, 0, "0",
                   "preload the input stream to memory. the size for input "
                   "file should be less than 15MBytes.");
  mvx_argp_add_opt(&argp, 0, "fw_timeout", true, 1, "5",
                   "timeout value[secs] for watchdog timeout. range: 5~60.");
  mvx_argp_add_opt(
      &argp, 0, "profiling", true, 1, "0",
      "enable profiling for bandwidth statistics . 0:disable; 1:enable.");
  mvx_argp_add_pos(&argp, "input", false, 1, "", "Input file.");
  mvx_argp_add_pos(&argp, "output", true, 1, "", "Output file.");

  ret = mvx_argp_parse(&argp, argc - 1, &argv[1]);
  if (ret != 0) {
    mvx_argp_help(&argp, argv[0]);
    return 1;
  }

  inputFormat = Codec::to4cc(mvx_argp_get(&argp, "inputformat", 0));
  if (inputFormat == 0) {
    fprintf(stderr, "Error: Illegal bitstream format. format=%s.\n",
            mvx_argp_get(&argp, "inputformat", 0));
    return 1;
  }

  bool isPreload = mvx_argp_is_set(&argp, "preload");

  ifstream is(mvx_argp_get(&argp, "input", 0));
  Input *inputFile;
  if (string(mvx_argp_get(&argp, "format", 0)).compare("ivf") == 0) {
    inputFile = new InputIVF(is, inputFormat, isPreload);
  } else if (string(mvx_argp_get(&argp, "format", 0)).compare("rcv") == 0) {
    inputFile = new InputRCV(is);
  } else if (string(mvx_argp_get(&argp, "format", 0)).compare("raw") == 0) {
    inputFile = new InputFile(is, inputFormat, isPreload);
  } else {
    cerr << "Error: Unsupported container format. format="
         << mvx_argp_get(&argp, "format", 0) << "." << endl;
    return 1;
  }
  int nalu_format = mvx_argp_get_int(&argp, "nalu", 0);
  int rotation = mvx_argp_get_int(&argp, "rotate", 0);
  int scale = mvx_argp_get_int(&argp, "downscale", 0);
  int frames = mvx_argp_get_int(&argp, "frames", 0);
  if (rotation % 90 != 0) {
    cerr << "Unsupported rotation:" << rotation << endl;
    rotation = 0;
  }
  outputFormat = Codec::to4cc(mvx_argp_get(&argp, "outputformat", 0));
  if (outputFormat == 0) {
    fprintf(stderr, "Error: Illegal frame format. format=%s.\n",
            mvx_argp_get(&argp, "outputformat", 0));
    return 1;
  }

  bool interlaced = mvx_argp_is_set(&argp, "interlaced");
  bool tiled = mvx_argp_is_set(&argp, "tiled");

  ofstream os(mvx_argp_get(&argp, "output", 0), ios::binary);
  Output *output;

  if (Codec::isAFBC(outputFormat)) {
    output = (interlaced) ? new OutputAFBCInterlaced(os, outputFormat, tiled)
                          : new OutputAFBC(os, outputFormat, tiled);
  } else {
    md5_filename = mvx_argp_get(&argp, "md5", 0);
    if (md5_filename) {
      printf("md5_filename is < %s >.\n", md5_filename);
      md5_os = new ofstream(md5_filename, ios::binary);
      if (NULL == md5_os) {
        fprintf(stderr, "Error: (NULL == md5_os).\n");
        return 1;
      }
      md5ref_filename = mvx_argp_get(&argp, "md5ref", 0);
      if (md5ref_filename) {
        printf("md5ref_filename is < %s >.\n", md5ref_filename);
        md5ref_is = new ifstream(md5ref_filename, ios::binary);
        if (NULL == md5ref_is) {
          fprintf(stderr, "Error: (NULL == md5ref_is).\n");
          return 1;
        }
      }
      if (md5ref_filename) {
        output = new OutputFileWithMD5(os, outputFormat, *md5_os, md5ref_is);
      } else {
        output = new OutputFileWithMD5(os, outputFormat, *md5_os, NULL);
      }
    } else {
      output = new OutputFile(os, outputFormat);
    }
  }

  Decoder decoder(mvx_argp_get(&argp, "dev", 0), *inputFile, *output);
  if (mvx_argp_is_set(&argp, "intbuf")) {
    decoder.setH264IntBufSize(mvx_argp_get_int(&argp, "intbuf", 0));
  }
  if (mvx_argp_is_set(&argp, "fro")) {
    decoder.setFrameReOrdering(mvx_argp_get_int(&argp, "fro", 0));
  }
  if (mvx_argp_is_set(&argp, "ish")) {
    decoder.setIgnoreStreamHeaders(mvx_argp_get_int(&argp, "ish", 0));
  }
  if (mvx_argp_is_set(&argp, "trystop")) {
    decoder.tryStopCmd(true);
  }
  if (mvx_argp_is_set(&argp, "one_frame_per_packet")) {
    decoder.setNaluFormat(V4L2_OPT_NALU_FORMAT_ONE_FRAME_PER_BUFFER);
  } else {
    decoder.setNaluFormat(nalu_format);
  }

  if (mvx_argp_is_set(&argp, "fps")) {
    decoder.setFramerate(mvx_argp_get_int(&argp, "fps", 0));
  }
  if (mvx_argp_is_set(&argp, "fw_timeout")) {
    decoder.setFWTimeout(mvx_argp_get_int(&argp, "fw_timeout", 0));
  }
  if (mvx_argp_is_set(&argp, "profiling")) {
    decoder.setProfiling(mvx_argp_get_int(&argp, "profiling", 0));
  }
  if (mvx_argp_is_set(&argp, "dsl_frame_width") &&
      mvx_argp_is_set(&argp, "dsl_frame_height")) {
    assert(!mvx_argp_is_set(&argp, "dsl_ratio_hor") &&
           !mvx_argp_is_set(&argp, "dsl_ratio_ver"));
    int width = mvx_argp_get_int(&argp, "dsl_frame_width", 0);
    int height = mvx_argp_get_int(&argp, "dsl_frame_height", 0);
    assert(2 <= width && 2 <= height);
    decoder.setDSLFrame(width, height);
  } else if (mvx_argp_is_set(&argp, "dsl_frame_width") ||
             mvx_argp_is_set(&argp, "dsl_frame_height")) {
    cerr << "Downscale frame width and height shoule be set together!" << endl;
  }

  if (mvx_argp_is_set(&argp, "dsl_ratio_hor") ||
      mvx_argp_is_set(&argp, "dsl_ratio_ver")) {
    assert(!mvx_argp_is_set(&argp, "dsl_frame_width") &&
           !mvx_argp_is_set(&argp, "dsl_frame_height"));
    int hor = mvx_argp_is_set(&argp, "dsl_ratio_hor")
                  ? mvx_argp_get_int(&argp, "dsl_ratio_hor", 0)
                  : 1;
    int ver = mvx_argp_is_set(&argp, "dsl_ratio_hor")
                  ? mvx_argp_get_int(&argp, "dsl_ratio_ver", 0)
                  : 1;
    decoder.setDSLRatio(hor, ver);
  }

  if (mvx_argp_is_set(&argp, "dsl_pos_mode")) {
    assert(mvx_argp_is_set(&argp, "dsl_ratio_hor") ||
           mvx_argp_is_set(&argp, "dsl_ratio_ver") ||
           mvx_argp_is_set(&argp, "dsl_frame_width") ||
           mvx_argp_is_set(&argp, "dsl_frame_height"));
    int mode = mvx_argp_get_int(&argp, "dsl_pos_mode", 0);
    if (mode < 0 || mode > 2) {
      mode = 0;
    }
    decoder.setDSLMode(mode);
  }

  decoder.setInterlaced(interlaced);
  decoder.setRotation(rotation);
  decoder.setDownScale(scale);
  decoder.setFrameCount(frames);

  ret = decoder.stream();

  float fps = decoder.getAverageFramerate();

  if (ret == 0) {
    if (md5_filename != NULL && md5ref_filename != NULL) {
      bool is_md5_ok = output->getMd5CheckResult();
      if (is_md5_ok) {
        printf(
            "-----[Test Result] MVX Decode MD5 Check PASS. Average Framerate: "
            "%.2f. \n",
            fps);
      } else {
        printf(
            "-----[Test Result] MVX Decode MD5 Check FAIL. Average Framerate: "
            "%.2f. \n",
            fps);
        ret = 1;
      }
    } else {
      printf("-----[Test Result] MVX Decode PASS. Average Framerate: %.2f.\n",
             fps);
    }
  } else {
    printf("-----[Test Result] MVX Decode FAIL. Average Framerate: %.2f.\n",
           fps);
  }

  if (md5_os) {
    md5_os->close();
    delete md5_os;
  }
  if (md5ref_is) {
    md5ref_is->close();
    delete md5ref_is;
  }

  is.close();
  os.close();

  delete inputFile;
  delete output;

  return ret;
}
