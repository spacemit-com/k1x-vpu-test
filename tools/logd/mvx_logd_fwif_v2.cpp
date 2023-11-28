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

/******************************************************************************
 * Includes
 ******************************************************************************/

#include <inttypes.h>

#include <iostream>

#include "fw_v2/mve_protocol_def.h"
#include "mvx_logd.hpp"

using namespace std;
using namespace MVX;

/******************************************************************************
 * Logd
 ******************************************************************************/

#if !defined(MVE_REQUEST_CODE_IDLE_ACK)
#define MVE_REQUEST_CODE_IDLE_ACK (1012)
#endif

const char *Logd::getSignalName(const uint32_t code) {
  switch (code) {
    case MVE_REQUEST_CODE_GO:
      return "GO";
    case MVE_REQUEST_CODE_STOP:
      return "STOP";
    case MVE_REQUEST_CODE_INPUT_FLUSH:
      return "INPUT_FLUSH";
    case MVE_REQUEST_CODE_OUTPUT_FLUSH:
      return "OUTPUT_FLUSH";
    case MVE_REQUEST_CODE_SWITCH:
      return "SWITCH";
    case MVE_REQUEST_CODE_PING:
      return "PING";
    case MVE_REQUEST_CODE_DUMP:
      return "DUMP";
    case MVE_REQUEST_CODE_JOB:
      return "JOB";
    case MVE_REQUEST_CODE_SET_OPTION:
      return "SET_OPTION";
    case MVE_REQUEST_CODE_IDLE_ACK:
      return "IDLE_ACK";
    case MVE_RESPONSE_CODE_SWITCHED_IN:
      return "SWITCHED_IN";
    case MVE_RESPONSE_CODE_SWITCHED_OUT:
      return "SWITCHED_OUT";
    case MVE_RESPONSE_CODE_SET_OPTION_CONFIRM:
      return "SET_OPTION_CONFIRM";
    case MVE_RESPONSE_CODE_JOB_DEQUEUED:
      return "JOB_DEQUEUED";
    case MVE_RESPONSE_CODE_INPUT:
      return "INPUT";
    case MVE_RESPONSE_CODE_OUTPUT:
      return "OUTPUT";
    case MVE_RESPONSE_CODE_INPUT_FLUSHED:
      return "INPUT_FLUSHED";
    case MVE_RESPONSE_CODE_OUTPUT_FLUSHED:
      return "OUTPUT_FLUSHED";
    case MVE_RESPONSE_CODE_PONG:
      return "PONG";
    case MVE_RESPONSE_CODE_ERROR:
      return "ERROR";
    case MVE_RESPONSE_CODE_STATE_CHANGE:
      return "STATE_CHANGE";
    case MVE_RESPONSE_CODE_DUMP:
      return "DUMP";
    case MVE_RESPONSE_CODE_IDLE:
      return "IDLE";
    case MVE_RESPONSE_CODE_FRAME_ALLOC_PARAM:
      return "FRAME_ALLOC_PARAM";
    case MVE_RESPONSE_CODE_SEQUENCE_PARAMETERS:
      return "SEQUENCE_PARAMETERS";
    case MVE_RESPONSE_CODE_EVENT:
      return "EVENT";
    case MVE_RESPONSE_CODE_SET_OPTION_FAIL:
      return "SET_OPTION_FAIL";
    case MVE_BUFFER_CODE_FRAME:
      return "FRAME";
    case MVE_BUFFER_CODE_BITSTREAM:
      return "BITSTREAM";
    case MVE_BUFFER_CODE_PARAM:
      return "PARAM";
    case MVE_BUFFER_CODE_GENERAL:
      return "GENERAL";
    case MVE_RESPONSE_CODE_DEBUG:
    case MVE_REQUEST_CODE_DEBUG:
      return "DEBUG";
    default:
      cerr << "Error: Firmware v2 unknown code. code=" << code << "." << endl;
      return "UNKNOWN";
  }
}

const char *Logd::getSetOptionName(const uint32_t index) {
  switch (index) {
    case MVE_SET_OPT_INDEX_NALU_FORMAT:
      return "NALU_FORMAT";
    case MVE_SET_OPT_INDEX_STREAM_ESCAPING:
      return "STREAM_ESCAPING";
    case MVE_SET_OPT_INDEX_PROFILE_LEVEL:
      return "PROFILE_LEVEL";
    case MVE_SET_OPT_INDEX_HOST_PROTOCOL_PRINTS:
      return "HOST_PROTOCOL_PRINTS";
    case MVE_SET_OPT_INDEX_PROFILING:
      return "PROFILING";
    case MVE_SET_OPT_INDEX_DISABLE_FEATURES:
      return "DISABLE_FEATURES";
    case MVE_SET_OPT_INDEX_IGNORE_STREAM_HEADERS:
      return "IGNORE_STREAM_HEADERS";
    case MVE_SET_OPT_INDEX_FRAME_REORDERING:
      return "FRAME_REORDERING";
    case MVE_SET_OPT_INDEX_INTBUF_SIZE:
      return "INTBUF_SIZE";
    case MVE_SET_OPT_INDEX_ENC_P_FRAMES:
      return "ENC_P_FRAMES";
    case MVE_SET_OPT_INDEX_ENC_B_FRAMES:
      return "ENC_B_FRAMES";
    case MVE_SET_OPT_INDEX_GOP_TYPE:
      return "GOP_TYPE";
    case MVE_SET_OPT_INDEX_INTRA_MB_REFRESH:
      return "INTRA_MB_REFRESH";
    case MVE_SET_OPT_INDEX_ENC_CONSTR_IPRED:
      return "ENC_CONSTR_IPRED";
    case MVE_SET_OPT_INDEX_ENC_ENTROPY_SYNC:
      return "ENC_ENTROPY_SYNC";
    case MVE_SET_OPT_INDEX_ENC_TEMPORAL_MVP:
      return "ENC_TEMPORAL_MVP";
    case MVE_SET_OPT_INDEX_TILES:
      return "TILES";
    case MVE_SET_OPT_INDEX_ENC_MIN_LUMA_CB_SIZE:
      return "ENC_MIN_LUMA_CB_SIZE";
    case MVE_SET_OPT_INDEX_ENC_MB_TYPE_ENABLE:
      return "ENC_MB_TYPE_ENABLE";
    case MVE_SET_OPT_INDEX_ENC_MB_TYPE_DISABLE:
      return "ENC_MB_TYPE_DISABLE";
    case MVE_SET_OPT_INDEX_ENC_H264_CABAC:
      return "ENC_H264_CABAC";
    case MVE_SET_OPT_INDEX_ENC_SLICE_SPACING:
      return "ENC_SLICE_SPACING";
    case MVE_SET_OPT_INDEX_ENC_VP9_PROB_UPDATE:
      return "ENC_VP9_PROB_UPDATE";
    case MVE_SET_OPT_INDEX_RESYNC_INTERVAL:
      return "RESYNC_INTERVAL";
    case MVE_SET_OPT_INDEX_HUFFMAN_TABLE:
      return "HUFFMAN_TABLE";
    case MVE_SET_OPT_INDEX_QUANT_TABLE:
      return "QUANT_TABLE";
    case MVE_SET_OPT_INDEX_ENC_EXPOSE_REF_FRAMES:
      return "ENC_EXPOSE_REF_FRAMES";
    case MVE_SET_OPT_INDEX_MBINFO_OUTPUT:
      return "MBINFO_OUTPUT";
    case MVE_SET_OPT_INDEX_MV_SEARCH_RANGE:
      return "MV_SEARCH_RANGE";
    case MVE_SET_OPT_INDEX_ENC_STREAM_BITDEPTH:
      return "ENC_STREAM_BITDEPTH";
    case MVE_SET_OPT_INDEX_ENC_STREAM_CHROMA_FORMAT:
      return "ENC_STREAM_CHROMA_FORMAT";
    case MVE_SET_OPT_INDEX_ENC_RGB_TO_YUV_MODE:
      return "ENC_RGB_TO_YUV_MODE";
    case MVE_SET_OPT_INDEX_ENC_BANDWIDTH_LIMIT:
      return "ENC_BANDWIDTH_LIMIT";
    case MVE_SET_OPT_INDEX_WATCHDOG_TIMEOUT:
      return "WATCHDOG_TIMEOUT";
    case MVE_SET_OPT_INDEX_ENC_CABAC_INIT_IDC:
      return "ENC_CABAC_INIT_IDC";
    case MVE_SET_OPT_INDEX_ENC_ADPTIVE_QUANTISATION:
      return "ENC_ADPTIVE_QUANTISATION";
    case MVE_SET_OPT_INDEX_QP_DELTA_I_P:
      return "QP_DELTA_I_P";
    case MVE_SET_OPT_INDEX_QP_DELTA_I_B_REF:
      return "QP_DELTA_I_B_REF";
    case MVE_SET_OPT_INDEX_QP_DELTA_I_B_NONREF:
      return "QP_DELTA_I_B_NONREF";
    case MVE_SET_OPT_INDEX_CB_QP_OFFSET:
      return "CB_QP_OFFSET";
    case MVE_SET_OPT_INDEX_CR_QP_OFFSET:
      return "CR_QP_OFFSET";
    case MVE_SET_OPT_INDEX_LAMBDA_SCALE:
      return "LAMBDA_SCALE";
    case MVE_SET_OPT_INDEX_ENC_MAX_NUM_CORES:
      return "ENC_MAX_NUM_CORES";
    case MVE_SET_OPT_INDEX_DEC_DOWNSCALE:
      return "DEC_DOWNSCALE";
    case MVE_SET_OPT_INDEX_ENC_LTR_MODE:
      return "ENC_LTR_MODE";
    case MVE_SET_OPT_INDEX_ENC_LTR_PERIOD:
      return "ENC_LTR_PERIOD";
    default:
      cerr << "Error: Firmware v2 unknown set option index. index=" << index
           << "." << endl;
      return "UNKNOWN";
  }
}

const char *Logd::getBufferParamName(const uint32_t type) {
  switch (type) {
    case MVE_BUFFER_PARAM_TYPE_QP:
      return "QP";
    case MVE_BUFFER_PARAM_TYPE_REGIONS:
      return "REGIONS";
    case MVE_BUFFER_PARAM_TYPE_DISPLAY_SIZE:
      return "DISPLAY_SIZE";
    case MVE_BUFFER_PARAM_TYPE_RANGE_MAP:
      return "RANGE_MAP";
    case MVE_BUFFER_PARAM_TYPE_FRAME_RATE:
      return "FRAME_RATE";
    case MVE_BUFFER_PARAM_TYPE_RATE_CONTROL:
      return "RATE_CONTROL";
    case MVE_BUFFER_PARAM_TYPE_QP_I:
      return "QP_I";
    case MVE_BUFFER_PARAM_TYPE_QP_P:
      return "QP_P";
    case MVE_BUFFER_PARAM_TYPE_QP_B:
      return "QP_B";
    case MVE_BUFFER_PARAM_TYPE_COLOUR_DESCRIPTION:
      return "COLOUR_DESCRIPTION";
    case MVE_BUFFER_PARAM_TYPE_FRAME_PACKING:
      return "FRAME_PACKING";
    case MVE_BUFFER_PARAM_TYPE_FRAME_FIELD_INFO:
      return "FRAME_FIELD_INFO";
    case MVE_BUFFER_PARAM_TYPE_GOP_RESET:
      return "GOP_RESET";
    case MVE_BUFFER_PARAM_TYPE_DPB_HELD_FRAMES:
      return "DPB_HELD_FRAMES";
    case MVE_BUFFER_PARAM_TYPE_CHANGE_RECTANGLES:
      return "CHANGE_RECTANGLES";
    case MVE_BUFFER_PARAM_TYPE_RATE_CONTROL_QP_RANGE:
      return "RATE_CONTROL_QP_RANGE";
    case MVE_BUFFER_PARAM_TYPE_RATE_CONTROL_HRD_BUF_SIZE:
      return "RATE_CONTROL_HRD_BUF_SIZE";
    case MVE_BUFFER_PARAM_TYPE_SEI_USER_DATA_UNREGISTERED:
      return "SEI_USER_DATA_UNREGISTERED";
    default:
      cerr << "Error: Firmware v2 unknown buffer param type. type=" << type
           << "." << endl;
      return "UNKNOWN";
  }
}

const char *Logd::getEventName(const uint32_t event) {
  switch (event) {
    case MVE_EVENT_ERROR_STREAM_CORRUPT:
      return "STREAM_CORRUPT";
    case MVE_EVENT_ERROR_STREAM_NOT_SUPPORTED:
      return "STREAM_NOT_SUPPORTED";
    case MVE_EVENT_PROCESSED:
      return "PROCESSED";
    case MVE_EVENT_REF_FRAME:
      return "REF_FRAME";
    case MVE_EVENT_TRACE_BUFFERS:
      return "TRACE_BUFFERS";
    default:
      cerr << "Error: Firmware v2 unknown event. event=" << event << "."
           << endl;
      return "UNKNOWN";
  }
}

const char *Logd::getNaluName(const uint32_t nalu) {
  switch (nalu) {
    case MVE_OPT_NALU_FORMAT_START_CODES:
      return "START_CODES";
    case MVE_OPT_NALU_FORMAT_ONE_NALU_PER_BUFFER:
      return "ONE_NALU_PER_BUFFER";
    case MVE_OPT_NALU_FORMAT_ONE_BYTE_LENGTH_FIELD:
      return "ONE_BYTE_LENGTH_FIELD";
    case MVE_OPT_NALU_FORMAT_TWO_BYTE_LENGTH_FIELD:
      return "TWO_BYTE_LENGTH_FIELD";
    case MVE_OPT_NALU_FORMAT_FOUR_BYTE_LENGTH_FIELD:
      return "FOUR_BYTE_LENGTH_FIELD";
    default:
      cerr << "Error: Firmware v2 unknown NALU type. nalu=" << nalu << "."
           << endl;
      return "UNKNOWN";
  }
}

void Logd::getSignalData(mve_msg_header *header, char *buf, size_t size) {
  size_t n = 0;

  /* Initialize buffer to empty string. */
  buf[0] = '\0';

  switch (header->code) {
    case MVE_REQUEST_CODE_GO:
    case MVE_REQUEST_CODE_STOP:
    case MVE_REQUEST_CODE_INPUT_FLUSH:
    case MVE_REQUEST_CODE_OUTPUT_FLUSH:
    case MVE_REQUEST_CODE_SWITCH:
    case MVE_REQUEST_CODE_PING:
    case MVE_REQUEST_CODE_DUMP:
    case MVE_REQUEST_CODE_IDLE_ACK:
    case MVE_RESPONSE_CODE_INPUT:
    case MVE_RESPONSE_CODE_OUTPUT:
    case MVE_RESPONSE_CODE_INPUT_FLUSHED:
    case MVE_RESPONSE_CODE_OUTPUT_FLUSHED:
    case MVE_RESPONSE_CODE_PONG:
    case MVE_RESPONSE_CODE_DUMP:
    case MVE_RESPONSE_CODE_IDLE:
    case MVE_REQUEST_CODE_DEBUG:
    case MVE_RESPONSE_CODE_DEBUG:
      break;
    case MVE_REQUEST_CODE_JOB: {
      struct mve_request_job *p = (struct mve_request_job *)(header + 1);
      n += scnprintf(&buf[n], size - n, "cores=%u, frames=%u, flags=0x%x",
                     p->cores, p->frames, p->flags);
      break;
    }
    case MVE_REQUEST_CODE_SET_OPTION: {
      struct mve_request_set_option *p =
          (struct mve_request_set_option *)(header + 1);
      n += scnprintf(&buf[n], size - n, "index=%s (%u)",
                     getSetOptionName(p->index), p->index);

      switch (p->index) {
        case MVE_SET_OPT_INDEX_NALU_FORMAT:
          n += scnprintf(&buf[n], size - n, ", nalu=%s (0x%x)",
                         getNaluName(p->data.arg), p->data.arg);
          break;
        case MVE_SET_OPT_INDEX_STREAM_ESCAPING:
        case MVE_SET_OPT_INDEX_HOST_PROTOCOL_PRINTS:
        case MVE_SET_OPT_INDEX_PROFILING:
        case MVE_SET_OPT_INDEX_DISABLE_FEATURES:
        case MVE_SET_OPT_INDEX_IGNORE_STREAM_HEADERS:
        case MVE_SET_OPT_INDEX_FRAME_REORDERING:
        case MVE_SET_OPT_INDEX_INTBUF_SIZE:
        case MVE_SET_OPT_INDEX_ENC_P_FRAMES:
        case MVE_SET_OPT_INDEX_ENC_B_FRAMES:
        case MVE_SET_OPT_INDEX_GOP_TYPE:
        case MVE_SET_OPT_INDEX_INTRA_MB_REFRESH:
        case MVE_SET_OPT_INDEX_ENC_CONSTR_IPRED:
        case MVE_SET_OPT_INDEX_ENC_ENTROPY_SYNC:
        case MVE_SET_OPT_INDEX_ENC_TEMPORAL_MVP:
        case MVE_SET_OPT_INDEX_ENC_MIN_LUMA_CB_SIZE:
        case MVE_SET_OPT_INDEX_ENC_MB_TYPE_ENABLE:
        case MVE_SET_OPT_INDEX_ENC_MB_TYPE_DISABLE:
        case MVE_SET_OPT_INDEX_ENC_H264_CABAC:
        case MVE_SET_OPT_INDEX_ENC_SLICE_SPACING:
        case MVE_SET_OPT_INDEX_ENC_VP9_PROB_UPDATE:
        case MVE_SET_OPT_INDEX_RESYNC_INTERVAL:
        case MVE_SET_OPT_INDEX_ENC_EXPOSE_REF_FRAMES:
        case MVE_SET_OPT_INDEX_MBINFO_OUTPUT:
        case MVE_SET_OPT_INDEX_ENC_STREAM_CHROMA_FORMAT:
        case MVE_SET_OPT_INDEX_WATCHDOG_TIMEOUT:
        case MVE_SET_OPT_INDEX_ENC_CABAC_INIT_IDC:
        case MVE_SET_OPT_INDEX_ENC_LTR_MODE:
        case MVE_SET_OPT_INDEX_ENC_LTR_PERIOD:
          n += scnprintf(&buf[n], size - n, ", arg=%u (0x%x)", p->data.arg,
                         p->data.arg);
          break;

        case MVE_SET_OPT_INDEX_PROFILE_LEVEL:
          n += scnprintf(
              &buf[n], size - n, ", profile_level={profile=%u, level=%u}",
              p->data.profile_level.profile, p->data.profile_level.level);
          break;
        case MVE_SET_OPT_INDEX_TILES:
          n += scnprintf(&buf[n], size - n, ", tiles={rows=%u, cols=%u}",
                         p->data.tiles.tile_rows, p->data.tiles.tile_cols);
          break;
        case MVE_SET_OPT_INDEX_HUFFMAN_TABLE:
          n += scnprintf(&buf[n], size - n, ", huffman_table={type=%u}",
                         p->data.huffman_table.type);
          break;
        case MVE_SET_OPT_INDEX_QUANT_TABLE:
          n += scnprintf(&buf[n], size - n, ", quant_table={type=%u}",
                         p->data.quant_table.type);
          break;
        case MVE_SET_OPT_INDEX_MV_SEARCH_RANGE:
          n += scnprintf(&buf[n], size - n, ", motion_vector={x=%u, y=%u}",
                         p->data.motion_vector_search_range.mv_search_range_x,
                         p->data.motion_vector_search_range.mv_search_range_y);
          break;
        case MVE_SET_OPT_INDEX_ENC_STREAM_BITDEPTH:
          n += scnprintf(&buf[n], size - n, ", bitdepth={luma=%u, chroma=%u}",
                         p->data.bitdepth.luma_bitdepth,
                         p->data.bitdepth.chroma_bitdepth);
          break;
        case MVE_SET_OPT_INDEX_DEC_DOWNSCALE:
          n += scnprintf(&buf[n], size - n, ", dsl frame={width=%u, height=%u}",
                         p->data.downscaled_frame.width,
                         p->data.downscaled_frame.height);
          break;
        default:
          break;
      }

      break;
    }
    case MVE_RESPONSE_CODE_SWITCHED_IN: {
      struct mve_response_switched_in *p =
          (struct mve_response_switched_in *)(header + 1);
      n += scnprintf(&buf[n], size - n, "core=%u", p->core);
      break;
    }
    case MVE_RESPONSE_CODE_SWITCHED_OUT: {
      struct mve_response_switched_out *p =
          (struct mve_response_switched_out *)(header + 1);
      n += scnprintf(&buf[n], size - n, "core=%u, reason=%u, sub_reason=%u",
                     p->core, p->reason, p->sub_reason);
      break;
    }
    case MVE_RESPONSE_CODE_SET_OPTION_CONFIRM:
      break;
    case MVE_RESPONSE_CODE_JOB_DEQUEUED: {
      struct mve_response_job_dequeued *p =
          (struct mve_response_job_dequeued *)(header + 1);
      n += scnprintf(&buf[n], size - n, "valid_job=%u", p->valid_job);
      break;
    }
    case MVE_RESPONSE_CODE_ERROR: {
      struct mve_response_error *p = (struct mve_response_error *)(header + 1);
      p->message[MVE_MAX_ERROR_MESSAGE_SIZE - 1] = '\0';
      n += scnprintf(&buf[n], size - n, "error_code=0x%x, message=\"%s\"",
                     p->error_code, p->message);
      break;
    }
    case MVE_RESPONSE_CODE_STATE_CHANGE: {
      struct mve_response_state_change *p =
          (struct mve_response_state_change *)(header + 1);
      n += scnprintf(&buf[n], size - n, "new_state=%u", p->new_state);
      break;
    }
    case MVE_RESPONSE_CODE_FRAME_ALLOC_PARAM: {
      struct mve_response_frame_alloc_parameters *p =
          (struct mve_response_frame_alloc_parameters *)(header + 1);
      n += scnprintf(&buf[n], size - n,
                     "width=%u, height=%u, afbc_alloc_bytes=%u, "
                     "afbc_width_in_superblocks=%u, mbinfo_alloc_bytes=%u, "
                     "dsl_frame_width=%u, dsl_frame_height=%u",
                     p->planar_alloc_frame_width, p->planar_alloc_frame_height,
                     p->afbc_alloc_bytes, p->afbc_width_in_superblocks,
                     p->mbinfo_alloc_bytes, p->dsl_frame_width,
                     p->dsl_frame_height);
      break;
    }
    case MVE_RESPONSE_CODE_SEQUENCE_PARAMETERS: {
      struct mve_response_sequence_parameters *p =
          (struct mve_response_sequence_parameters *)(header + 1);
      n += scnprintf(&buf[n], size - n,
                     "interlace=%u, chroma_format=%u, bitdepth_luma=%u, "
                     "bitdepth_chroma=%u, num_buffers_planar=%u, "
                     "num_buffers_afbc=%u, range_mapping_enabled=%u",
                     p->interlace, p->chroma_format, p->bitdepth_luma,
                     p->bitdepth_chroma, p->num_buffers_planar,
                     p->num_buffers_afbc, p->range_mapping_enabled);
      break;
    }
    case MVE_RESPONSE_CODE_EVENT: {
      struct mve_response_event *p = (struct mve_response_event *)(header + 1);
      n += scnprintf(&buf[n], size - n, "event_code=%s (%u)",
                     getEventName(p->event_code), p->event_code);
      switch (p->event_code) {
        case MVE_EVENT_ERROR_STREAM_CORRUPT:
        case MVE_EVENT_ERROR_STREAM_NOT_SUPPORTED:
          break;
        case MVE_EVENT_PROCESSED: {
          struct mve_event_processed *r = &p->event_data.event_processed;
          n += scnprintf(
              &buf[n], size - n,
              ", processed={pic_format=%u, parse_start_time=%u, "
              "parse_end_time=%u, parse_idle_time=%u, pipe_start_time=%u, "
              "pipe_end_time=%u, pipe_idle_time=%u, parser_coreid=%u, "
              "pipe_coreid=%u, bitstream_bits=%u, intermediate_buffer_size=%u, "
              "total_memory_allocated=%u, bus_read_bytes=%u, "
              "bus_write_bytes=%u, afbc_bytes=%u}",
              r->pic_format, r->parse_start_time, r->parse_end_time,
              r->parse_idle_time, r->pipe_start_time, r->pipe_end_time,
              r->pipe_idle_time, r->parser_coreid, r->pipe_coreid,
              r->bitstream_bits, r->intermediate_buffer_size,
              r->total_memory_allocated, r->bus_read_bytes, r->bus_write_bytes,
              r->afbc_bytes);
          break;
        }
        case MVE_EVENT_REF_FRAME: {
          struct mve_event_ref_frame *r = &p->event_data.event_ref_frame;
          n += scnprintf(
              &buf[n], size - n,
              ", ref_frame={addr=0x%x, width=%u, height=%u, mb_width=%u, "
              "mb_height=%u, left_crop=%u, top_crop=%u, frame_size=%u, "
              "display_order=%u, bit_width=%u}",
              r->ref_addr, r->ref_width, r->ref_height, r->ref_mb_width,
              r->ref_mb_height, r->ref_left_crop, r->ref_top_crop,
              r->ref_frame_size, r->ref_display_order, r->bit_width);
          break;
        }
        case MVE_EVENT_TRACE_BUFFERS:
          break;
        default:
          cerr
              << "Error: Firmware v2, response event, unknown event code. code="
              << p->event_code << "." << endl;
          break;
      }

      break;
    }
    case MVE_RESPONSE_CODE_SET_OPTION_FAIL: {
      struct mve_response_set_option_fail *p =
          (struct mve_response_set_option_fail *)(header + 1);
      p->message[sizeof(p->message) - 1] = '\0';
      n += scnprintf(&buf[n], size - n, "index=%u, message=\"%s\"", p->index,
                     p->message);
      break;
    }
    case MVE_BUFFER_CODE_FRAME: {
      struct mve_buffer_frame *p = (struct mve_buffer_frame *)(header + 1);
      n +=
          scnprintf(&buf[n], size - n,
                    "host_handle=0x%" PRIx64 ", user_data_tag=%" PRIx64
                    ", frame_flags=0x%x, width=%u, height=%u, format=0x%x",
                    p->host_handle, p->user_data_tag, p->frame_flags,
                    p->visible_frame_width, p->visible_frame_height, p->format);
      if (p->format & (1 << MVE_FORMAT_BF_A)) {
        struct mve_buffer_frame_afbc *r = &p->data.afbc;

        n += scnprintf(
            &buf[n], size - n,
            ", afbc={plane=[0x%x, 0x%x], alloc_bytes[%u, %u], cropx=%u, "
            "cropy=%u, afbc_width_in_superblocks=[%u, %u], afbc_params=0x%x}",
            r->plane[0], r->plane[1], r->alloc_bytes[0], r->alloc_bytes[1],
            r->cropx, r->cropy, r->afbc_width_in_superblocks[0],
            r->afbc_width_in_superblocks[1], r->afbc_params);
      } else {
        struct mve_buffer_frame_planar *r = &p->data.planar;

        n += scnprintf(
            &buf[n], size - n,
            ", planar={top=[0x%x, 0x%x, 0x%x], bot=[0x%x, 0x%x, 0x%x], "
            "stride=[%u, %u, %u], max_frame_width=%u, max_frame_height=%u}",
            r->plane_top[0], r->plane_top[1], r->plane_top[2], r->plane_bot[0],
            r->plane_bot[1], r->plane_bot[2], r->stride[0], r->stride[1],
            r->stride[2], r->max_frame_width, r->max_frame_height);
      }

      break;
    }
    case MVE_BUFFER_CODE_BITSTREAM: {
      struct mve_buffer_bitstream *p =
          (struct mve_buffer_bitstream *)(header + 1);
      n += scnprintf(&buf[n], size - n,
                     "host_handle=0x%" PRIx64 ", user_data_tag=%" PRIx64
                     ", bitstream_flags=0x%x, alloc_bytes=%u, offset=%u, "
                     "filled_len=%u, buf_addr=0x%x",
                     p->host_handle, p->user_data_tag, p->bitstream_flags,
                     p->bitstream_alloc_bytes, p->bitstream_offset,
                     p->bitstream_filled_len, p->bitstream_buf_addr);
      break;
    }
    case MVE_BUFFER_CODE_GENERAL: {
      struct mve_buffer_general *p = (struct mve_buffer_general *)(header + 1);
      n +=
          scnprintf(&buf[n], size - n,
                    "host_handle=0x%" PRIx64 ", user_data_tag=%" PRIx64
                    ", buffer_size=%u, config_size=%u, n_cols_minus1=%u, "
                    "n_rows_minus1=%u",
                    p->header.host_handle, p->header.user_data_tag,
                    p->header.buffer_size, p->header.config_size,
                    p->config.block_configs.blk_cfgs.rows_uncomp.n_cols_minus1,
                    p->config.block_configs.blk_cfgs.rows_uncomp.n_rows_minus1);
      break;
    }
    case MVE_BUFFER_CODE_PARAM: {
      struct mve_buffer_param *p = (struct mve_buffer_param *)(header + 1);
      int i;

      n += scnprintf(&buf[n], size - n, "type=%s (%u)",
                     getBufferParamName(p->type), p->type);

      switch (p->type) {
        case MVE_BUFFER_PARAM_TYPE_QP:
          n += scnprintf(&buf[n], size - n, ", qp=%u", p->data.qp.qp);
          break;
        case MVE_BUFFER_PARAM_TYPE_REGIONS:
          n += scnprintf(&buf[n], size - n, ", regions={n_regions=%u",
                         p->data.regions.n_regions);
          for (i = 0; i < p->data.regions.n_regions; ++i) {
            n += scnprintf(&buf[n], size - n,
                           ", {mbx_left=%u, mbx_right=%u, mby_top=%u, "
                           "mby_bottom=%u, qp_delta=%d}",
                           p->data.regions.region[i].mbx_left,
                           p->data.regions.region[i].mbx_right,
                           p->data.regions.region[i].mby_top,
                           p->data.regions.region[i].mby_bottom,
                           p->data.regions.region[i].qp_delta);
          }

          n += scnprintf(&buf[n], size - n, "}");
          break;
        case MVE_BUFFER_PARAM_TYPE_DISPLAY_SIZE:
          n += scnprintf(&buf[n], size - n,
                         ", display_size={width=%u, height=%u}",
                         p->data.display_size.display_width,
                         p->data.display_size.display_height);
          break;
        case MVE_BUFFER_PARAM_TYPE_RANGE_MAP:
          n += scnprintf(&buf[n], size - n,
                         ", range_map={luma_enabled=%u, luma_value=%u, "
                         "chroma_enabled=%u, chroma_value=%u}",
                         p->data.range_map.luma_map_enabled,
                         p->data.range_map.luma_map_value,
                         p->data.range_map.chroma_map_enabled,
                         p->data.range_map.chroma_map_value);
          break;
        case MVE_BUFFER_PARAM_TYPE_FRAME_RATE: {
          float frame_rate = (float)p->data.arg / (1 << 16);
          n += scnprintf(&buf[n], size - n, ", frame_rate=%u (%f)", p->data.arg,
                         frame_rate);
          break;
        }
        case MVE_BUFFER_PARAM_TYPE_RATE_CONTROL:
          n += scnprintf(
              &buf[n], size - n,
              ", rate_control={mode=%u, target_bitrate=%u, max_bitrate=%u}",
              p->data.rate_control.rate_control_mode,
              p->data.rate_control.target_bitrate,
              p->data.rate_control.maximum_bitrate);
          break;
        case MVE_BUFFER_PARAM_TYPE_RATE_CONTROL_QP_RANGE:
          n += scnprintf(&buf[n], size - n,
                         ", rate_control_qp_range={qp_min=%d, qp_max=%d}",
                         p->data.rate_control_qp_range.qp_min,
                         p->data.rate_control_qp_range.qp_max);
          break;
        case MVE_BUFFER_PARAM_TYPE_QP_I:
          n += scnprintf(&buf[n], size - n, ", qp_i=%u", p->data.qp.qp);
          break;
        case MVE_BUFFER_PARAM_TYPE_QP_P:
          n += scnprintf(&buf[n], size - n, ", qp_p=%u", p->data.qp.qp);
          break;
        case MVE_BUFFER_PARAM_TYPE_QP_B:
          n += scnprintf(&buf[n], size - n, ", qp_b=%u", p->data.qp.qp);
          break;
        case MVE_BUFFER_PARAM_TYPE_COLOUR_DESCRIPTION:
          n += scnprintf(&buf[n], size - n, ", color_description");
          break;
        case MVE_BUFFER_PARAM_TYPE_FRAME_PACKING:
          n += scnprintf(&buf[n], size - n, ", frame_packing");
          break;
        case MVE_BUFFER_PARAM_TYPE_GOP_RESET:
          n += scnprintf(&buf[n], size - n, ", gop_reset");
          break;
        case MVE_BUFFER_PARAM_TYPE_DPB_HELD_FRAMES:
          n +=
              scnprintf(&buf[n], size - n, ", dpb_held_frames=%u", p->data.arg);
          break;
        case MVE_BUFFER_PARAM_TYPE_CHANGE_RECTANGLES:
          n += scnprintf(&buf[n], size - n, ", change_rectangles");
          break;
        case MVE_BUFFER_PARAM_TYPE_RATE_CONTROL_HRD_BUF_SIZE:
          n += scnprintf(&buf[n], size - n, ", hdr_buf_size=%u", p->data.arg);
          break;
        case MVE_BUFFER_PARAM_TYPE_SEI_USER_DATA_UNREGISTERED:
          n += scnprintf(&buf[n], size - n, ", sei user data");
          break;
        case MVE_BUFFER_PARAM_TYPE_FRAME_FIELD_INFO:
          n += scnprintf(&buf[n], size - n,
                         ", frame filed info={pic_struct=%d, "
                         "source_scan_type=%d, duplicate_flag=%d}",
                         p->data.frame_field_info.pic_struct,
                         p->data.frame_field_info.source_scan_type,
                         p->data.frame_field_info.duplicate_flag);
          break;
        default:
          cerr << "Error: Firmware v2, buffer param, unknown type. type="
               << p->type << "." << endl;
          break;
      }

      break;
    }
    default: {
      cerr << "Error: Firmware v2 illegal code. code=" << header->code << "."
           << endl;
      break;
    }
  }
}

void Logd::getRpcData(mve_rpc_communication_area *rpc, char *buf, size_t size) {
  size_t n = 0;

  n +=
      scnprintf(&buf[n], size - n, "state=%s, call_id=%s, size=%u",
                getRpcState(rpc->state), getRpcCallId(rpc->call_id), rpc->size);

  if (rpc->state == MVE_RPC_STATE_PARAM) {
    switch (rpc->call_id) {
      case MVE_RPC_FUNCTION_DEBUG_PRINTF:
        rpc->params.debug_print.string[rpc->size] = '\0';
        rstrip(rpc->params.debug_print.string, "\n");
        n += scnprintf(&buf[n], size - n, ", print=\"%s\"",
                       rpc->params.debug_print.string);
        break;
      case MVE_RPC_FUNCTION_MEM_ALLOC:
        n += scnprintf(
            &buf[n], size - n,
            ", alloc={size=%u, max_size=%u, region=%u, log2_alignment=%u}",
            rpc->params.mem_alloc.size, rpc->params.mem_alloc.max_size,
            rpc->params.mem_alloc.region, rpc->params.mem_alloc.log2_alignment);
        break;
      case MVE_RPC_FUNCTION_MEM_RESIZE:
        n += scnprintf(&buf[n], size - n, ", resize={va=%u, new_size=%u}",
                       rpc->params.mem_resize.ve_pointer,
                       rpc->params.mem_resize.new_size);
        break;
      case MVE_RPC_FUNCTION_MEM_FREE:
        n += scnprintf(&buf[n], size - n, ", free={va=%u}",
                       rpc->params.mem_free.ve_pointer);
        break;
      default:
        break;
    }
  } else if (rpc->state == MVE_RPC_STATE_RETURN) {
    switch (rpc->call_id) {
      case MVE_RPC_FUNCTION_DEBUG_PRINTF:
      case MVE_RPC_FUNCTION_MEM_FREE:
        break;
      case MVE_RPC_FUNCTION_MEM_ALLOC:
      case MVE_RPC_FUNCTION_MEM_RESIZE:
        n += scnprintf(&buf[n], size - n, ", va=0x%x", rpc->params.data[0]);
        break;
      default:
        break;
    }
  }
}

const char *Logd::getRpcState(const uint32_t state) {
  switch (state) {
    case MVE_RPC_STATE_FREE:
      return "free";
    case MVE_RPC_STATE_PARAM:
      return "param";
    case MVE_RPC_STATE_RETURN:
      return "return";
    default:
      cerr << "Error: Firmware v2 illegal RPC state. state=" << state << "."
           << endl;
      return "UNKNOWN";
  }
}

const char *Logd::getRpcCallId(const uint32_t id) {
  switch (id) {
    case MVE_RPC_FUNCTION_DEBUG_PRINTF:
      return "printf";
    case MVE_RPC_FUNCTION_MEM_ALLOC:
      return "alloc";
    case MVE_RPC_FUNCTION_MEM_RESIZE:
      return "resize";
    case MVE_RPC_FUNCTION_MEM_FREE:
      return "free";
    default:
      cerr << "Error: Firmware v2 illegal RPC call id. id=" << id << "."
           << endl;
      return "UNKNOWN";
  }
}

/******************************************************************************
 * Logd text
 ******************************************************************************/

void LogdText::unpackV2(mvx_log_fwif *fwif, size_t length) {
  switch (fwif->channel) {
    case MVX_LOG_FWIF_CHANNEL_MESSAGE:
    case MVX_LOG_FWIF_CHANNEL_INPUT_BUFFER:
    case MVX_LOG_FWIF_CHANNEL_OUTPUT_BUFFER:
      unpackV2(reinterpret_cast<mve_msg_header *>(fwif + 1),
               length - sizeof(*fwif));
      break;
    case MVX_LOG_FWIF_CHANNEL_RPC:
      unpackV2(reinterpret_cast<mve_rpc_communication_area *>(fwif + 1),
               length - sizeof(*fwif));
      break;
    default:
      cerr << "Warning: Unsupported FWIF channel '" << fwif->channel << "'."
           << endl;
  }
}

void LogdText::unpackV2(mve_msg_header *msg, size_t length) {
  char data[500];

  /* Print signal name. */
  write("%s (%u)", getSignalName(msg->code), msg->code);

  /* Print signal data. */
  getSignalData(msg, data, sizeof(data));
  if (data[0] != '\0') {
    write("={%s}", data);
  }
}

void LogdText::unpackV2(mve_rpc_communication_area *rpc, size_t length) {
  char data[500] = {0};
  getRpcData(rpc, data, sizeof(data));
  write("RPC={%s}", data);
}

/******************************************************************************
 * Logd text
 ******************************************************************************/

void LogdJSON::unpackV2(mvx_log_fwif *fwif, size_t length) {
  switch (fwif->channel) {
    case MVX_LOG_FWIF_CHANNEL_MESSAGE:
    case MVX_LOG_FWIF_CHANNEL_INPUT_BUFFER:
    case MVX_LOG_FWIF_CHANNEL_OUTPUT_BUFFER:
      unpackV2(reinterpret_cast<mve_msg_header *>(fwif + 1),
               length - sizeof(*fwif));
      break;
    case MVX_LOG_FWIF_CHANNEL_RPC:
      unpackV2(reinterpret_cast<mve_rpc_communication_area *>(fwif + 1),
               length - sizeof(*fwif));
      break;
    default:
      cerr << "Warning: Unsupported FWIF channel '" << fwif->channel << "'."
           << endl;
  }
}

void LogdJSON::unpackV2(mve_msg_header *msg, size_t length) {
  char data[500];

  /* Print signal name. */
  writeJSON("signal", getSignalName(msg->code));

  /* Print signal data. */
  getSignalData(msg, data, sizeof(data));
  if (data[0] != '\0') {
    writeJSON("data", data);
  }
}

void LogdJSON::unpackV2(mve_rpc_communication_area *rpc, size_t length) {
  char data[500] = {0};
  getRpcData(rpc, data, sizeof(data));
  writeJSON("rpc", data);
}
