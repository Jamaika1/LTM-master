# Library for EVC Base Codec using ETM
#
find_package( Threads )

list(APPEND DECODER_ETM_SRCS
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evc_alf.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evc.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evcd_bsr.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evcd.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evc_debug.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evcd_eco.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evc_df.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evc_dra.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evcd_util.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evce_bsw.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evce.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evce_eco.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evce_mode.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evce_pibc.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evce_pinter.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evce_pintra.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evce_sad.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evce_tbl.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evce_tq.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evce_util.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evc_img.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evc_ipred.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evc_itdq.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evc_mc.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evc_picman.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evc_recon.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evc_tbl.c
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evc_util.c

	${CMAKE_CURRENT_LIST_DIR}/ETM/src/EncAdaptiveLoopFilter.cpp
	${CMAKE_CURRENT_LIST_DIR}/ETM/src/evce_ibc_hashmap.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/codec_api.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/evc_decoder_mem.c
	${PROJECT_SOURCE_DIR}/util/src/CodecUtils.c
	${PROJECT_SOURCE_DIR}/util/src/Diagnostics.cpp
	${PROJECT_SOURCE_DIR}/util/src/Misc.cpp )

add_library(base_evc SHARED ${DECODER_ETM_SRCS})

target_include_directories(base_evc PUBLIC
  "${CMAKE_CURRENT_LIST_DIR}/include" )

target_include_directories(base_evc PRIVATE
  "${PROJECT_SOURCE_DIR}/util/include"
  "${CMAKE_CURRENT_LIST_DIR}/ETM/inc" )

target_link_libraries(base_evc ${CMAKE_THREAD_LIBS_INIT} )

if (WIN32)
  target_link_libraries(base_evc Ws2_32 )
endif(WIN32)

target_compile_definitions(base_evc PRIVATE CODEC_API_LIBRARY)

#if (UNIX)
#  set_property(TARGET base_avc APPEND_STRING PROPERTY LINK_FLAGS " -Wl,--version-script=${PROJECT_SOURCE_DIR}/cmake/codec_api.script")
#endif(UNIX)
