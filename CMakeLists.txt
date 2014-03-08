project(HPX-tests CXX)

# Require a recent version of cmake
cmake_minimum_required(VERSION 2.8.4 FATAL_ERROR)


if (NOT $ENV{HPX_ROOT} STREQUAL "") 
  message("HPX root is ${HPX_ROOT} or $ENV{HPX_ROOT}")
  set(HPX_ROOT $ENV{HPX_ROOT})
endif()
if (WIN32)
  set(HPX_ROOT "C:/Program Files/hpx")
endif()

if(HPX_ROOT STREQUAL "")
  message(ERROR "HPX_ROOT not set to installation of HPX")
endif()

# This adds the HPX cmake configuration directory to the search path.
message("Setting ${HPX_ROOT}/share/cmake-${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}/Modules")
set(CMAKE_MODULE_PATH
    ${HPX_ROOT}/share/cmake-${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}/Modules
    ${HPX_ROOT}/share/hpx-0.9.8/cmake)


# Instruct cmake to find the HPX settings
find_package(HPX)

include_directories(${HPX_INCLUDE_DIR})     
link_directories(${HPX_LIBRARY_DIR})

if (NOT $ENV{BOOST_ROOT} STREQUAL "")
  set(BOOST_ROOT $ENV{BOOST_ROOT})
endif()

find_package(Boost)

message("boost root is ${BOOST_ROOT}")
link_directories("${BOOST_ROOT}/lib")
link_directories("${HPXBOOST_ROOT}/lib")
include_directories(${Boost_INCLUDE_DIR})

find_package(QT4)
if (QT4_FOUND)
  add_subdirectory(qt)
endif()

find_package(MPI)
INCLUDE_DIRECTORIES(${MPI_INCLUDE_PATH})

if(BGQ)
  add_hpx_executable(bgq_test
      ESSENTIAL
      SOURCES bgq_test.cpp
      COMPONENT_DEPENDENCIES)
  #target_link_libraries(bgq_test_exe iostreamsd)
endif()

add_hpx_executable(future_reduce
    ESSENTIAL
    SOURCES future_reduce.cpp
    COMPONENT_DEPENDENCIES)
target_link_libraries(future_reduce_exe iostreamsd)

add_hpx_executable(benchmark_transfers
    ESSENTIAL
    SOURCES benchmark_transfers.cpp
#    osu_base.cpp
    COMPONENT_DEPENDENCIES)
target_link_libraries(benchmark_transfers_exe iostreamsd)
target_link_libraries(benchmark_transfers_exe ${MPI_LIBRARY})

add_hpx_executable(serialize_buffer
    ESSENTIAL
    SOURCES serialize_buffer.cpp
    COMPONENT_DEPENDENCIES)
target_link_libraries(serialize_buffer_exe iostreamsd)

add_subdirectory(test_component)
#add_subdirectory(accumulator)
