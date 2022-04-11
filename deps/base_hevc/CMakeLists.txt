# Library for HEVC Base Codec using HM
#
find_package( Threads )

list(APPEND DECODER_HM_SRCS
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibDecoder/AnnexBread.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibDecoder/NALread.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibDecoder/SEIread.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibDecoder/SyntaxElementParser.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibDecoder/TDecBinCoderCABAC.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibDecoder/TDecCAVLC.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibDecoder/TDecConformance.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibDecoder/TDecCu.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibDecoder/TDecEntropy.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibDecoder/TDecGop.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibDecoder/TDecSbac.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibDecoder/TDecSlice.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibDecoder/TDecTop.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/ContextModel3DBuffer.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/ContextModel.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/Debug.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/ProfileLevelTierFeatures.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/SEI.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComBitStream.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComCABACTables.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComChromaFormat.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComDataCU.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComInterpolationFilter.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComLoopFilter.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComMotionInfo.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComPattern.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComPic.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComPicSym.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComPicYuv.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComPicYuvMD5.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComPrediction.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComRdCost.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComRdCostWeightPrediction.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComRom.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComSampleAdaptiveOffset.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComSlice.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComTrQuant.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComTU.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComWeightPrediction.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/TLibCommon/TComYuv.cpp
	${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib/libmd5/libmd5.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/TVideoIOYuvMem.cpp
	${CMAKE_CURRENT_LIST_DIR}/src/codec_api.cpp
	${PROJECT_SOURCE_DIR}/util/src/CodecUtils.c
	${PROJECT_SOURCE_DIR}/util/src/Diagnostics.cpp
	${PROJECT_SOURCE_DIR}/util/src/Misc.cpp )

add_library(base_hevc SHARED ${DECODER_HM_SRCS})

target_include_directories(base_hevc PUBLIC
  "${CMAKE_CURRENT_LIST_DIR}/include" )

target_include_directories(base_hevc PRIVATE
  "${PROJECT_SOURCE_DIR}/util/include"
  "${CMAKE_CURRENT_LIST_DIR}/HM/source/Lib" )

target_link_libraries(base_hevc ${CMAKE_THREAD_LIBS_INIT} )

target_compile_definitions(base_hevc PRIVATE CODEC_API_LIBRARY)

if (WIN32)
  target_link_libraries(base_hevc Ws2_32 )
endif(WIN32)

#if (UNIX)
#  set_property(TARGET base_avc APPEND_STRING PROPERTY LINK_FLAGS " -Wl,--version-script=${PROJECT_SOURCE_DIR}/cmake/codec_api.script")
#endif(UNIX)
