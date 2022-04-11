#
cmake_minimum_required(VERSION 3.10)

project(TestModel VERSION 0.3
        DESCRIPTION "LCEVC test model"
        LANGUAGES CXX C )

# Options to choose which base codecs are built as shared libraries, versus calling external executable.
#
option(LTM_ENABLE_CODECAPI_AVC			"Use LTM Codec API for AVC with JM in shared library"						ON)
option(LTM_ENABLE_CODECAPI_HEVC			"Use LTM Codec API for HEVC with HM in shared library"						ON)
option(LTM_ENABLE_CODECAPI_VVC			"Use LTM Codec API for VVC with VTM in shared library"						ON)
option(LTM_ENABLE_CODECAPI_EVC			"Use LTM Codec API for EVC with ETM in shared library"						ON)
option(LTM_BUILD_EXTERNAL_CODECS		"Compile base codecs from respective test model source"						OFF)

#
set(SRC_DIR "${PROJECT_SOURCE_DIR}")
set(DEPS_DIR "${PROJECT_SOURCE_DIR}/deps")
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# -----------------------------------------------
# C++ standard and global compilation options
# -----------------------------------------------
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific flags
if (UNIX)
  add_compile_options(-march=sandybridge -mfpmath=sse)
  set(CMAKE_C_FLAGS_RELEASE "-g -O3 -fomit-frame-pointer -funroll-loops")
  set(CMAKE_CXX_FLAGS_RELEASE "-g -O3 -fomit-frame-pointer -funroll-loops")
  set(CMAKE_SHARED_LINK_FLAGS_RELEASE "-g -O3 -fomit-frame-pointer -funroll-loops")
  set(CMAKE_EXE_LINK_FLAGS_RELEASE "-g -O3 -fomit-frame-pointer -funroll-loops")

  list(APPEND LCEVC_EXTERNAL_LINK_LIBS m gcov dl)
endif(UNIX)

if (WIN32)
  add_compile_options(-nologo -EHsc -Gy -GS- -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -DNOMINMAX)
  set(LCEVC_EXE_LINK_FLAGS "-nologo" CACHE STRING "Linker flags when building executables (win)")
  set(CMAKE_C_FLAGS_RELEASE "-O2 -Oy -Oi")
  set(CMAKE_CXX_FLAGS_RELEASE "-O2 -Oy -Oi")

  list(APPEND LCEVC_EXTERNAL_LINK_LIBS Ws2_32)
endif(WIN32)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Default CMake build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if("${CMAKE_GENERATOR}" MATCHES "^Visual Studio")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_BINARY_DIR}")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${PROJECT_BINARY_DIR}/Debug")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PROJECT_BINARY_DIR}/RelWithDebInfo")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${PROJECT_BINARY_DIR}/MinSizeRel")
else()
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
endif()

set(LCEVC_BITSTREAM_DEBUG 0 CACHE STRING "BITSTREAM DEBUG flag")
set(LCEVC_USE_SEI_NALU 0 CACHE STRING "SEI NALU flag")

# -----------------------------------------------
# Collect sources for ModelDecoder
# -----------------------------------------------

list(APPEND DECODER_SRCS
  ${SRC_DIR}/src/ModelDecoderApp.cpp
  ${SRC_DIR}/decoder/src/Decoder.cpp
  ${SRC_DIR}/decoder/src/Add.cpp
  ${SRC_DIR}/decoder/src/BaseVideoDecoder.cpp
  ${SRC_DIR}/decoder/src/BaseVideoDecoderCodecApi.cpp
  ${SRC_DIR}/decoder/src/Conform.cpp
  ${SRC_DIR}/decoder/src/Convert.cpp
  ${SRC_DIR}/decoder/src/Deblocking.cpp
  ${SRC_DIR}/decoder/src/Deserializer.cpp
  ${SRC_DIR}/decoder/src/Dimensions.cpp
  ${SRC_DIR}/decoder/src/Dithering.cpp
  ${SRC_DIR}/decoder/src/EntropyDecoder.cpp
  ${SRC_DIR}/decoder/src/Expand.cpp
  ${SRC_DIR}/decoder/src/HuffmanDecoder.cpp
  ${SRC_DIR}/decoder/src/InverseQuantize.cpp
  ${SRC_DIR}/decoder/src/InverseTransformDD.cpp
  ${SRC_DIR}/decoder/src/InverseTransformDDS.cpp
  ${SRC_DIR}/decoder/src/InverseTransformDDS_1D.cpp
  ${SRC_DIR}/decoder/src/InverseTransformDD_1D.cpp
  ${SRC_DIR}/decoder/src/PredictedResidual.cpp
  ${SRC_DIR}/decoder/src/Probe.cpp
  ${SRC_DIR}/decoder/src/ScanEnhancement.cpp
  ${SRC_DIR}/decoder/src/TemporalDecode.cpp
  ${SRC_DIR}/decoder/src/Upsampling.cpp
  ${SRC_DIR}/util/src/BitstreamStatistic.cpp
  ${SRC_DIR}/util/src/BitstreamUnpacker.cpp
  ${SRC_DIR}/util/src/Buffer.cpp
  ${SRC_DIR}/util/src/Component.cpp
  ${SRC_DIR}/util/src/Codec.cpp
  ${SRC_DIR}/util/src/Diagnostics.cpp
  ${SRC_DIR}/util/src/Image.cpp
  ${SRC_DIR}/util/src/Misc.cpp
  ${SRC_DIR}/util/src/LcevcMd5.cpp
  ${SRC_DIR}/util/src/Packet.cpp
  ${SRC_DIR}/util/src/Surface.cpp
  ${SRC_DIR}/util/src/YUVReader.cpp
  ${SRC_DIR}/util/src/YUVWriter.cpp
  ${SRC_DIR}/src/Types.cpp
  ${SRC_DIR}/src/uBaseDecoder.cpp
  ${SRC_DIR}/src/uBaseDecoderAVC.cpp
  ${SRC_DIR}/src/uBaseDecoderEVC.cpp
  ${SRC_DIR}/src/uBaseDecoderHEVC.cpp
  ${SRC_DIR}/src/uBaseDecoderVVC.cpp
  ${SRC_DIR}/src/uBaseDecoderYUV.cpp
  ${SRC_DIR}/src/uESFile.cpp
  ${SRC_DIR}/src/uYUVDesc.cpp
  ${SRC_DIR}/src/uString.cpp )

# -----------------------------------------------
# Collect sources for ModelEncoder
# -----------------------------------------------

list(APPEND ENCODER_SRCS
  ${SRC_DIR}/src/ModelEncoderApp.cpp
  ${SRC_DIR}/encoder/src/FileEncoder.cpp
  ${SRC_DIR}/encoder/src/Encoder.cpp
  ${SRC_DIR}/decoder/src/Add.cpp
  ${SRC_DIR}/decoder/src/Convert.cpp
  ${SRC_DIR}/decoder/src/Conform.cpp
  ${SRC_DIR}/decoder/src/Deblocking.cpp
  ${SRC_DIR}/decoder/src/Dimensions.cpp
  ${SRC_DIR}/decoder/src/Dithering.cpp
  ${SRC_DIR}/decoder/src/EntropyDecoder.cpp
  ${SRC_DIR}/decoder/src/Expand.cpp
  ${SRC_DIR}/decoder/src/HuffmanDecoder.cpp
  ${SRC_DIR}/decoder/src/InverseQuantize.cpp
  ${SRC_DIR}/decoder/src/InverseTransformDD.cpp
  ${SRC_DIR}/decoder/src/InverseTransformDDS.cpp
  ${SRC_DIR}/decoder/src/InverseTransformDDS_1D.cpp
  ${SRC_DIR}/decoder/src/InverseTransformDD_1D.cpp
  ${SRC_DIR}/decoder/src/PredictedResidual.cpp
  ${SRC_DIR}/decoder/src/TemporalDecode.cpp
  ${SRC_DIR}/decoder/src/Upsampling.cpp
  ${SRC_DIR}/encoder/src/Compare.cpp
  ${SRC_DIR}/encoder/src/Crop.cpp
  ${SRC_DIR}/encoder/src/Downsampling.cpp
  ${SRC_DIR}/encoder/src/EntropyEncoder.cpp
  ${SRC_DIR}/encoder/src/HuffmanEncoder.cpp
  ${SRC_DIR}/encoder/src/LayerEncodeFlags.cpp
  ${SRC_DIR}/encoder/src/ParameterDefaults.cpp
  ${SRC_DIR}/encoder/src/PriorityConfiguration.cpp
  ${SRC_DIR}/encoder/src/PriorityMap.cpp
  ${SRC_DIR}/decoder/src/Probe.cpp
  ${SRC_DIR}/encoder/src/Quantize.cpp
  ${SRC_DIR}/encoder/src/ResidualMap.cpp
  ${SRC_DIR}/encoder/src/Serializer.cpp
  ${SRC_DIR}/encoder/src/Subtract.cpp
  ${SRC_DIR}/encoder/src/TemporalEncode.cpp
  ${SRC_DIR}/encoder/src/TransformDD.cpp
  ${SRC_DIR}/encoder/src/TransformDDS.cpp
  ${SRC_DIR}/encoder/src/TransformDDS_1D.cpp
  ${SRC_DIR}/encoder/src/TransformDD_1D.cpp
  ${SRC_DIR}/util/src/BitstreamPacker.cpp
  ${SRC_DIR}/util/src/BitstreamStatistic.cpp
  ${SRC_DIR}/util/src/BitstreamUnpacker.cpp
  ${SRC_DIR}/util/src/Buffer.cpp
  ${SRC_DIR}/util/src/Component.cpp
  ${SRC_DIR}/util/src/Diagnostics.cpp
  ${SRC_DIR}/util/src/Image.cpp
  ${SRC_DIR}/util/src/LcevcMd5.cpp
  ${SRC_DIR}/util/src/Misc.cpp
  ${SRC_DIR}/util/src/Packet.cpp
  ${SRC_DIR}/util/src/Parameters.cpp
  ${SRC_DIR}/util/src/Surface.cpp
  ${SRC_DIR}/util/src/YUVReader.cpp
  ${SRC_DIR}/util/src/YUVWriter.cpp
  ${SRC_DIR}/src/Types.cpp
  ${SRC_DIR}/src/uBaseDecoder.cpp
  ${SRC_DIR}/src/uBaseDecoderAVC.cpp
  ${SRC_DIR}/src/uBaseDecoderEVC.cpp
  ${SRC_DIR}/src/uBaseDecoderHEVC.cpp
  ${SRC_DIR}/src/uBaseDecoderVVC.cpp
  ${SRC_DIR}/src/uBaseDecoderYUV.cpp
  ${SRC_DIR}/src/uESFile.cpp  )

### Tests
##

list(APPEND TEST_SRCS
  ${SRC_DIR}/unit_tests/TestTemporal.cpp
  ${SRC_DIR}/encoder/src/TransformDD.cpp
  ${SRC_DIR}/encoder/src/TransformDDS.cpp
  ${SRC_DIR}/encoder/src/TransformDD_1D.cpp
  ${SRC_DIR}/encoder/src/TransformDDS_1D.cpp
  ${SRC_DIR}/decoder/src/InverseTransformDD.cpp
  ${SRC_DIR}/decoder/src/InverseTransformDDS.cpp
  ${SRC_DIR}/decoder/src/InverseTransformDD_1D.cpp
  ${SRC_DIR}/decoder/src/InverseTransformDDS_1D.cpp
  ${SRC_DIR}/encoder/src/HuffmanEncoder.cpp
  ${SRC_DIR}/encoder/src/EntropyEncoder.cpp
  ${SRC_DIR}/decoder/src/HuffmanDecoder.cpp
  ${SRC_DIR}/decoder/src/EntropyDecoder.cpp
  ${SRC_DIR}/decoder/src/TemporalDecode.cpp
  ${SRC_DIR}/util/src/YUVReader.cpp
  ${SRC_DIR}/util/src/YUVWriter.cpp
  ${SRC_DIR}/util/src/Diagnostics.cpp
  ${SRC_DIR}/util/src/Surface.cpp
  ${SRC_DIR}/util/src/Packet.cpp
  ${SRC_DIR}/util/src/Buffer.cpp
  ${SRC_DIR}/util/src/Image.cpp
  ${SRC_DIR}/util/src/Component.cpp
  ${SRC_DIR}/util/src/BitstreamPacker.cpp
  ${SRC_DIR}/util/src/BitstreamUnpacker.cpp
  ${SRC_DIR}/util/src/Misc.cpp
  ${SRC_DIR}/decoder/src/Convert.cpp )

# Specify include path for the base codec shims - adding them all causes name clashes
set_property(SOURCE
  ${SRC_DIR}/src/uBaseDecoderAVC.cpp
  ${SRC_DIR}/decoder/src/BaseVideoDecoderJM.cpp
  APPEND PROPERTY COMPILE_FLAGS
	"-I${DEPS_DIR}/base_avc/JM/source/app/ldecod -I${DEPS_DIR}/base_avc/JM/source/lib/lcommon" )

set_property(SOURCE
  ${SRC_DIR}/src/uBaseDecoderHEVC.cpp
  ${SRC_DIR}/decoder/src/BaseVideoDecoderHM.cpp
  APPEND PROPERTY COMPILE_FLAGS
  "-I${DEPS_DIR}/base_hevc/HM/source/Lib" )

set_property(SOURCE  ${SRC_DIR}/src/uBaseDecoderVVC.cpp  APPEND PROPERTY COMPILE_FLAGS
  "-I${DEPS_DIR}/base_vvc/minimum_VTM/source/Lib" )

if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
  message("Foo")
  # -----------------------------------------------
  # Run git and extract commit hash
  # -----------------------------------------------
  set(PRE_CONFIGURE_FILE "git_version.h.in")
  set(POST_CONFIGURE_FILE "${CMAKE_BINARY_DIR}/git_version.h")
  include(git_version)
endif()

# -----------------------------------------------
# Include external dependencies
# -----------------------------------------------

# Command line parser
set(CXXOPTS_DIR "${DEPS_DIR}/cxxopts")

# JSON parser
set(JSON_DIR "${DEPS_DIR}/json")

# VVC Parser
add_subdirectory (deps/base_vvc/minimum_VTM)

# -----------------------------------------------
# ModelDecoder and ModelEncoder:
# add include directories
# add dependencies
# set preprocessor directives (DEFINEs)
# -----------------------------------------------
add_executable(ModelEncoder ${ENCODER_SRCS})
add_executable(ModelDecoder ${DECODER_SRCS})

list(APPEND LCEVC_TARGETS "ModelEncoder" "ModelDecoder")

foreach(TARGET ${LCEVC_TARGETS})
  # LCEVC test model include directories
  target_include_directories(${TARGET} PRIVATE
	"${SRC_DIR}/util/include"
	"${SRC_DIR}/decoder/include"
	"${SRC_DIR}/encoder/include"
	"${SRC_DIR}/common/include"
	"${SRC_DIR}/src" )

  # External dependencies
  target_include_directories(${TARGET} PRIVATE
	"${CXXOPTS_DIR}/include"
	"${JSON_DIR}/include"
	"${CMAKE_BINARY_DIR}")

  # External libraries to link to
  target_link_libraries(${TARGET} ${LCEVC_EXTERNAL_LINK_LIBS} BaseVvcMinimumVTM )

  # Compilation definitions
  target_compile_definitions(${TARGET} PRIVATE BITSTREAM_DEBUG=${LCEVC_BITSTREAM_DEBUG} USE_SEI_NALU=${LCEVC_USE_SEI_NALU})

  if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
	# Make sure that the header file with git version number is
	# generated before compilation
	add_dependencies(${TARGET} check_git)
  endif()

  # Install target into runtime destination
  install(TARGETS ${TARGET} RUNTIME DESTINATION bin)
endforeach(TARGET)

# AVC base codec
# NB: Windows targets put .dlls in RUNTIME_OUTPUT_DIRECTORY
#
function(ltm_set_codec_output TGT)
  if("${CMAKE_GENERATOR}" MATCHES "^Visual Studio")
	set_target_properties( ${TGT}
	  PROPERTIES
	  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PROJECT_BINARY_DIR}/external_codecs/libs"
	  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${PROJECT_BINARY_DIR}/Debug/external_codecs/libs"
	  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PROJECT_BINARY_DIR}/RelWithDebInfo/external_codecs/libs"
	  RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${PROJECT_BINARY_DIR}/MinSizeRel/external_codecs/libs"

	  LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PROJECT_BINARY_DIR}/external_codecs/libs"
	  LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PROJECT_BINARY_DIR}/Debug/external_codecs/libs"
	  LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PROJECT_BINARY_DIR}/RelWithDebInfo/external_codecs/libs"
	  LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PROJECT_BINARY_DIR}/MinSizeRel/external_codecs/libs")
  else()
	set_target_properties( ${TGT}
	  PROPERTIES
	  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/external_codecs/libs"
	  LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/external_codecs/libs")
  endif()
endfunction()

if(LTM_ENABLE_CODECAPI_AVC)
  target_compile_definitions(ModelDecoder PRIVATE  "LTM_ENABLE_CODECAPI_AVC")
  add_subdirectory (deps/base_avc)
  target_link_libraries(ModelDecoder base_avc)
  ltm_set_codec_output(base_avc)
endif(LTM_ENABLE_CODECAPI_AVC)

# HEVC base decoder as shared library
if(LTM_ENABLE_CODECAPI_HEVC)
  target_compile_definitions(ModelDecoder PRIVATE "LTM_ENABLE_CODECAPI_HEVC")
  add_subdirectory (deps/base_hevc)
  target_link_libraries(ModelDecoder base_hevc)
  ltm_set_codec_output(base_hevc)
endif(LTM_ENABLE_CODECAPI_HEVC)

# VVC base decoder as shared library
if(LTM_ENABLE_CODECAPI_VVC)
  target_compile_definitions(ModelDecoder PRIVATE "LTM_ENABLE_CODECAPI_VVC")
  add_subdirectory (deps/base_vvc)
  target_link_libraries(ModelDecoder base_vvc)
  ltm_set_codec_output(base_vvc)
endif(LTM_ENABLE_CODECAPI_VVC)

# EVC base decoder as shared library
if(LTM_ENABLE_CODECAPI_EVC)
  target_compile_definitions(ModelDecoder PRIVATE "LTM_ENABLE_CODECAPI_EVC")
  add_subdirectory (deps/base_evc)
  target_link_libraries(ModelDecoder base_evc)
  ltm_set_codec_output(base_evc)
endif(LTM_ENABLE_CODECAPI_EVC)

# Base Encoders as executables
#
if(LTM_BUILD_EXTERNAL_CODECS)
  add_subdirectory (deps/base_avc/JM)
  add_subdirectory (deps/base_hevc/HM)
  add_subdirectory (deps/base_evc/ETM)
  add_subdirectory (deps/base_vvc/VTM)
endif(LTM_BUILD_EXTERNAL_CODECS)

if(NOT "${CMAKE_GENERATOR}" MATCHES "^Visual Studio")
  # -----------------------------------------------
  # Custom target to build documentation.
  # -----------------------------------------------
  add_custom_target(documentation
	COMMAND pandoc "${PROJECT_SOURCE_DIR}/README.md" -o "${CMAKE_BINARY_DIR}/README.pdf"
	COMMAND pandoc "${PROJECT_SOURCE_DIR}/README.md" -o "${CMAKE_BINARY_DIR}/README.html"
	WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
	VERBATIM
	)

  # -----------------------------------------------
  # Custom target to build a self contained source release
  #
  # Creates a 'lcevc_test_model.tar.gz' archive with:
  #   No executables,
  #   The submodule source for all the base codecs
  #   A git_version.h reflecting the commit that build the release
  #
  # -----------------------------------------------
  add_custom_target(source_release
	COMMAND tar cf "${PROJECT_BINARY_DIR}/_source_gather.tar"
	    "--exclude-ignore=tar_source_ignore"
		"--exclude-vcs" "--exclude=_build"
		"--exclude=_build_win32"
		.
	COMMAND rm -rf ${PROJECT_BINARY_DIR}/lcevc_test_model
	COMMAND mkdir -p ${PROJECT_BINARY_DIR}/lcevc_test_model
	COMMAND tar -x -f "${PROJECT_BINARY_DIR}/_source_gather.tar" -C ${PROJECT_BINARY_DIR}/lcevc_test_model

	COMMAND sed -i "/LTM_BUILD_EXTERNAL_CODECS/s/OFF/ON/" "${PROJECT_BINARY_DIR}/lcevc_test_model/CMakeLists.txt"
	COMMAND cp "${PROJECT_BINARY_DIR}/git_version.h" "${PROJECT_BINARY_DIR}/lcevc_test_model/src"
	COMMAND tar -c -z -f "${PROJECT_BINARY_DIR}/lcevc_test_model.tar.gz" -C "${PROJECT_BINARY_DIR}" lcevc_test_model
	WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
	VERBATIM
	)
endif()

# -----------------------------------------------
# Install section for stuff under external_codecs
# -----------------------------------------------
if(WIN32)
  install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/external_codecs"
    USE_SOURCE_PERMISSIONS
    DESTINATION bin
    PATTERN "*.exe"
    PATTERN "*.cfg")
else(WIN32)
  install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/external_codecs"
    USE_SOURCE_PERMISSIONS
    DESTINATION bin
    PATTERN "*"
    PATTERN "*.exe" EXCLUDE)
endif(WIN32)
