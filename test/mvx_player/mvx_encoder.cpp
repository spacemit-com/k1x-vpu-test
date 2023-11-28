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
  const char *roi_file = NULL;
  const char *epr_file = NULL;

  mvx_argp_construct(&argp);
  mvx_argp_add_opt(&argp, '\0', "dev", true, 1, "/dev/video0", "Device.");
  mvx_argp_add_opt(&argp, 'i', "inputformat", true, 1, "yuv420",
                   "Pixel format.");
  mvx_argp_add_opt(&argp, 'o', "outputformat", true, 1, "h264",
                   "Output pixel format.");
  mvx_argp_add_opt(&argp, 'f', "format", true, 1, "ivf",
                   "Output container format. [ivf, raw]");
  mvx_argp_add_opt(&argp, 'w', "width", true, 1, "1920", "Width.");
  mvx_argp_add_opt(&argp, 'h', "height", true, 1, "1080", "Height.");
  mvx_argp_add_opt(&argp, 's', "stride", true, 1, "1", "Stride alignment.");
  mvx_argp_add_opt(&argp, 0, "mirror", true, 1, "0",
                   "mirror, 1 : horizontal; 2 : vertical.");
  mvx_argp_add_opt(&argp, 0, "roi_cfg", true, 1, NULL, "ROI config file.");
  mvx_argp_add_opt(&argp, 0, "frames", true, 1, "0", "nr of frames to process");
  mvx_argp_add_opt(&argp, 0, "epr_cfg", true, 1, NULL,
                   "Encode Parameter Records config file name");
  mvx_argp_add_opt(&argp, 0, "rate_control", true, 1, "off",
                   "Selects rate control type, constant/variable/off");
  mvx_argp_add_opt(
      &argp, 0, "target_bitrate", true, 1, "0",
      "If rate control is enabled, this option sets target bitrate");
  mvx_argp_add_opt(
      &argp, 0, "max_bitrate", true, 1, "0",
      "If rate control is enabled, this option sets maximum bitrate");
  mvx_argp_add_opt(&argp, 0, "gop", true, 1, "0",
                   "GOP: 0 is None, 1 is Bidi, 2 is Low delay, 3 is Pyramid");
  mvx_argp_add_opt(&argp, 0, "pframes", true, 1, "0", "Number of P frames");
  mvx_argp_add_opt(&argp, 0, "bframes", true, 1, "0", "Number of B frames");
  mvx_argp_add_opt(&argp, 'n', "minqp", true, 1, "0", "H264 min QP");
  mvx_argp_add_opt(&argp, 'm', "maxqp", true, 1, "51", "H264 max QP");
  mvx_argp_add_opt(&argp, 't', "tier", true, 1, "2", "Profile.");
  mvx_argp_add_opt(&argp, 'l', "level", true, 1, "1", "Level.");
  mvx_argp_add_opt(&argp, 'v', "fps", true, 1, "24", "Frame rate.");
  mvx_argp_add_opt(&argp, 0, "ecm", true, 1, "1", "0 is CAVLC, 1 is CABAC");
  mvx_argp_add_opt(&argp, 0, "bitdepth", true, 1, "8", "Set other bitdepth");
  mvx_argp_add_opt(&argp, 'q', "fixedqp", true, 1, "20",
                   "H264 fixed QP for I P B frames. If it is combined with -x "
                   "then the value will later be increased with 2.");
  mvx_argp_add_opt(&argp, 0, "qpi", true, 1, "20",
                   "H264 fixed QP for I frames.");
  mvx_argp_add_opt(&argp, 0, "qpb", true, 1, "20",
                   "H264 fixed QP for B frames.");
  mvx_argp_add_opt(&argp, 0, "qpp", true, 1, "20",
                   "H264 fixed QP for P frames.");
  mvx_argp_add_opt(&argp, 0, "crop_left", true, 1, "0",
                   "encoder SPS crop param, left offset");
  mvx_argp_add_opt(&argp, 0, "crop_right", true, 1, "0",
                   "encoder SPS crop param, right offset");
  mvx_argp_add_opt(&argp, 0, "crop_top", true, 1, "0",
                   "encoder SPS crop param, top offset");
  mvx_argp_add_opt(&argp, 0, "crop_bottom", true, 1, "0",
                   "encoder SPS crop param, bottom offset");

  mvx_argp_add_opt(&argp, 0, "colour_description_range", true, 1, "0",
                   "VUI param: Colour description; range\n"
                   "\t\tValue: 0=Unspecified, 1=Limited, 2=Full");
  mvx_argp_add_opt(&argp, 0, "colour_primaries", true, 1, "0",
                   "VUI param: Colour description; colour primaries (0-255, "
                   "see hevc spec. E.3.1)\n"
                   "\t\tValue: 0=Unspecified, 1=BT709, 2=BT470M, 3=BT601_625, "
                   "4=T601_525, 5=GENERIC_FILM, 6=BT2020");
  mvx_argp_add_opt(
      &argp, 0, "transfer_characteristics", true, 1, "0",
      "VUI param: Colour description; transfer characteristics (0-255, see "
      "hevc spec. E.3.1)\n"
      "\t\tValue: 0=Unspecified, 1=LINEAR, 2=SRGB, 3=SMPTE170M, 4=GAMMA22, "
      "5=GAMMA28, 6=ST2084, 7=HLG, 8=SMPTE240M, 9=XVYCC, 10=BT1361, 11=ST428");
  mvx_argp_add_opt(&argp, 0, "matrix_coeff", true, 1, "0",
                   "VUI param: Colour description; matrix coefficients (0-255, "
                   "see hevc spec. E.3.1)\n"
                   "\t\tValue: 0=Unspecified, 1=BT709, 2=BT470M, 3=BT601, "
                   "4=SMPTE240M, 5=T2020, 6=BT2020Constant");
  mvx_argp_add_opt(&argp, 0, "time_scale", true, 1, "0",
                   "VUI param: vui_time_scale");
  mvx_argp_add_opt(&argp, 0, "num_units_in_tick", true, 1, "0",
                   "VUI param: vui_num_units_in_tick");
  mvx_argp_add_opt(&argp, 0, "aspect_ratio_idc", true, 1, "0",
                   "VUI param: aspect_ratio_idc. [0,255]");
  mvx_argp_add_opt(&argp, 0, "sar_width", true, 1, "0", "VUI param: sar_width");
  mvx_argp_add_opt(&argp, 0, "sar_height", true, 1, "0",
                   "VUI param: sar_height");
  mvx_argp_add_opt(
      &argp, 0, "video_format", true, 1, "0",
      "VUI param: video_format. (0-5, see hevc spec. E.3.1)\n"
      "\t\tValue: 0=Component, 2=PAL, 2=NTSC, 3=SECAM, 4=MAC, 5=Unspecified");

  mvx_argp_add_opt(&argp, 0, "sei_mastering_display", true, 1, "0",
                   "SEI param : mastering display 's parameters");
  mvx_argp_add_opt(&argp, 0, "sei_content_light", true, 1, "0",
                   "SEI param : sei_content_light");
  mvx_argp_add_opt(&argp, 0, "sei_user_data_unregistered", true, 1, "0",
                   "SEI param : user data unregisterd");
  mvx_argp_add_opt(
      &argp, 0, "hrd_buffer_size", true, 1, "0",
      "Hypothetical Reference Decoder buffer size relative to the bitrate (in "
      "seconds) for rate control"
      "\t\tValue: should bigger than target_bitrate/fps on normal case");
  mvx_argp_add_opt(
      &argp, 0, "ltr_mode", true, 1, "0",
      "encoder long term reference mode,range from 1 to 8 (inclusive)\n"
      "\t\t1: LDP-method-1 | 2: LDP-method-2 | 3: LDB-method-1 | 4: "
      "LDB-method-2\n"
      "\t\t5: BiDirection-method-1 | 6: BiDirection-method-2 | 7: "
      "Pyrimid-method-1 | 8: Pyrimid-method-2\n");
  mvx_argp_add_opt(
      &argp, 0, "ltr_period", true, 1, "0",
      "encoder long term reference period, range from 2 to 254 (inclusive)");
  mvx_argp_add_pos(&argp, "input", false, 1, "", "Input file.");
  mvx_argp_add_pos(&argp, "output", true, 1, "", "Output file.");
  mvx_argp_add_opt(&argp, 0, "trystop", true, 0, "0",
                   "Try if Encoding Stop Command exixts");
  mvx_argp_add_opt(&argp, 0, "restart_interval", true, 1, "-1",
                   "JPEG restart interval.");
  mvx_argp_add_opt(&argp, 0, "quality", true, 1, "0",
                   "JPEG compression quality. [1-100, 0 - default]");
  mvx_argp_add_opt(&argp, 0, "preload", true, 0, "0",
                   "preload the first 5 yuv frames to memory.");
  mvx_argp_add_opt(&argp, 0, "fw_timeout", true, 1, "5",
                   "timeout value[secs] for watchdog timeout. range: 5~60.");
  mvx_argp_add_opt(
      &argp, 0, "profiling", true, 1, "0",
      "enable profiling for bandwidth statistics . 0:disable; 1:enable.");

  ret = mvx_argp_parse(&argp, argc - 1, &argv[1]);
  if (ret != 0) {
    mvx_argp_help(&argp, argv[0]);
    return 1;
  }

  inputFormat = Codec::to4cc(mvx_argp_get(&argp, "inputformat", 0));
  if (inputFormat == 0) {
    fprintf(stderr, "Error: Illegal frame format. format=%s.\n",
            mvx_argp_get(&argp, "inputformat", 0));
    return 1;
  }

  outputFormat = Codec::to4cc(mvx_argp_get(&argp, "outputformat", 0));
  if (outputFormat == 0) {
    fprintf(stderr, "Error: Illegal bitstream format. format=%s.\n",
            mvx_argp_get(&argp, "outputformat", 0));
    return 1;
  }

  bool preload = mvx_argp_is_set(&argp, "preload");

  ifstream is(mvx_argp_get(&argp, "input", 0));
  Input *inputFile;
  ifstream *roi_stream = NULL;
  ifstream *epr_stream = NULL;
  if (Codec::isAFBC(inputFormat)) {
    inputFile =
        new InputAFBC(is, inputFormat, mvx_argp_get_int(&argp, "width", 0),
                      mvx_argp_get_int(&argp, "height", 0), preload);
  } else {
    if (mvx_argp_get_int(&argp, "stride", 0) == 0) {
      fprintf(stderr, "Error: Illegal stride 0.\n");
      return 1;
    }
    roi_file = mvx_argp_get(&argp, "roi_cfg", 0);
    if (roi_file) {
      printf("roi config filename is < %s >.\n", roi_file);
      roi_stream = new ifstream(roi_file, ios::binary);
      if (NULL == roi_stream) {
        fprintf(stderr, "Error: (NULL == roi_stream).\n");
        return 1;
      }
      inputFile = new InputFileFrameWithROI(
          is, inputFormat, mvx_argp_get_int(&argp, "width", 0),
          mvx_argp_get_int(&argp, "height", 0),
          mvx_argp_get_int(&argp, "stride", 0), *roi_stream);
    } else {
      epr_file = mvx_argp_get(&argp, "epr_cfg", 0);
      if (epr_file) {
        printf("epr config filename is < %s >.\n", epr_file);
        epr_stream = new ifstream(epr_file, ios::binary);
        if (NULL == epr_stream) {
          fprintf(stderr, "Error: (NULL == epr_stream).\n");
          return 1;
        }
        inputFile = new InputFileFrameWithEPR(
            is, inputFormat, mvx_argp_get_int(&argp, "width", 0),
            mvx_argp_get_int(&argp, "height", 0),
            mvx_argp_get_int(&argp, "stride", 0), *epr_stream, outputFormat);
      } else {
        inputFile = new InputFileFrame(
            is, inputFormat, mvx_argp_get_int(&argp, "width", 0),
            mvx_argp_get_int(&argp, "height", 0),
            mvx_argp_get_int(&argp, "stride", 0), preload);
      }
    }
  }

  int mirror = mvx_argp_get_int(&argp, "mirror", 0);
  int frames = mvx_argp_get_int(&argp, "frames", 0);
  ofstream os(mvx_argp_get(&argp, "output", 0));
  Output *outputFile;

  if (string(mvx_argp_get(&argp, "format", 0)).compare("ivf") == 0) {
    outputFile =
        new OutputIVF(os, outputFormat, mvx_argp_get_int(&argp, "width", 0),
                      mvx_argp_get_int(&argp, "height", 0));
  } else if (string(mvx_argp_get(&argp, "format", 0)).compare("raw") == 0) {
    outputFile = new OutputFile(os, outputFormat);
  } else {
    cerr << "Error: Unsupported container format. format="
         << mvx_argp_get(&argp, "format", 0) << "." << endl;
    return 1;
  }

  Encoder encoder(mvx_argp_get(&argp, "dev", 0), *inputFile, *outputFile);
  if (mvx_argp_is_set(&argp, "trystop")) {
    encoder.tryStopCmd(true);
  }
  if (mvx_argp_is_set(&argp, "restart_interval")) {
    encoder.setJPEGRefreshInterval(
        mvx_argp_get_int(&argp, "restart_interval", 0));
  }
  if (mvx_argp_is_set(&argp, "quality")) {
    encoder.setJPEGQuality(mvx_argp_get_int(&argp, "quality", 0));
  }
  encoder.setMirror(mirror);
  encoder.setFrameCount(frames);
  encoder.setRateControl(mvx_argp_get(&argp, "rate_control", 0),
                         mvx_argp_get_int(&argp, "target_bitrate", 0),
                         mvx_argp_get_int(&argp, "max_bitrate", 0));
  if (mvx_argp_is_set(&argp, "pframes")) {
    encoder.setPFrames(mvx_argp_get_int(&argp, "pframes", 0));
  }
  if (mvx_argp_is_set(&argp, "bframes")) {
    encoder.setBFrames(mvx_argp_get_int(&argp, "bframes", 0));
  }
  if (mvx_argp_is_set(&argp, "maxqp")) {
    encoder.setH264MaxQP(mvx_argp_get_int(&argp, "maxqp", 0));
  }
  if (mvx_argp_is_set(&argp, "minqp")) {
    encoder.setH264MinQP(mvx_argp_get_int(&argp, "minqp", 0));
  }
  if (mvx_argp_is_set(&argp, "tier")) {
    encoder.setProfile(mvx_argp_get_int(&argp, "tier", 0));
  }
  if (mvx_argp_is_set(&argp, "level")) {
    encoder.setLevel(mvx_argp_get_int(&argp, "level", 0));
  }
  if (mvx_argp_is_set(&argp, "fps")) {
    encoder.setFramerate(mvx_argp_get_int(&argp, "fps", 0));
  }
  if (mvx_argp_is_set(&argp, "bitdepth")) {
    encoder.setH264Bitdepth(mvx_argp_get_int(&argp, "bitdepth", 0));
  }
  if (mvx_argp_is_set(&argp, "ecm")) {
    encoder.setH264EntropyCodingMode(mvx_argp_get_int(&argp, "ecm", 0));
  }
  if (mvx_argp_is_set(&argp, "fixedqp")) {
    encoder.setH264FixedQP(mvx_argp_get_int(&argp, "fixedqp", 0));
  }
  if (mvx_argp_is_set(&argp, "qpi")) {
    encoder.setH264FixedQPI(mvx_argp_get_int(&argp, "qpi", 0));
  }
  if (mvx_argp_is_set(&argp, "qpb")) {
    encoder.setH264FixedQPB(mvx_argp_get_int(&argp, "qpb", 0));
  }
  if (mvx_argp_is_set(&argp, "qpp")) {
    encoder.setH264FixedQPP(mvx_argp_get_int(&argp, "qpp", 0));
  }
  if (mvx_argp_is_set(&argp, "crop_left")) {
    encoder.setCropLeft(mvx_argp_get_int(&argp, "crop_left", 0));
  }
  if (mvx_argp_is_set(&argp, "crop_right")) {
    encoder.setCropRight(mvx_argp_get_int(&argp, "crop_right", 0));
  }
  if (mvx_argp_is_set(&argp, "crop_top")) {
    encoder.setCropTop(mvx_argp_get_int(&argp, "crop_top", 0));
  }
  if (mvx_argp_is_set(&argp, "crop_bottom")) {
    encoder.setCropBottom(mvx_argp_get_int(&argp, "crop_bottom", 0));
  }
  if (mvx_argp_is_set(&argp, "fw_timeout")) {
    encoder.setFWTimeout(mvx_argp_get_int(&argp, "fw_timeout", 0));
  }
  if (mvx_argp_is_set(&argp, "profiling")) {
    encoder.setProfiling(mvx_argp_get_int(&argp, "profiling", 0));
  }
  if (mvx_argp_is_set(&argp, "colour_description_range") ||
      mvx_argp_is_set(&argp, "colour_primaries") ||
      mvx_argp_is_set(&argp, "transfer_characteristics") ||
      mvx_argp_is_set(&argp, "matrix_coeff") ||
      mvx_argp_is_set(&argp, "time_scale") ||
      mvx_argp_is_set(&argp, "num_units_in_tick") ||
      mvx_argp_is_set(&argp, "aspect_ratio_idc") ||
      mvx_argp_is_set(&argp, "sar_width") ||
      mvx_argp_is_set(&argp, "sar_height") ||
      mvx_argp_is_set(&argp, "video_format") ||
      mvx_argp_is_set(&argp, "sei_mastering_display") ||
      mvx_argp_is_set(&argp, "sei_content_light")) {
    struct v4l2_mvx_color_desc color_desc;
    memset(&color_desc, 0, sizeof(color_desc));
    color_desc.range = mvx_argp_get_int(&argp, "colour_description_range", 0);
    color_desc.primaries = mvx_argp_get_int(&argp, "colour_primaries", 0);
    color_desc.transfer =
        mvx_argp_get_int(&argp, "transfer_characteristics", 0);
    color_desc.matrix = mvx_argp_get_int(&argp, "matrix_coeff", 0);
    color_desc.time_scale = mvx_argp_get_int(&argp, "time_scale", 0);
    color_desc.num_units_in_tick =
        mvx_argp_get_int(&argp, "num_units_in_tick", 0);
    color_desc.aspect_ratio_idc =
        mvx_argp_get_int(&argp, "aspect_ratio_idc", 0);
    color_desc.sar_width = mvx_argp_get_int(&argp, "sar_width", 0);
    color_desc.sar_height = mvx_argp_get_int(&argp, "sar_height", 0);
    color_desc.video_format = mvx_argp_get_int(&argp, "video_format", 0);
    if (mvx_argp_is_set(&argp, "sei_mastering_display")) {
      uint32_t x[3];
      uint32_t y[3];
      uint32_t wp_x, wp_y;
      uint32_t max_dml, min_dml;
      if (10 == sscanf(mvx_argp_get(&argp, "sei_mastering_display", 0),
                       "SEI_MS=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &x[0], &y[0],
                       &x[1], &y[1], &x[2], &y[2], &wp_x, &wp_y, &max_dml,
                       &min_dml)) {
        color_desc.display.r.x = x[0];
        color_desc.display.r.y = y[0];
        color_desc.display.g.x = x[1];
        color_desc.display.g.y = y[1];
        color_desc.display.b.x = x[2];
        color_desc.display.b.y = y[2];
        color_desc.display.w.x = wp_x;
        color_desc.display.w.y = wp_y;
        color_desc.display.luminance_min = min_dml;
        color_desc.display.luminance_max = max_dml;
        color_desc.flags |=
            V4L2_BUFFER_PARAM_COLOUR_FLAG_MASTERING_DISPLAY_DATA_VALID;
      }
    }
    if (mvx_argp_is_set(&argp, "sei_content_light")) {
      uint32_t max_cl;
      uint32_t min_pal;
      if (2 == sscanf(mvx_argp_get(&argp, "sei_content_light", 0),
                      "SEI_CL=%d,%d", &max_cl, &min_pal)) {
        color_desc.content.luminance_average = min_pal;
        color_desc.content.luminance_max = max_cl;
        color_desc.flags |=
            V4L2_BUFFER_PARAM_COLOUR_FLAG_CONTENT_LIGHT_DATA_VALID;
      }
    }
    encoder.setVuiColourDesc(&color_desc);
  }
  if (mvx_argp_is_set(&argp, "sei_user_data_unregistered")) {
    const char *user_data =
        mvx_argp_get(&argp, "sei_user_data_unregistered", 0);
    char *data_loc;
    int user_data_len;
    uint32_t uuid_l0, uuid_l1, uuid_l2, uuid_l3;
    struct v4l2_sei_user_data sei_user_data;
    user_data_len = strlen(user_data);
    if (user_data_len > (256 - 1)) {
      cout << "ERROR: host input error, user_data_unregistered string info "
              "larger than 256"
           << endl;
      ;
    }
    data_loc = strstr(const_cast<char *>(user_data), "+");
    if (data_loc == NULL) {
      cout << "ERROR: host input error, user_data_unregistered do not have "
              "seperate character : +"
           << endl;
    }
    if (data_loc - user_data != 35) {
      cout << "ERROR: host input error, user_data_unregistered's uuid is not "
              "16 bytes, or format error"
           << endl;
    }
    memset(&sei_user_data, 0, sizeof(sei_user_data));
    if (4 == sscanf(user_data, "%x-%x-%x-%x+", &uuid_l3, &uuid_l2, &uuid_l1,
                    &uuid_l0)) {
      sei_user_data.uuid[0] = uuid_l3 >> 24;
      sei_user_data.uuid[1] = (uuid_l3 >> 16) & 0xff;
      sei_user_data.uuid[2] = (uuid_l3 >> 8) & 0xff;
      sei_user_data.uuid[3] = uuid_l3 & 0xff;
      sei_user_data.uuid[4] = uuid_l2 >> 24;
      sei_user_data.uuid[5] = (uuid_l2 >> 16) & 0xff;
      sei_user_data.uuid[6] = (uuid_l2 >> 8) & 0xff;
      sei_user_data.uuid[7] = uuid_l2 & 0xff;
      sei_user_data.uuid[8] = uuid_l1 >> 24;
      sei_user_data.uuid[9] = (uuid_l1 >> 16) & 0xff;
      sei_user_data.uuid[10] = (uuid_l1 >> 8) & 0xff;
      sei_user_data.uuid[11] = uuid_l1 & 0xff;
      sei_user_data.uuid[12] = uuid_l0 >> 24;
      sei_user_data.uuid[13] = (uuid_l0 >> 16) & 0xff;
      sei_user_data.uuid[14] = (uuid_l0 >> 8) & 0xff;
      sei_user_data.uuid[15] = uuid_l0 & 0xff;
    } else {
      cout << "ERROR: invalid wrong userdata format" << endl;
    }
    for (int i = 36; i < user_data_len; i++) {
      sei_user_data.user_data[i - 36] = user_data[i];
    }
    sei_user_data.user_data_len = user_data_len - 36;
    sei_user_data.flags = V4L2_BUFFER_PARAM_USER_DATA_UNREGISTERED_VALID;
    encoder.setSeiUserData(&sei_user_data);
  }
  if (mvx_argp_is_set(&argp, "hrd_buffer_size")) {
    encoder.setHRDBufferSize(mvx_argp_get_int(&argp, "hrd_buffer_size", 0));
  }
  if (mvx_argp_is_set(&argp, "ltr_mode")) {
    int ltr_mode = mvx_argp_get_int(&argp, "ltr_mode", 0);
    bool is_gop_set = mvx_argp_is_set(&argp, "gop");
    int gop = 0;
    if (is_gop_set) {
      gop = mvx_argp_get_int(&argp, "gop", 0);
    }
    if (ltr_mode >= 1 && ltr_mode <= 4) {
      if (!(is_gop_set && gop == 2)) {
        cerr << "(ltr_mode >= 1 && ltr_mode <= 4), but gop_type is not "
                "MVE_OPT_GOP_TYPE_LOW_DELAY!"
             << endl;
        return 1;
      }
    }
    if (ltr_mode == 5 || ltr_mode == 6) {
      if (!(is_gop_set && gop == 1)) {
        cerr << "(ltr_mode == 5 or 6), but gop_type is not "
                "MVE_OPT_GOP_TYPE_BIDIRECTIONAL!"
             << endl;
        return 1;
      }
    }
    if (ltr_mode == 7 || ltr_mode == 8) {
      if (!(is_gop_set && gop == 3)) {
        cerr << "(ltr_mode == 7 or 8), but gop_type is not "
                "MVE_OPT_GOP_TYPE_PYRAMID!"
             << endl;
        return 1;
      }
    }
    if (mvx_argp_is_set(&argp, "ltr_period")) {
      int ltr_period = mvx_argp_get_int(&argp, "ltr_period", 0);
      switch (ltr_mode) {
        case 1: {
          if (ltr_period < 1 || ltr_period > 255) {
            cerr << "ltr_period can not less than 1 or greater than 255 under "
                    "long term LDP method 1!"
                 << endl;
            return 1;
          }
          break;
        }
        case 2: {
          if (ltr_period < 2 || ltr_period > 255) {
            cerr << "ltr_period can not less than 2 or greater than 255 under "
                    "long term LDP method 2!"
                 << endl;
            return 1;
          }
          break;
        }
        case 3:
        case 5: {
          if (!mvx_argp_is_set(&argp, "bframes") || ltr_period < 1 ||
              ltr_period > 255 / (mvx_argp_get_int(&argp, "bframes", 0) + 1)) {
            cerr << "--ltr_period can not less than 1 or greater than "
                    "255/(options_p->bframes.val+1), and --bframes must be "
                    "assigned under long term LDB method 3 or 5!"
                 << endl;
            return 1;
          }
          break;
        }
        case 4:
        case 6: {
          if (!mvx_argp_is_set(&argp, "bframes") || ltr_period < 2 ||
              ltr_period > 255 / (mvx_argp_get_int(&argp, "bframes", 0) + 1)) {
            cerr << "--ltr_period can not less than 1 or greater than "
                    "255/(options_p->bframes.val+1), and --bframes must be "
                    "assigned under long term LDB method 4 or 6!"
                 << endl;
            return 1;
          }
          break;
        }
        case 7: {
          if (ltr_period < 1 || ltr_period > 63) {
            cerr << "ltr_period can not less than 1 or greater than 63 under "
                    "long term LDP method 7!"
                 << endl;
            return 1;
          }
          break;
        }
        case 8: {
          if (ltr_period < 2 || ltr_period > 63) {
            cerr << "ltr_period can not less than 1 or greater than 63 under "
                    "long term LDP method 8!"
                 << endl;
            return 1;
          }
          break;
        }
        default: {
          cerr << "ltr_mode have been assinged with a wrong value!" << endl;
          return 1;
        }
      }
    }
    encoder.setLongTermRef(mvx_argp_get_int(&argp, "ltr_mode", 0),
                           mvx_argp_get_int(&argp, "ltr_period", 0));
  } else if (mvx_argp_is_set(&argp, "ltr_period")) {
    cerr << "ltr_period have been set, but ltr_mode do not assinged!" << endl;
    return 1;
  }
  if (mvx_argp_is_set(&argp, "gop")) {
    encoder.setH264GOPType(mvx_argp_get_int(&argp, "gop", 0));
  }

  int result = encoder.stream();

  float fps = encoder.getAverageFramerate();
  if (result == 0) {
    printf("-----[Test Result] MVX Encode PASS. Average Framerate: %.2f.\n",
           fps);
  } else {
    printf("-----[Test Result] MVX Encode FAIL. Average Framerate: %.2f.\n",
           fps);
  }

  is.close();
  os.close();
  if (roi_stream != NULL) {
    roi_stream->close();
    delete roi_stream;
  }
  if (epr_stream != NULL) {
    epr_stream->close();
    delete epr_stream;
  }

  delete inputFile;
  delete outputFile;

  return result;
}
