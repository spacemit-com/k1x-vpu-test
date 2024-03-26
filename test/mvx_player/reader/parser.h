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

#ifndef __C_PARSER_H__
#define __C_PARSER_H__

#include <assert.h>

#include "read_util.h"

class reader {
 public:
  enum result {
    RR_OK,                // Read operation OK (streamed data)
    RR_EOS,               // Read to end of file (returned data still valid)
    RR_EOP,               // Read to end of packet
    RR_EOP_FRAME,         // Read to end of frame
    RR_EOP_CODEC_CONFIG,  // Read to end of codec config
    RR_ERROR,
  };

  enum codec {
    RCODEC_UNKNOWN,
    RCODEC_VC1,
    RCODEC_VC1_SIMPLE_PROFILE,
    RCODEC_VC1_MAIN_PROFILE,
    RCODEC_VC1_ADVANCED_PROFILE,
    RCODEC_REAL_VIDEO,
    RCODEC_REAL_MEDIA_FORMAT,
    RCODEC_MPEG2,
    RCODEC_MPEG4,
    RCODEC_H263,
    RCODEC_H264,
    RCODEC_HEVC,
    RCODEC_VP8,
    RCODEC_VP9,
    RCODEC_JPEG,
    RCODEC_MBINFO,
    RCODEC_AVS,
    RCODEC_AVS2,
  };

 public:
  class packet_info {
   public:
    uint8_t prefix_size;
    bool codec_config;
    uint8_t *buf;
    uint32_t buf_bytes;

    ~packet_info() {}
    packet_info() {
      prefix_size = 0;
      codec_config = false;
      buf = NULL;
      buf_bytes = 0;
    }
    packet_info(uint8_t *buf, uint32_t buf_size)
        : buf(buf), buf_bytes(buf_size) {
      prefix_size = 0;
      codec_config = false;
    }
  };

 public:
  reader() {}
  virtual ~reader() {}

  codec get_codec_id(const std::string &name) {
    if (name == "mpeg2dec") {
      return reader::RCODEC_MPEG2;
    } else if (name == "jpegdec") {
      return reader::RCODEC_MPEG2;
    } else if (name == "h263dec" || name == "mpeg4dec") {
      return reader::RCODEC_MPEG4;
    } else if (name == "h264dec") {
      return reader::RCODEC_H264;
    } else if (name == "hevcdec") {
      return reader::RCODEC_HEVC;
    } else if (name == "vp8dec") {
      return reader::RCODEC_VP8;
    } else if (name == "vp9dec") {
      return reader::RCODEC_VP9;
    } else if (name == "vc1dec") {
      return reader::RCODEC_VC1;
    } else if (name == "rvdec") {
      return reader::RCODEC_REAL_VIDEO;
    } else if (name == "avsdec") {
      return reader::RCODEC_AVS;
    } else if (name == "avs2dec") {
      return reader::RCODEC_AVS2;
    }
    assert(0);
    return reader::RCODEC_UNKNOWN;
  }
};

class parser {
 public:
  class info {
   public:
    bool new_frame;
    bool config;
    bool slice;
    info() {
      new_frame = false;
      config = false;
      slice = false;
    }
  };
  virtual bool parse(reader::packet_info packet, info &inf) = 0;
  virtual ~parser() {}
  virtual void reset() {}
};

class h264_parser : public parser {
  struct sps_data {
    int log2_max_frame_num;
    bool frame_mbs_only_flag;
    int pic_order_cnt_type;
    int log2_max_pic_order_cnt_lsb;
    bool delta_pic_order_always_zero_flag;
    sps_data() {
      log2_max_frame_num = 0;
      frame_mbs_only_flag = false;
      pic_order_cnt_type = 0;
      log2_max_pic_order_cnt_lsb = 0;
      delta_pic_order_always_zero_flag = false;
    }
  };
  struct pps_data {
    int sps_id;
    bool pic_order_present_flag;
    pps_data() {
      sps_id = 0;
      pic_order_present_flag = false;
    }
  };

  sps_data sps[32];
  pps_data pps[256];
  int last_frame_num;
  int last_field_mode;
  int last_idr_pic_flag;
  int last_idr_pic_id;
  int last_nal_ref_idc;
  int last_pps_id;
  int last_pic_order_cnt_0;
  int last_pic_order_cnt_1;

 public:
  h264_parser() { reset(); }

  void reset() {
    last_frame_num = -1;
    last_field_mode = -1;
    last_idr_pic_flag = -1;
    last_idr_pic_id = -1;
    last_nal_ref_idc = -1;
    last_pps_id = -1;
    last_pic_order_cnt_0 = -1;
    last_pic_order_cnt_1 = -1;
  }

  bool parse(reader::packet_info packet, info &inf) {
    // const int buf_size = 256;
    // uint8_t buf[buf_size];
    // uint32_t bytes = packet.write_to_buffer(buf,32);
    uint32_t bytes = packet.buf_bytes <= 32 ? packet.buf_bytes : 32;
    bitreader b(packet.buf, bytes);

    b.read_bits(1);  // forbidden_zero_bit
    int nal_ref_idc = b.read_bits(2);
    int nal_unit_type = b.read_bits(5);

    inf.slice = false;

    // printf("*************************************** nal_unit_type
    // %d\n",nal_unit_type);
    switch (nal_unit_type) {
      case 1:
      case 5:  // slice
      {
        inf.slice = true;

        ue(b);  // first_mb_in_slice
        ue(b);  // slice_type
        int pps_id = ue(b);
        pps_data &p = pps[pps_id];
        sps_data &s = sps[p.sps_id];
        int log2_max_frame_num = s.log2_max_frame_num;

        int frame_num = b.read_bits(log2_max_frame_num);

        int field_mode = 0;
        if (!s.frame_mbs_only_flag) {
          if (b.read_bits(1))  // field_pic_flag
          {
            field_mode = 1;
            field_mode += b.read_bits(1);  // bottom_field_flag
          }
        }
        int idr_pic_flag = 0;
        int idr_pic_id = 0;
        if (nal_unit_type == 5) {
          idr_pic_flag = 1;
          idr_pic_id = ue(b);
        }
        int pic_order_cnt_type = s.pic_order_cnt_type;
        int pic_order_cnt_0 = 0;
        int pic_order_cnt_1 = 0;
        if (pic_order_cnt_type == 0) {
          pic_order_cnt_0 = b.read_bits(s.log2_max_pic_order_cnt_lsb);
          if (p.pic_order_present_flag && field_mode == 0) {
            pic_order_cnt_1 = se(b);
          }
        } else if (pic_order_cnt_type == 1 &&
                   !s.delta_pic_order_always_zero_flag) {
          pic_order_cnt_0 = se(b);
          if (p.pic_order_present_flag && field_mode == 0) {
            pic_order_cnt_1 = se(b);
          }
        }
        if (pps_id != last_pps_id || frame_num != last_frame_num ||
            field_mode != last_field_mode ||
            idr_pic_flag != last_idr_pic_flag ||
            idr_pic_id != last_idr_pic_id ||
            ((nal_ref_idc != 0) && (last_nal_ref_idc == 0)) ||
            ((nal_ref_idc == 0) && (last_nal_ref_idc != 0)) ||
            pic_order_cnt_0 != last_pic_order_cnt_0 ||
            pic_order_cnt_1 != last_pic_order_cnt_1) {
          inf.new_frame = true;
          // printf("******************************* new frame\n");
        }
        /*
        printf("field_mode=%d, %d\n", field_mode, last_field_mode);
        printf("idr_pic_flag=%d, %d\n", idr_pic_flag, last_idr_pic_flag);
        printf("idr_pic_id=%d, %d\n", idr_pic_id, last_idr_pic_id);
        printf("nal_ref_idc=%d, %d\n", nal_ref_idc, last_nal_ref_idc);
        printf("pps_id=%d, %d\n", pps_id, last_pps_id);
        printf("frame_num=%d, %d\n", frame_num, last_frame_num);
        printf("pic_order_cnt_0=%d, %d\n", pic_order_cnt_0,
        last_pic_order_cnt_0); printf("pic_order_cnt_1=%d, %d\n",
        pic_order_cnt_1, last_pic_order_cnt_1);
        */

        last_field_mode = field_mode;
        last_idr_pic_flag = idr_pic_flag;
        last_idr_pic_id = idr_pic_id;
        last_nal_ref_idc = nal_ref_idc;
        last_pps_id = pps_id;
        last_frame_num = frame_num;
        last_pic_order_cnt_0 = pic_order_cnt_0;
        last_pic_order_cnt_1 = pic_order_cnt_1;
        // printf("first_mb_in_slice %d, slice_type %d, pps_id %d, frame_num %d
        // pic_order_cnt_type %d pic_order_cnt_0 %d pic_order_cnt_1 %d
        // log2_max_pic_order_cnt_lsb %d \n",first_mb_in_slice,slice_type
        // ,pps_id,frame_num, pic_order_cnt_type,
        // pic_order_cnt_0,pic_order_cnt_1,s.log2_max_pic_order_cnt_lsb);
        break;
      }
      case 9: {
        /*access unit delimiter*/
        break;
      }
      case 6: {
        /*SEI NAL unit*/
        break;
      }
      case 8:  // PPS
      {
        inf.config = true;
        int pic_parameter_set_id = ue(b);
        int seq_parameter_set_id = ue(b);
        b.read_bits(1);  // entropy_coding_mode_flag

        /* bottom_field_pic_order_in_frame_present_flag */
        bool pic_order_present_flag = b.read_bits(1);

        pps[pic_parameter_set_id].sps_id = seq_parameter_set_id;
        pps[pic_parameter_set_id].pic_order_present_flag =
            pic_order_present_flag;
        // printf("PPS pic_parameter_set_id %d, pic_parameter_set_id
        // %d\n",pic_parameter_set_id,pic_parameter_set_id);
        break;
      }
      case 7:  // SPS
      {
        // printf("sps\n");
        inf.config = true;

        // bytes += packet.write_to_buffer(buf+32,buf_size-32);
        // b = bitreader(buf+1,bytes-1);
        b = bitreader(packet.buf + 1, packet.buf_bytes - 1);

        int profile_idc = b.read_bits(8);
        b.read_bits(8);  // constra_and_res
        b.read_bits(8);  // level_idc
        int sps_id = ue(b);

        // printf("profile_idc %d, cc %d, level %d, id
        // %d\n",profile_idc,constra_and_res,level_idc,sps_id);

        if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 ||
            profile_idc == 244 || profile_idc == 44 || profile_idc == 83 ||
            profile_idc == 86 || profile_idc == 118 || profile_idc == 128) {
          int chroma_format_idc = ue(b);

          if (chroma_format_idc == 3) {
            b.read_bits(1);  // separate_colour_plane_flag
          }
          ue(b);           // bit_depth_luma_minus8
          ue(b);           // bit_depth_chroma_minus8
          b.read_bits(1);  // qpprime_y_zero_transform_bypass_flag
          int seq_scaling_matrix_present_flag = b.read_bits(1);

          if (seq_scaling_matrix_present_flag) {
            for (int i = 0; i < ((chroma_format_idc != 3) ? 8 : 12) && !b.eos;
                 i++) {
              bool seq_scaling_list_present_flag = b.read_bits(1);
              if (seq_scaling_list_present_flag) {
                if (i < 6) {
                  scaling_list(b, 16);
                } else {
                  scaling_list(b, 64);
                }
              }
            }
          }
        }

        int log2_max_frame_num = ue(b) + 4;
        // printf("log2_max_frame_num %d\n",log2_max_frame_num);

        sps_data *s = &sps[sps_id];
        s->log2_max_frame_num = log2_max_frame_num;

        int pic_order_cnt_type = ue(b);

        if (pic_order_cnt_type == 0) {
          int log2_max_pic_order_cnt_lsb_minus4 = ue(b);
          s->log2_max_pic_order_cnt_lsb = log2_max_pic_order_cnt_lsb_minus4 + 4;
        } else if (pic_order_cnt_type == 1) {
          bool delta_pic_order_always_zero_flag = b.read_bits(1);
          s->delta_pic_order_always_zero_flag =
              delta_pic_order_always_zero_flag;
          se(b);  // offset_for_non_ref_pic
          se(b);  // offset_for_top_to_bottom_field
          int num_ref_frames_in_pic_order_cnt_cycle = ue(b);
          for (int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle && !b.eos;
               i++) {
            se(b);  // offset_for_ref_frame[i]
          }
        }
        ue(b);           // ax_num_ref_frames
        b.read_bits(1);  // gaps_in_frame_num_value_allowed_flag

        ue(b);  // pic_width_in_mbs_minus1
        ue(b);  // pic_height_in_map_units_minus1
        s->frame_mbs_only_flag = b.read_bits(1);

        s->pic_order_cnt_type = pic_order_cnt_type;
        // printf("s->frame_mbs_only_flag %d\n",s->frame_mbs_only_flag);

        break;
      }
      default: {
        break;
      }
    }
    return true;
  }

 private:
  uint32_t ue(bitreader &b) { return b.read_exp_golomb(); }

  int32_t se(bitreader &b) {
    uint32_t c = ue(b);
    if ((c & 1) == 0) {
      return -(c >> 1);
    } else {
      return 1 + (c >> 1);
    }
  }

  void scaling_list(bitreader &b, int sizeOfScalingList) {
    int lastScale = 8;
    int nextScale = 8;
    for (int j = 0; j < sizeOfScalingList && !b.eos; j++) {
      if (nextScale != 0) {
        int delta_scale = se(b);
        nextScale = (lastScale + delta_scale + 256) % 256;
      }
      int s = (nextScale == 0) ? lastScale : nextScale;
      lastScale = s;
    }
  }
};

class hevc_parser : public parser {
  // int find_new_frame_count;

 public:
  hevc_parser() {}

  bool parse(reader::packet_info packet, info &inf) {
    // uint8_t buf[32];
    // uint32_t r = packet.write_to_buffer(buf,32);
    // bitreader b(buf,r);
    uint32_t bytes = packet.buf_bytes <= 32 ? packet.buf_bytes : 32;
    bitreader b(packet.buf, bytes);

    int forbidden_zero_bit = b.read_bits(1);
    int nal_unit_type = b.read_bits(6);
    int nuh_layer_id = b.read_bits(6);
    int nuh_temporal_id = b.read_bits(3);

    (void)forbidden_zero_bit;
    (void)nuh_layer_id;
    (void)nuh_temporal_id;

    inf.slice = false;

    // printf("%d\n",nal_unit_type);
    switch (nal_unit_type) {
      case HEVC_NAL_TRAIL_N:
      case HEVC_NAL_TRAIL_R:
      case HEVC_NAL_TSA_N:
      case HEVC_NAL_TLA_R:
      case HEVC_NAL_STSA_N:
      case HEVC_NAL_STSA_R:
      case HEVC_NAL_RADL_N:
      case HEVC_NAL_RADL_R:
      case HEVC_NAL_RASL_N:
      case HEVC_NAL_RASL_R:
      case HEVC_NAL_BLA_W_LP:
      case HEVC_NAL_BLA_W_RADL:
      case HEVC_NAL_BLA_N_LP:
      case HEVC_NAL_IDR_W_RADL:
      case HEVC_NAL_IDR_N_LP:
      case HEVC_NAL_CRA: {
        // printf("Slice\n");
        inf.slice = true;

        uint32_t first_slice_segment_in_pic = b.read_bits(1);
        if (first_slice_segment_in_pic) {
          inf.new_frame = true;
        }
        break;
      }
      case HEVC_NAL_VPS:
        // printf("VPS\n");
        inf.config = true;
        break;
      case HEVC_NAL_SPS:
        // printf("SPS\n");
        inf.config = true;
        break;
      case HEVC_NAL_PPS:
        // printf("PPS\n");
        inf.config = true;
        break;
      default:
        // printf("NAL %d\n",nal_unit_type);
        break;
    }
    return true;
  }
  enum hevc_nal_unit_type {
    HEVC_NAL_TRAIL_N = 0,  // 0
    HEVC_NAL_TRAIL_R,      // 1
    HEVC_NAL_TSA_N,        // 2
    HEVC_NAL_TLA_R,        // 3
    HEVC_NAL_STSA_N,       // 4
    HEVC_NAL_STSA_R,       // 5
    HEVC_NAL_RADL_N,       // 6
    HEVC_NAL_RADL_R,       // 7
    HEVC_NAL_RASL_N,       // 8
    HEVC_NAL_RASL_R,       // 9

    HEVC_NAL_RSV_VCL_N10,
    HEVC_NAL_RSV_VCL_R11,
    HEVC_NAL_RSV_VCL_N12,
    HEVC_NAL_RSV_VCL_R13,
    HEVC_NAL_RSV_VCL_N14,
    HEVC_NAL_RSV_VCL_R15,

    HEVC_NAL_BLA_W_LP = 16,  // 16
    HEVC_NAL_BLA_W_RADL,     // 17
    HEVC_NAL_BLA_N_LP,       // 18
    HEVC_NAL_IDR_W_RADL,     // 19
    HEVC_NAL_IDR_N_LP,       // 20
    HEVC_NAL_CRA,            // 21

    HEVC_NAL_RSV_IRAP_VCL22,
    HEVC_NAL_RSV_IRAP_VCL23,
    HEVC_NAL_RSV_VCL24,
    HEVC_NAL_RSV_VCL25,
    HEVC_NAL_RSV_VCL26,
    HEVC_NAL_RSV_VCL27,
    HEVC_NAL_RSV_VCL28,
    HEVC_NAL_RSV_VCL29,
    HEVC_NAL_RSV_VCL30,
    HEVC_NAL_RSV_VCL31,

    HEVC_NAL_VPS = 32,     // 32
    HEVC_NAL_SPS,          // 33
    HEVC_NAL_PPS,          // 34
    HEVC_NAL_AUD,          // 35
    HEVC_NAL_EOS,          // 36
    HEVC_NAL_EOB,          // 37
    HEVC_NAL_FILLER_DATA,  // 38
    HEVC_NAL_PREFIX_SEI,   // 39
    HEVC_NAL_SUFFIX_SEI,   // 40
    HEVC_NAL_RSV_NVCL41,
    HEVC_NAL_RSV_NVCL42,
    HEVC_NAL_RSV_NVCL43,
    HEVC_NAL_RSV_NVCL44,
    HEVC_NAL_RSV_NVCL45,
    HEVC_NAL_RSV_NVCL46,
    HEVC_NAL_RSV_NVCL47,
    HEVC_NAL_INVALID = 64,
  };
};

class start_code_reader : public reader {
 protected:
  int last_start_code_len;
  uint64_t prev_offset;
  bool include_start_codes_in_packet;
  bool allow_start_codes_len_4;
  parser *dec;
  uint32_t start_code;
  uint32_t start_code_mask;
  bool seen_new_frame;
  codec codec_id;
  bool is_eos;
  uint8_t *bitstream_buf;
  uint32_t bitstream_buf_size;
  uint32_t bitstream_pos;
  uint32_t frame_cnt;

 public:
  start_code_reader(const std::string &name) {
    start_code = 0x00000001;
    start_code_mask = 0x00ffffff;
    allow_start_codes_len_4 = false;
    include_start_codes_in_packet = false;
    seen_new_frame = false;
    prev_offset = 0, is_eos = false;
    dec = NULL;
    bitstream_buf = NULL;
    bitstream_buf_size = 0;
    bitstream_pos = 0;
    frame_cnt = 0;

    codec_id = get_codec_id(name);
    switch (codec_id) {
      /*case RCODEC_VC1:
              set_metadata(RMETA_CODEC, RCODEC_VC1_ADVANCED_PROFILE);
              dec = new vc1_advanced_parser();
              break;
      case RCODEC_MPEG2:
              dec = new mpeg2_parser();
              break;
      case RCODEC_MPEG4:
              include_start_codes_in_packet = true;
              dec = new mpeg4_parser();
              break;*/
      case RCODEC_HEVC:
        allow_start_codes_len_4 = true;
        dec = new hevc_parser();
        break;
      case RCODEC_H264:
        allow_start_codes_len_4 = true;
        dec = new h264_parser();
        break;
      /*case RCODEC_REAL_VIDEO:
              dec = new rv_parser();
              break;*/
      default:
        break;
    }
  }

  start_code_reader(uint32_t name) {
    start_code = 0x00000001;
    start_code_mask = 0x00ffffff;
    allow_start_codes_len_4 = false;
    include_start_codes_in_packet = false;
    seen_new_frame = false;
    prev_offset = 0, is_eos = false;
    dec = NULL;
    bitstream_buf = NULL;
    bitstream_buf_size = 0;
    bitstream_pos = 0;
    frame_cnt = 0;

    // codec_id = get_codec_id(name);
    switch (name) {
      /*case RCODEC_VC1:
              set_metadata(RMETA_CODEC, RCODEC_VC1_ADVANCED_PROFILE);
              dec = new vc1_advanced_parser();
              break;
      case RCODEC_MPEG2:
              dec = new mpeg2_parser();
              break;
      case RCODEC_MPEG4:
              include_start_codes_in_packet = true;
              dec = new mpeg4_parser();
              break;*/
      case V4L2_PIX_FMT_HEVC:
        allow_start_codes_len_4 = true;
        dec = new hevc_parser();
        break;
      case V4L2_PIX_FMT_H264:
        allow_start_codes_len_4 = true;
        dec = new h264_parser();
        break;
      /*case RCODEC_REAL_VIDEO:
              dec = new rv_parser();
              break;*/
      default:
        break;
    }
  }
  virtual ~start_code_reader() {
    if (dec != NULL) {
      delete dec;
    }
  }

  void reset_bitstream_data(uint8_t *buffer, uint32_t buffer_size) {
    bitstream_buf = buffer;
    bitstream_buf_size = buffer_size;
    bitstream_pos = 0;
  }

  void get_remaining_bitstream_data(uint8_t *&remaining_data,
                                    uint32_t &remaining_size) {
    remaining_data = bitstream_buf + bitstream_pos;
    remaining_size = bitstream_buf_size - bitstream_pos;
  }

  void set_eos(bool is_eos) { this->is_eos = is_eos; }

  bool is_parser_valid() { return (NULL != dec); }

  int get_start_code_len(uint32_t prefix) {
    if (allow_start_codes_len_4 && (prefix & 0xff000000) == 0) {
      return 4;
    } else {
      return 3;
    }
  }

  bool seek_prefix_in_buffer(uint8_t *buffer, uint32_t buffer_size,
                             uint32_t &found_pos, uint32_t &check) {
    uint32_t mask = start_code_mask;
    uint32_t match = start_code;

    for (uint32_t i = 0; i < buffer_size; i++) {
      uint8_t temp = buffer[i];

      check = (check << 8) | temp;

      // printf("%d: %x %x\n",i,temp, check);
      if ((check & mask) == match) {
        found_pos = i + 1;
        return true;
      }
    }
    return false;
  }

  result find_one_frame(uint32_t &slice_cnt, uint32_t &frame_start_pos,
                        uint32_t &frame_size) {
    result rtn = RR_ERROR;
    bool ret = false;
    uint32_t prefix = ~0u;
    uint32_t found_pos0 = 0;
    uint32_t found_pos1 = 0;
    int start_code_len0;
    int start_code_len;
    uint8_t *buf;
    uint32_t buf_size;
    uint32_t off = 0;
    uint32_t nal_size = 0;
    uint32_t frame_end_pos = 0;
    uint8_t *buffer = bitstream_buf + bitstream_pos;
    uint32_t buffer_size = bitstream_buf_size - bitstream_pos;
    uint32_t new_frame_cnt = 0;

    frame_size = 0;
    slice_cnt = 0;

    prefix = ~0u;
    ret = seek_prefix_in_buffer(buffer, buffer_size, found_pos0, prefix);
    if (false == ret) {
      rtn = is_eos ? RR_EOS : RR_EOP;
      return rtn;
    }
    start_code_len0 = get_start_code_len(prefix);
    frame_start_pos = found_pos0 - start_code_len0 + off;
    off += found_pos0;
    buf = buffer + off;
    buf_size = buffer_size - off;

    dec->reset();

    while (1) {
      prefix = ~0u;
      found_pos1 = 0;
      ret = seek_prefix_in_buffer(buf, buf_size, found_pos1, prefix);
      if (false == ret && !is_eos) {
        rtn = RR_EOP;
        break;
      } else if (false == ret && is_eos) {
        found_pos1 = buf_size;
        start_code_len = 0;
      } else if (true == ret) {
        start_code_len = get_start_code_len(prefix);
      }

      nal_size = found_pos1 - start_code_len;
      if (1 > nal_size) {
        rtn = is_eos ? RR_EOS : RR_EOP;
        break;
      }
      packet_info current_packet = packet_info(buf, nal_size);
      parser::info info;
      dec->parse(current_packet, info);

      if (info.new_frame ||
          (1 == new_frame_cnt &&
           !info.slice)) {  // the (!info.slice) case:
                            // Frame,Frame,Frame,SPS,PPS,SEI ...
        new_frame_cnt++;
      }

      if (2 <= new_frame_cnt) {  // find new frame
        bitstream_pos += frame_end_pos;
        frame_size = frame_end_pos - frame_start_pos;
        frame_cnt++;
        rtn = frame_size ? RR_EOP_FRAME : RR_ERROR;
        // printf("new_frame, frame_size=%d, bitstream_pos=%d.\n", frame_size,
        // bitstream_pos);
        break;
      }

      if (info.slice) {
        slice_cnt++;
      }

      if (info.config ||
          (!info.slice)) {  // the (!info.config && !info.slice) case : SEI
        // current_packet.prefix_size = start_code_len0;
        // current_packet.codec_config = info.config;

        frame_end_pos = off + nal_size;
        bitstream_pos += frame_end_pos;
        frame_size = frame_end_pos - frame_start_pos;
        rtn = info.config ? RR_EOP_CODEC_CONFIG : RR_OK;
        // printf("%s, frame_size=%d, bitstream_pos=%d.\n", info.config ?
        // "config" : "other", frame_size, bitstream_pos);
        break;
      }

      if (false == ret && is_eos) {
        frame_end_pos = off + nal_size;
        bitstream_pos += frame_end_pos;
        frame_size = frame_end_pos - frame_start_pos;
        rtn = RR_EOS;
        break;
      }

      // continue to check the next packet
      frame_end_pos = off + nal_size;
      off += found_pos1;
      buf = buffer + off;
      buf_size = buffer_size - off;
      // printf("off=%d, frame_end_pos=%d.\n", off, frame_end_pos);
    }

    return rtn;
  }

  bool find_all_frames(uint32_t &find_frames, uint32_t &start_pos,
                       uint32_t &all_frames_size) {
    result rtn = RR_ERROR;
    uint32_t frame_start_pos = 0;
    uint32_t frame_size = 0;
    uint32_t slice_cnt = 0;
    bool seeking = true;

    find_frames = 0;
    all_frames_size = 0;
    while (seeking) {
      rtn = find_one_frame(slice_cnt, frame_start_pos, frame_size);
      switch (rtn) {
        case RR_OK:
        case RR_EOP_CODEC_CONFIG:
        case RR_EOP_FRAME:
          if (0 == all_frames_size) {
            start_pos = frame_start_pos;
          }
          all_frames_size += frame_size;

          if (RR_EOP_FRAME == rtn) {
            find_frames++;
            // printf("frame_cnt=%d, find_frames=%d, slice_cnt=%d,
            // (%d..+%dB).\n", frame_cnt, find_frames, slice_cnt,
            // frame_start_pos, frame_size);
            printf("frame_cnt=%d, slice_cnt=%d, (%d..+%dB).\n", frame_cnt,
                   slice_cnt, frame_start_pos, frame_size);
          } else if (RR_EOP_CODEC_CONFIG == rtn) {
            printf("CODEC_CONFIG (%d..+%dB).\n", frame_start_pos, frame_size);
          } else {
            printf("OTHER_INFO (%d..+%dB).\n", frame_start_pos, frame_size);
          }
          break;

        case RR_EOS:
          seeking = false;
          break;

        case RR_EOP:
          seeking = false;
          break;

        case RR_ERROR:
        default:
          printf("ERROR (%d..+%dB).\n", frame_start_pos, frame_size);
          find_frames = 0;
          seeking = false;
          break;
      }
    }

    return (find_frames ? true : false);
  }
};

#endif /*__C_PARSER_H__*/
