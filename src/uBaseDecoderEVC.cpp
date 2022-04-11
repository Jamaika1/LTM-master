// The copyright in this software is being made available under the BSD
// License, included below. This software may be subject to other third party
// and contributor rights, including patent rights, and no such rights are
// granted under this license.
//
// Copyright (c) 2022, ISO/IEC
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//  * Neither the name of the ISO/IEC nor the names of its contributors may
//    be used to endorse or promote products derived from this software without
//    specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//

#include <algorithm>
#include <assert.h>
#include <climits>
#include <cmath>
#include <memory.h>
#include <stdexcept>

#include "Misc.hpp"
#include "uBaseDecoderEVC.h"

#if defined(__GNUC__)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#endif

#define SLICE_B 0
#define SLICE_P 1
#define SLICE_I 2

namespace vnova {
namespace utility {
namespace {

BaseDecNalUnitType::Enum uFromEVC(uint8_t type) {
	switch (type) {
	case EVCNalType::NONIDR_NUT:
	case EVCNalType::IDR_NUT:
		return BaseDecNalUnitType::Slice;
	case EVCNalType::SPS_NUT:
		return BaseDecNalUnitType::SPS;
	case EVCNalType::PPS_NUT:
		return BaseDecNalUnitType::PPS;
#if defined(EVC_FDIS)
	case EVCNalType::FD_NUT:
		return BaseDecNalUnitType::Filler;
#endif
	case EVCNalType::SEI_NUT:
		return BaseDecNalUnitType::SEI;
	}

	return BaseDecNalUnitType::Unknown;
}

uint32_t uOffsetForNalUnitHeader(const uint8_t *nal) {
	return 0;
}

} // namespace

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// EVC
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

BaseDecoderEVC::SPS::SPS() { memset(this, 0, sizeof(SPS)); }

BaseDecoderEVC::PPS::PPS() { memset(this, 0, sizeof(PPS)); }

BaseDecoderEVC::SliceHeader::SliceHeader() { memset(this, 0, sizeof(SliceHeader)); }

BaseDecoderEVC::BaseDecoderEVC() : m_nal_type(0), m_sps(nullptr), m_pps(nullptr) {}

bool BaseDecoderEVC::ParseNalUnit(const uint8_t *nal, uint32_t nalLength) {
	bool bSuccess = true;

	uint32_t offset = 0;

	// [forbidden-zero:1, nal_type:6, layer_id_top:1]
	m_nal_type = ((nal[offset] >> 1) & 0x3F) - 1;
	// [layer_id:5, temporal_id_plus1:3]
	m_temporal_id = nal[offset + 1] >> 5;

	offset += 2;

	BaseDecNalUnitType::Enum nalType = uFromEVC(m_nal_type);

	nal += offset;
	nalLength -= offset;

	m_currentNalPayload.clear();
	m_currentNalPayload.insert(m_currentNalPayload.end(), nal, nal + nalLength);

	m_currentBits = *m_currentNalPayload.data();
	m_remainingBits = 8;
	m_byteOffset = 0;

	switch (nalType) {
	case BaseDecNalUnitType::VPS:
		bSuccess = ParseVPS();
		break;
	case BaseDecNalUnitType::PPS:
		bSuccess = ParsePPS();
		break;
	case BaseDecNalUnitType::SPS:
		bSuccess = ParseSPS();
		break;
	case BaseDecNalUnitType::Slice:
		if ((bSuccess = ParseSliceHeader(m_nal_type))) {
			HandlePictureOrderCount(m_nal_type, m_temporal_id);
		}
		break;
	case BaseDecNalUnitType::EOS:
		break;
	default:
		break;
	}

	return bSuccess;
}

BaseDecPictType::Enum BaseDecoderEVC::GetBasePictureType() const {
	switch (m_slice_header.slice_type) {
	case SLICE_P:
		return BaseDecPictType::P;
	case SLICE_B:
		return BaseDecPictType::B;
	case SLICE_I:
		return BaseDecPictType::I;
	default:
		break;
	}

	return BaseDecPictType::Unknown;
}

BaseDecNalUnitType::Enum BaseDecoderEVC::GetBaseNalUnitType() const { 
	return uFromEVC(m_nal_type);
}

int32_t BaseDecoderEVC::GetQP() const {
	// XXX
	return 30;
}

uint32_t BaseDecoderEVC::GetNalType() const { return m_nal_type; }

uint32_t BaseDecoderEVC::GetPictureWidth() const {
	if (!m_sps)
		return 0;

	return m_sps->pic_width_in_luma_samples;
}

uint32_t BaseDecoderEVC::GetPictureHeight() const {
	if (!m_sps) {
		return 0;
	}

	return m_sps->pic_height_in_luma_samples;
}

uint32_t BaseDecoderEVC::GetBitDepthLuma() const {
	if (!m_sps)
		return 0;

	return m_sps->bit_depth_luma;
}

uint32_t BaseDecoderEVC::GetBitDepthChroma() const {
	if (!m_sps)
		return 0;

	return m_sps->bit_depth_chroma;
}

uint32_t BaseDecoderEVC::GetChromaFormatIDC() const {
	if (!m_sps)
		return 0;

	return m_sps->chroma_format_idc;
}

int64_t BaseDecoderEVC::GetPictureOrderCount() const { return m_poc_val; }

bool BaseDecoderEVC::GetDPBCanRefresh() const {
	CHECK(0);
	return false;
}

uint8_t BaseDecoderEVC::GetMaxNumberOfReorderFrames() const {
	CHECK(0);
	return 0;
}

uint32_t BaseDecoderEVC::GetFrameRate() const {
	CHECK(0);
	return 0;
}

uint32_t BaseDecoderEVC::GetTemporalId() const {
	return m_temporal_id;
}

void BaseDecoderEVC::HandlePictureOrderCount(uint32_t nal_unit_type, uint32_t nuh_temporal_id) {

	if (!m_sps->tool_pocs) {
		CHECK(0); // XXX Not handled
	} else {
		if (nal_unit_type == EVCNalType::IDR_NUT) {
			m_slice_header.poc_lsb = 0;
			m_poc_val = 0;
		} else {
			int poc_msb, poc_lsb, max_poc_lsb, prev_poc_lsb, prev_poc_msb;

			poc_lsb = m_slice_header.poc_lsb;
			max_poc_lsb = 1 << (m_sps->log2_max_pic_order_cnt_lsb);
			prev_poc_lsb = m_prev_poc_val & (max_poc_lsb - 1);
			prev_poc_msb = m_prev_poc_val - prev_poc_lsb;
			if ((poc_lsb < prev_poc_lsb) && ((prev_poc_lsb - poc_lsb) >= (max_poc_lsb / 2)))
				poc_msb = prev_poc_msb + max_poc_lsb;
			else if ((poc_lsb > prev_poc_lsb) && ((poc_lsb - prev_poc_lsb) > (max_poc_lsb / 2)))
				poc_msb = prev_poc_msb - max_poc_lsb;
			else
				poc_msb = prev_poc_msb;

			m_poc_val = poc_msb + poc_lsb;

			if (nuh_temporal_id == 0)
				m_prev_poc_val = m_poc_val;
		}
	}
}

bool BaseDecoderEVC::ParseSPS() {

	uint32_t seq_parameter_set_id = ReadUE();
	if (seq_parameter_set_id >= EVCMaxSPSCount) {
		ERR("spsID out of range %d", seq_parameter_set_id);
		return false;
	}
	m_sps = &m_sps_set[seq_parameter_set_id];
	m_sps->seq_parameter_set_id = seq_parameter_set_id;

	m_sps->profile_idc = ReadBits(8);
	m_sps->level_idc = ReadBits(8);
	m_sps->toolset_idc_h = ReadBits(32);
	m_sps->toolset_idc_l = ReadBits(32);
	m_sps->chroma_format_idc = ReadUE();
	m_sps->pic_width_in_luma_samples = ReadUE();
	m_sps->pic_height_in_luma_samples = ReadUE();
	m_sps->bit_depth_luma = ReadUE() + 8;
	m_sps->bit_depth_chroma = ReadUE() + 8;
	m_sps->sps_btt_flag = ReadFlag();
	if (m_sps->sps_btt_flag) {
		m_sps->log2_ctu_size = ReadUE() + 5;
		m_sps->log2_min_cb_size = ReadUE() + 2;
		m_sps->log2_diff_ctu_max_14_cb_size = ReadUE();
		m_sps->log2_diff_ctu_max_tt_cb_size = ReadUE();
		m_sps->log2_diff_min_cb_min_tt_cb_size = ReadUE() + 2;
	}
	m_sps->sps_suco_flag = ReadFlag();
	if (m_sps->sps_suco_flag) {
		m_sps->log2_diff_ctu_size_max_suco_cb_size = ReadUE();
		m_sps->log2_diff_max_suco_min_suco_cb_size = ReadUE();
	}

	m_sps->tool_admvp = ReadFlag();
	if (m_sps->tool_admvp) {
		m_sps->tool_affine = ReadFlag();
		m_sps->tool_amvr = ReadFlag();
		m_sps->tool_dmvr = ReadFlag();
		m_sps->tool_mmvd = ReadFlag();
		m_sps->tool_hmvp = ReadFlag();
	}

	m_sps->tool_eipd = ReadFlag();
	if (m_sps->tool_eipd) {
		m_sps->ibc_flag = ReadFlag();
		if (m_sps->ibc_flag) {
			m_sps->ibc_log_max_size = ReadUE() + 2;
		}
	}

	m_sps->tool_cm_init = ReadFlag();
	if (m_sps->tool_cm_init) {
		m_sps->tool_adcc = ReadFlag();
	}
	m_sps->tool_iqt = ReadFlag();
	if (m_sps->tool_iqt) {
		m_sps->tool_ats = ReadFlag();
	}
	m_sps->tool_addb = ReadFlag();
	m_sps->tool_alf = ReadFlag();
	m_sps->tool_htdf = ReadFlag();
	m_sps->tool_rpl = ReadFlag();
	m_sps->tool_pocs = ReadFlag();
	m_sps->dquant_flag = ReadFlag();
	m_sps->tool_dra = ReadFlag();

	if (m_sps->tool_pocs) {
		m_sps->log2_max_pic_order_cnt_lsb = ReadUE() + 4;
	}

	if (!m_sps->tool_rpl || !m_sps->tool_pocs) {
		m_sps->log2_sub_gop_length = ReadUE();
		if (m_sps->log2_sub_gop_length == 0) {
			m_sps->log2_ref_pic_gap_length = ReadUE();
		}
	}
	if (!m_sps->tool_rpl) {
		m_sps->max_num_ref_pics = ReadUE();
	} else {
		m_sps->sps_max_dec_pic_buffering = ReadUE() + 1;
		m_sps->long_term_ref_pics_flag = ReadFlag();
		m_sps->rpl1_same_as_rpl0_flag = ReadFlag();
		m_sps->num_ref_pic_lists_in_sps0 = ReadUE();
	}

	// Ignore rest
	// ...

	return true;
}

bool BaseDecoderEVC::ParsePPS() {
	uint32_t pps_pic_parameter_set_id = ReadUE();

	if (pps_pic_parameter_set_id >= EVCMaxPPSCount) {
		ERR("ppsID out of range %d", pps_pic_parameter_set_id);
		return false;
	}

	m_pps = &m_pps_set[pps_pic_parameter_set_id];
	m_pps->pic_parameter_set_id = pps_pic_parameter_set_id;
	m_pps->seq_parameter_set_id = ReadUE();

	m_pps->num_ref_idx_default_active[0] = ReadUE() + 1;
	m_pps->num_ref_idx_default_active[1] = ReadUE() + 1;
	m_pps->additional_lt_poc_lsb_len = ReadUE();
	m_pps->rpl1_idx_present_flag = ReadFlag();
	m_pps->single_tile_in_pic_flag = ReadFlag();
	if (!m_pps->single_tile_in_pic_flag) {
		m_pps->num_tile_columns = ReadUE() + 1;
		m_pps->num_tile_rows = ReadUE() + 1;
		m_pps->uniform_tile_spacing_flag = ReadFlag();
		if (!m_pps->uniform_tile_spacing_flag) {
			for (unsigned i = 0; i < m_pps->num_tile_columns - 1; ++i)
				m_pps->tile_column_width[i] = ReadUE() + 1;
			for (unsigned i = 0; i < m_pps->num_tile_rows - 1; ++i)
				m_pps->tile_row_height[i] = ReadUE() + 1;
		}
		m_pps->loop_filter_across_tiles_enabled_flag = ReadFlag();
		m_pps->tile_offset_lens = ReadUE() + 1;
	} else {
		m_pps->num_tile_columns = 1;
		m_pps->num_tile_rows = 1;
		m_pps->tile_offset_lens = 1;
	}

	m_pps->tile_id_len = ReadUE() + 1;
	m_pps->explicit_tile_id_flag = ReadFlag();
	if (m_pps->explicit_tile_id_flag) {
		for (unsigned i = 0; i <= m_pps->num_tile_rows - 1; ++i)
			for (unsigned j = 0; j <= m_pps->num_tile_columns - 1; ++j)
				m_pps->tile_id_val[i][j] = ReadBits(m_pps->tile_id_len);
	}

	m_pps->pic_dra_enabled_present_flag = 0;
	m_pps->pic_dra_enabled_flag = 0;
	if (m_sps->tool_dra) {
		m_pps->pic_dra_enabled_present_flag = ReadFlag();
		if (m_pps->pic_dra_enabled_present_flag) {
			m_pps->pic_dra_enabled_flag = ReadFlag();
			if (m_pps->pic_dra_enabled_flag) {
				m_pps->pic_dra_aps_id = ReadBits(3);
			}
		}
	}

	m_pps->arbitrary_slice_present_flag = ReadFlag();
	m_pps->constrained_intra_pred_flag = ReadFlag();
	m_pps->cu_qp_delta_enabled_flag = ReadFlag();
	if (m_pps->cu_qp_delta_enabled_flag)
		m_pps->cu_qp_delta_area = ReadUE() + 6;

	return true;
}

bool BaseDecoderEVC::ParseSliceHeader(uint32_t nal_unit_type) {

	m_slice_header.pic_parameter_set_id = ReadUE();

	m_slice_header.pps = &m_pps_set[m_slice_header.pic_parameter_set_id];
	m_slice_header.sps = &m_sps_set[m_slice_header.pps->seq_parameter_set_id];

	PPS *pps = m_slice_header.pps;
	SPS *sps = m_slice_header.sps;
	SliceHeader *sh = &m_slice_header;

	uint32_t num_tiles_in_slice = 0;

	if (!pps->single_tile_in_pic_flag) {
		sh->single_tile_in_slice_flag = ReadFlag();
		sh->first_tile_id = ReadBits(pps->tile_id_len);
	} else {
		sh->single_tile_in_slice_flag = 1;
	}

	if (!sh->single_tile_in_slice_flag) {
		if (pps->arbitrary_slice_present_flag) {
			sh->arbitrary_slice_flag = ReadFlag();
		}
		if (!sh->arbitrary_slice_flag) {
			sh->last_tile_id = ReadBits(pps->tile_id_len);
		} else {
			sh->num_remaining_tiles_in_slice = ReadUE() + 1;
			num_tiles_in_slice = sh->num_remaining_tiles_in_slice + 1;
			for (unsigned i = 0; i < num_tiles_in_slice - 1; ++i) {
				sh->delta_tile_id[i] = ReadUE() + 1;
			}
		}
	}

	sh->slice_type = ReadUE();

	if (!sh->arbitrary_slice_flag) {
		int first_row_slice, w_tile_slice, first_col_slice, h_tile_slice, w_tile;
		w_tile = pps->num_tile_columns;
		first_row_slice = sh->first_tile_id / w_tile;
		first_col_slice = sh->first_tile_id % w_tile;
		w_tile_slice = (sh->last_tile_id % w_tile) - first_col_slice;
		h_tile_slice = (sh->last_tile_id / w_tile) - first_row_slice;
		num_tiles_in_slice = (w_tile_slice + 1) * (h_tile_slice + 1);
	} else {
		num_tiles_in_slice = sh->num_remaining_tiles_in_slice + 1;
	}

	if (nal_unit_type == EVCNalType::IDR_NUT) {
		sh->no_output_of_prior_pics_flag = ReadFlag();
	}

	if (sps->tool_mmvd && ((sh->slice_type == SLICE_B) || (sh->slice_type == SLICE_P))) {
		sh->mmvd_group_enable_flag = ReadFlag();
	} else {
		sh->mmvd_group_enable_flag = 0;
	}

	if (sps->tool_alf) {
		sh->alfChromaIdc = 0;
		sh->alf_sh_param_enabledFlag[0] = 0;
		sh->alf_sh_param_enabledFlag[1] = 0;
		sh->alf_sh_param_enabledFlag[2] = 0;

		sh->alf_on = ReadFlag();
		sh->alf_sh_param_enabledFlag[0] = sh->alf_on;
		if (sh->alf_on) {
			sh->aps_id_y = ReadBits(5);

			bool isCtbAlfOn = ReadFlag();

			sh->alfChromaIdc = ReadBits(2);
			sh->alf_sh_param_enabledFlag[1] = sh->alfChromaIdc & 1;
			sh->alf_sh_param_enabledFlag[2] = (sh->alfChromaIdc >> 1) & 1;
			if (sh->alfChromaIdc == 1) {
				sh->ChromaAlfEnabledFlag = 1;
				sh->ChromaAlfEnabled2Flag = 0;
			} else if (sh->alfChromaIdc == 2) {
				sh->ChromaAlfEnabledFlag = 0;
				sh->ChromaAlfEnabled2Flag = 1;
			} else if (sh->alfChromaIdc == 3) {
				sh->ChromaAlfEnabledFlag = 1;
				sh->ChromaAlfEnabled2Flag = 1;
			} else {
				sh->ChromaAlfEnabledFlag = 0;
				sh->ChromaAlfEnabled2Flag = 0;
			}
			if (sh->alfChromaIdc && (sps->chroma_format_idc == 1 || sps->chroma_format_idc == 2)) {
				sh->aps_id_ch = ReadBits(5);
			}
		}
		if (sps->chroma_format_idc == 3 && sh->ChromaAlfEnabledFlag) {
			sh->aps_id_ch = ReadBits(5);
			sh->alfChromaMapSignalled = ReadFlag();
		}
		if (sps->chroma_format_idc == 3 && sh->ChromaAlfEnabled2Flag) {
			sh->aps_id_ch2 = ReadBits(5);
			sh->alfChroma2MapSignalled = ReadFlag();
		}
	}

	if (nal_unit_type != EVCNalType::IDR_NUT) {
		if (sps->tool_pocs) {
			sh->poc_lsb = ReadBits(sps->log2_max_pic_order_cnt_lsb);
		}
	}

	return true;
}

bool BaseDecoderEVC::ParseVPS() { return true; }

std::unique_ptr<BaseDecoder> CreateBaseDecoderEVC() { return std::unique_ptr<BaseDecoder>{new BaseDecoderEVC{}}; }

} // namespace utility
} // namespace vnova

#if defined(__GNUC__)
#if defined(__clang__)
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif
#endif
