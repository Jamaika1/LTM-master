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
//
// Contributors: Sam Littlewood (sam.littlewood@v-nova.com)

#pragma once

#include "uTypes.h"
#include "uBaseDecoder.h"

#define EVCMaxSPSCount 16
#define EVCMaxPPSCount 64
#define EVCMaxSubLayersCount 8

#define EVCMaxTilesRow 22
#define EVCMaxTilesCol 20

namespace vnova {
namespace utility {


struct EVCNalType {
	enum Enum {
		NONIDR_NUT = 0,
		IDR_NUT = 1,
		SPS_NUT = 24,
		PPS_NUT = 25,
		APS_NUT = 26,
		FD_NUT = 27,
		SEI_NUT = 28,
	};
};


class BaseDecoderEVC : public BaseDecoder {
public:
	BaseDecoderEVC();

	bool ParseNalUnit(const uint8_t *nal, uint32_t nalLength);
	BaseDecPictType::Enum GetBasePictureType() const;
	BaseDecNalUnitType::Enum GetBaseNalUnitType() const;
	int32_t GetQP() const;
	uint32_t GetNalType() const;
	int64_t GetPictureOrderCount() const;
	uint32_t GetPictureWidth() const;
	uint32_t GetPictureHeight() const;
	bool GetDPBCanRefresh() const;
	uint8_t GetMaxNumberOfReorderFrames() const;
	uint32_t GetFrameRate() const;
	uint32_t GetBitDepthLuma() const;
	uint32_t GetBitDepthChroma() const;
	uint32_t GetChromaFormatIDC() const;
	uint32_t GetTemporalId() const;
	NALDelimitier Delimiter() const { return NALDelimiterU32Length; }
	int64_t GetPictureOrderCountIncrement() const { return 1; }

private:
	struct SliceType {
		enum Enum { Unknown = 0, I, P, B };
	};

	struct SPS {
		SPS();

		uint32_t seq_parameter_set_id;
		uint32_t profile_idc;
		uint32_t level_idc;
		uint32_t toolset_idc_h;
		uint32_t toolset_idc_l;
		uint32_t chroma_format_idc;
		uint32_t pic_width_in_luma_samples;
		uint32_t pic_height_in_luma_samples;
		uint32_t bit_depth_luma;
		uint32_t bit_depth_chroma;
		bool  sps_btt_flag;


        uint32_t log2_ctu_size;
        uint32_t log2_min_cb_size;
        uint32_t log2_diff_ctu_max_14_cb_size;
        uint32_t log2_diff_ctu_max_tt_cb_size;
		uint32_t  log2_diff_min_cb_min_tt_cb_size;
		bool sps_suco_flag;
        uint32_t log2_diff_ctu_size_max_suco_cb_size;
        uint32_t log2_diff_max_suco_min_suco_cb_size;
		bool tool_admvp;
        bool tool_affine;
        bool tool_amvr;
        bool tool_dmvr;
        bool tool_mmvd;
        bool tool_hmvp;

		bool tool_eipd;
        bool ibc_flag;
		uint32_t ibc_log_max_size;
		bool tool_cm_init;
        bool tool_adcc;

		bool tool_iqt;
		bool tool_ats;
		bool tool_addb ;

		bool tool_alf;
		bool tool_htdf;
		bool tool_rpl;
		bool tool_pocs;
		bool dquant_flag;
		bool tool_dra;

        uint32_t log2_max_pic_order_cnt_lsb;

		uint32_t log2_sub_gop_length;
		uint32_t log2_ref_pic_gap_length;


		uint32_t max_num_ref_pics;

        uint32_t sps_max_dec_pic_buffering;
        bool long_term_ref_pics_flag;
        bool rpl1_same_as_rpl0_flag;
        uint32_t num_ref_pic_lists_in_sps0;
	};

	struct PPS {
		PPS();

		uint32_t pps_id;
		uint32_t sps_id;


		uint32_t pic_parameter_set_id;
		uint32_t seq_parameter_set_id;
		uint32_t num_ref_idx_default_active[2];
		uint32_t additional_lt_poc_lsb_len;
		bool rpl1_idx_present_flag;
		bool single_tile_in_pic_flag;
		uint32_t num_tile_columns;
		uint32_t num_tile_rows;
		bool uniform_tile_spacing_flag;
		uint32_t tile_column_width[EVCMaxTilesCol];
		uint32_t tile_row_height[EVCMaxTilesRow];
		bool loop_filter_across_tiles_enabled_flag;
		uint32_t tile_offset_lens;
		uint32_t tile_id_len;
		bool explicit_tile_id_flag;
		uint32_t tile_id_val[EVCMaxTilesRow][EVCMaxTilesCol];
        bool pic_dra_enabled_present_flag;
		bool pic_dra_enabled_flag;
		uint32_t pic_dra_aps_id;
		bool arbitrary_slice_present_flag;
		bool constrained_intra_pred_flag;
		bool cu_qp_delta_enabled_flag;
        uint32_t cu_qp_delta_area;
	};

	struct SliceHeader {
		SliceHeader();

		PPS *pps;
		SPS *sps;

		uint32_t pic_parameter_set_id;
		bool single_tile_in_slice_flag;
		uint32_t first_tile_id;
        bool arbitrary_slice_flag;
        uint32_t last_tile_id;
        uint32_t num_remaining_tiles_in_slice;
        uint32_t delta_tile_id[EVCMaxTilesRow * EVCMaxTilesCol];
		uint32_t slice_type;
		bool no_output_of_prior_pics_flag;
        bool mmvd_group_enable_flag;
        uint32_t alfChromaIdc = 0;
        bool alf_sh_param_enabledFlag[3];
        bool alf_on;
        uint32_t aps_id_y;
		bool ChromaAlfEnabledFlag;
		bool ChromaAlfEnabled2Flag;
        uint32_t aps_id_ch;
		bool alfChromaMapSignalled;
		uint32_t aps_id_ch2;
		bool alfChroma2MapSignalled;
        uint32_t poc_lsb;
        bool ref_pic_list_sps_flag[2];
        uint32_t rpl_l0_idx;
        uint32_t rpl_l1_idx;
        bool num_ref_idx_active_override_flag;
        bool temporal_mvp_asigned_flag;
        bool collocated_from_list_idx;
		bool collocated_mvp_source_list_idx;
        bool collocated_from_ref_idx;
		bool deblocking_filter_on;
		uint32_t qp;
		int32_t qp_u_offset;
		int32_t qp_v_offset;
		uint32_t entry_point_offset_minus1[EVCMaxTilesRow * EVCMaxTilesCol];
	};

	bool ParseVPS();
	bool ParseSPS();
	bool ParsePPS();
	bool ParseSliceHeader(uint32_t nal_unit_type);
	void HandlePictureOrderCount(uint32_t nal_unit_type, uint32_t nuh_temporal_id);

	uint32_t m_nal_type;
	uint32_t m_temporal_id;

	SPS m_sps_set[EVCMaxSPSCount];
	PPS m_pps_set[EVCMaxPPSCount];
	SPS *m_sps;
	PPS *m_pps;
	SliceHeader m_slice_header;

    int m_poc_val = 0;
    uint32_t m_prev_poc_val = 0;
};

} // namespace utility
} // namespace vnova
