set(ENABLE_MPI Off CACHE BOOL "")
set(CMAKE_CXX_COMPILER "/usr/tce/packages/cce-tce/cce-16.0.0/bin/crayCC" CACHE PATH "")
set(CMAKE_C_COMPILER "/usr/tce/packages/cce-tce/cce-16.0.0/bin/craycc" CACHE PATH "")
set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "")
set(BLT_CXX_STD "c++17" CACHE STRING "")
set(CMAKE_CXX_STANDARD "17" CACHE STRING "")
set(ENABLE_C On CACHE BOOL "")
set(ENABLE_FORTRAN On CACHE BOOL "")
set(ENABLE_CUDA Off CACHE BOOL "")
set(ENABLE_OPENMP Off CACHE BOOL "")
set(ENABLE_HIP On CACHE BOOL "")
set(ENABLE_EXAMPLES Off CACHE BOOL "")
set(ENABLE_TESTS Off CACHE BOOL "")
set(ENABLE_TOOLS Off CACHE BOOL "")
set(ENABLE_DOCS Off CACHE BOOL "")
set(ENABLE_BENCHMARKS Off CACHE BOOL "")
set(UMPIRE_ENABLE_SLIC Off CACHE BOOL "")
set(UMPIRE_ENABLE_LOGGING On CACHE BOOL "")
set(UMPIRE_ENABLE_BACKTRACE Off CACHE BOOL "")
set(UMPIRE_ENABLE_IPC_SHARED_MEMORY Off CACHE BOOL "")
set(CMAKE_CXX_FLAGS "--offload-arch=gfx90a" CACHE STRING "")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG" CACHE STRING "")
set(HIP_ROOT_DIR "/opt/rocm-5.6.0/hip" CACHE PATH "")
set(HIP_HIPCC_FLAGS "-D__HIP_ROCclr__ -D__HIP_PLATFORM_AMD__ -DCAMP_USE_PLATFORM_DEFAULT_STREAM -D__HIP_ARCH_GFX90A__=1 --rocm-path=/opt/rocm-5.6.0 -std=c++14 -x hip --offload-arch=gfx90a" CACHE STRING "")
set(CMAKE_HIP_ARCHITECTURES "gfx90a" CACHE STRING "")
set(CMAKE_ROCM_COMPILER "/usr/tce/packages/cray-mpich-tce/cray-mpich-8.1.26-cce-16.0.0/bin/mpicrayCC")
set(CMAKE_ROCM_HOST_COMPILER "/usr/tce/packages/cray-mpich-tce/cray-mpich-8.1.26-cce-16.0.0/bin/mpicxx")
