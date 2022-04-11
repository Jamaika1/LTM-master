# Library for AVC Base Codec using JM
#
list(APPEND DECODER_JM_SRCS
	${CMAKE_CURRENT_LIST_DIR}/src/annexb.c
	${CMAKE_CURRENT_LIST_DIR}/src/output.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/biaridecod.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/block.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/cabac.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/configfile.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/context_ini.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/dec_statistics.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/erc_api.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/erc_do_i.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/erc_do_p.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/errorconcealment.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/filehandle.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/fmo.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/header.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/image.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/intra16x16_pred.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/intra16x16_pred_mbaff.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/intra16x16_pred_normal.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/intra4x4_pred.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/intra4x4_pred_mbaff.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/intra4x4_pred_normal.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/intra8x8_pred.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/intra8x8_pred_mbaff.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/intra8x8_pred_normal.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/intra_chroma_pred.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/intra_chroma_pred_mbaff.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/intra_pred_common.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/ldecod.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/leaky_bucket.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/loopFilter.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/loop_filter_mbaff.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/loop_filter_normal.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/macroblock.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/mb_access.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/mb_prediction.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/mb_read.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/mbuffer.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/mbuffer_mvc.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/mc_direct.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/mc_prediction.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/nal.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/nalu.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/parset.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/quant.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/read_comp_cabac.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/read_comp_cavlc.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/rtp.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/sei.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/transform8x8.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod/vlc.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/lib/lcommon/blk_prediction.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/lib/lcommon/config_common.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/lib/lcommon/img_io.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/lib/lcommon/img_process.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/lib/lcommon/input.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/lib/lcommon/io_raw.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/lib/lcommon/io_tiff.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/lib/lcommon/mbuffer_common.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/lib/lcommon/memalloc.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/lib/lcommon/mv_prediction.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/lib/lcommon/nalucommon.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/lib/lcommon/parsetcommon.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/lib/lcommon/resize.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/lib/lcommon/transform.c
	${CMAKE_CURRENT_LIST_DIR}/JM/source/lib/lcommon/win32.c
	${CMAKE_CURRENT_LIST_DIR}/src/codec_api.cpp
    ${PROJECT_SOURCE_DIR}/util/src/CodecUtils.c
	${PROJECT_SOURCE_DIR}/util/src/Diagnostics.cpp
	${PROJECT_SOURCE_DIR}/util/src/Misc.cpp )

add_library(base_avc SHARED ${DECODER_JM_SRCS})

target_include_directories(base_avc PUBLIC
  "${CMAKE_CURRENT_LIST_DIR}/include" )

target_include_directories(base_avc PRIVATE
  "${PROJECT_SOURCE_DIR}/util/include"
  "${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod"
  "${CMAKE_CURRENT_LIST_DIR}/JM/source/app/ldecod"
  "${CMAKE_CURRENT_LIST_DIR}/JM/source/lib/lcommon" )

target_compile_definitions(base_avc PRIVATE CODEC_API_LIBRARY)

if (WIN32)
  target_link_libraries(base_avc Ws2_32 )
endif(WIN32)

#if (UNIX)
#  set_property(TARGET base_avc APPEND_STRING PROPERTY LINK_FLAGS " -Wl,--version-script=${PROJECT_SOURCE_DIR}/cmake/codec_api.script")
#endif(UNIX)
