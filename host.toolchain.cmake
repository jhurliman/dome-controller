# Specify the compilers
set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")

# Specify Clang as the CUDA compiler
set(CMAKE_CUDA_COMPILER "${CMAKE_CXX_COMPILER}")

# Set linker flags on non-MacOS platforms
if(NOT APPLE)
  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld"
      CACHE STRING "Linker flags")
endif()

set(CUDAToolkit_ROOT "${CMAKE_CURRENT_LIST_DIR}/nvidia/cuda-10.2_amd64")
if(NOT EXISTS ${CUDAToolkit_ROOT})
  message(
    FATAL_ERROR
      "CUDAToolkit_ROOT does not exist: ${CUDAToolkit_ROOT}\nPlease run ./scripts/extract-cuda.sh")
endif()

# Set compiler flags for color diagnostics
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fansi-escape-codes -fcolor-diagnostics")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fansi-escape-codes -fcolor-diagnostics")
