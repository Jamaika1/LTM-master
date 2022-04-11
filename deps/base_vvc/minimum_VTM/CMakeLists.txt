# Library for VVC Base Parsing - Enough files to parse NAL Units
#
list(APPEND VTM_COMMON_SRC_FILES
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/BitStream.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/Buffer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/CodingStructure.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/ContextModelling.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/Contexts.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/Hash.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/HRD.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/InterpolationFilter.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/InterPrediction.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/MCTS.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/Mv.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/ParameterSetManager.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/Picture.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/ProfileLevelTier.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/RdCost.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/RdCostWeightPrediction.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/Rom.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/SampleAdaptiveOffset.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/Slice.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/Unit.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/UnitTools.cpp
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/CommonLib/WeightPrediction.cpp )

list(APPEND VTM_DECODER_SRC_FILES
  ${CMAKE_CURRENT_LIST_DIR}/source/Lib/DecoderLib/VLCReader.cpp )

add_library(BaseVvcMinimumVTM ${VTM_COMMON_SRC_FILES} ${VTM_DECODER_SRC_FILES} )

target_include_directories(BaseVvcMinimumVTM PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include" )

target_include_directories(BaseVvcMinimumVTM PRIVATE "${CMAKE_CURRENT_LIST_DIR}/source/Lib" )
