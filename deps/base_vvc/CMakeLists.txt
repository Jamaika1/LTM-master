# Library for VVC Base Codec using VTM
#
find_package( Threads )

list(APPEND DECODER_VTM_SRCS
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/DecoderLib/BinDecoder.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/DecoderLib/DecSlice.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/DecoderLib/SEIread.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/DecoderLib/AnnexBread.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/DecoderLib/VLCReader.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/DecoderLib/DecCu.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/DecoderLib/DecLib.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/DecoderLib/CABACReader.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/DecoderLib/NALread.cpp

	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/AdaptiveLoopFilter.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/AffineGradientSearch.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/BitStream.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/Buffer.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/CacheModel.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/ChromaFormat.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/CodingStructure.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/ContextModelling.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/Contexts.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/DepQuant.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/dtrace_blockstatistics.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/dtrace.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/Hash.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/HRD.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/IbcHashMap.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/InterpolationFilter.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/InterPrediction.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/IntraPrediction.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/LoopFilter.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/MatrixIntraPrediction.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/MCTS.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/Mv.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/ParameterSetManager.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/Picture.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/PicYuvMD5.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/ProfileLevelTier.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/Quant.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/QuantRDOQ.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/RdCost.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/RdCostWeightPrediction.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/Reshape.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/Rom.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/RomLFNST.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/RomTr.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/SampleAdaptiveOffset.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/SEI.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/Slice.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/TrQuant.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/TrQuant_EMT.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/Unit.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/UnitPartitioner.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/UnitTools.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/WeightPrediction.cpp
    ${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/libmd5/libmd5.cpp
    ${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/Utilities/program_options_lite.cpp
    ${CMAKE_CURRENT_LIST_DIR}/VTM/source/App/DecoderApp/DecAppCfg.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/VideoIOYuvMem.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/DecAppMem.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/codec_api.cpp
	${PROJECT_SOURCE_DIR}/util/src/CodecUtils.c
	${PROJECT_SOURCE_DIR}/util/src/Diagnostics.cpp
	${PROJECT_SOURCE_DIR}/util/src/Misc.cpp )

list(APPEND DECODER_VTM_SRCS_X86
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/InitX86.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/CommonDefX86.cpp )

list(APPEND DECODER_VTM_SRCS_X86_SSE41
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/sse41/InterpolationFilter_sse41.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/sse41/AffineGradientSearch_sse41.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/sse41/Buffer_sse41.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/sse41/RdCost_sse41.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/sse41/AdaptiveLoopFilter_sse41.cpp )

list(APPEND DECODER_VTM_SRCS_X86_SSE42
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/sse42/IbcHashmap_sse42.cpp )

list(APPEND DECODER_VTM_SRCS_X86_AVX
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/avx/InterpolationFilter_avx.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/avx/Buffer_avx.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/avx/AdaptiveLoopFilter_avx.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/avx/AffineGradientSearch_avx.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/avx/RdCost_avx.cpp )

list(APPEND DECODER_VTM_SRCS_X86_AVX2
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/avx2/AdaptiveLoopFilter_avx2.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/avx2/Buffer_avx2.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/avx2/InterpolationFilter_avx2.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/avx2/RdCost_avx2.cpp
	${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib/x86/avx2/AffineGradientSearch_avx2.cpp )


add_library(base_vvc SHARED
  ${DECODER_VTM_SRCS}
  ${DECODER_VTM_SRCS_X86}
  ${DECODER_VTM_SRCS_X86_SSE41}
  ${DECODER_VTM_SRCS_X86_SSE42}
  ${DECODER_VTM_SRCS_X86_AVX}
  ${DECODER_VTM_SRCS_X86_AVX2} )


# Compile flags
if( MSVC )
  set_property( SOURCE ${DECODER_VTM_SRCS_X86_SSE41} APPEND PROPERTY COMPILE_FLAGS " -DUSE_SSE41 " )
  set_property( SOURCE ${DECODER_VTM_SRCS_X86_SSE42} APPEND PROPERTY COMPILE_FLAGS " -DUSE_SSE42 " )
  set_property( SOURCE ${DECODER_VTM_SRCS_X86_AVX}   APPEND PROPERTY COMPILE_FLAGS " -DUSE_AVX /arch:AVX " )
  set_property( SOURCE ${DECODER_VTM_SRCS_X86_AVX2}  APPEND PROPERTY COMPILE_FLAGS " -DUSE_AVX2 /arch:AVX2 " )
elseif( UNIX )
  set_property( SOURCE ${DECODER_VTM_SRCS_X86_SSE41} APPEND PROPERTY COMPILE_FLAGS " -DUSE_SSE41 -msse4.1 " )
  set_property( SOURCE ${DECODER_VTM_SRCS_X86_SSE42} APPEND PROPERTY COMPILE_FLAGS " -DUSE_SSE42 -msse4.2 " )
  set_property( SOURCE ${DECODER_VTM_SRCS_X86_AVX}   APPEND PROPERTY COMPILE_FLAGS " -DUSE_AVX  -mavx " )
  set_property( SOURCE ${DECODER_VTM_SRCS_X86_AVX2}  APPEND PROPERTY COMPILE_FLAGS " -DUSE_AVX2 -mavx2 " )
endif()

target_include_directories(base_vvc PUBLIC
  "${CMAKE_CURRENT_LIST_DIR}/include" )

target_include_directories(base_vvc PRIVATE
  "${PROJECT_SOURCE_DIR}/util/include"
  "${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib"
  "${CMAKE_CURRENT_LIST_DIR}/VTM/source/Lib/CommonLib"
  "${CMAKE_CURRENT_LIST_DIR}/VTM/source/App/DecoderApp" )

target_link_libraries(base_vvc ${CMAKE_THREAD_LIBS_INIT} )

if (WIN32)
  target_link_libraries(base_vvc Ws2_32 )
endif(WIN32)

target_compile_definitions(base_vvc PRIVATE CODEC_API_LIBRARY)

#if (UNIX)
#  set_property(TARGET base_avc APPEND_STRING PROPERTY LINK_FLAGS " -Wl,--version-script=${PROJECT_SOURCE_DIR}/cmake/codec_api.script")
#endif(UNIX)
