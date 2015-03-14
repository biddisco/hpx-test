project(HPX-tests CXX)

# Require a recent version of cmake
cmake_minimum_required(VERSION 2.8.4 FATAL_ERROR)

#--------------------------------------------------
# Setup Output directories
#--------------------------------------------------
SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY
    ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all Executables."
)
SET (CMAKE_LIBRARY_OUTPUT_DIRECTORY
    ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all Libraries"
)
SET (CMAKE_ARCHIVE_OUTPUT_DIRECTORY
    ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all static libraries."
)
#--------------------------------------------------
# Find Boost (should already be found by HPX anyway)
#--------------------------------------------------
find_package(Boost 1.55
    COMPONENTS
        program_options thread system date_time chrono serialization atomic filesystem
    REQUIRED
)
if (WIN32)
  add_definitions(-DBOOST_PROGRAM_OPTIONS_DYN_LINK)
endif()

message("Boost libs are in ${Boost_LIBRARY_DIRS}  ${Boost_LIBRARIES}")

#--------------------------------------------------
# Find HPX
#--------------------------------------------------
find_package(HPX REQUIRED)
set(CMAKE_CXX_FLAGS ${HPX_CXX_FLAGS})
include_directories(${HPX_INCLUDE_DIRS})
get_filename_component(HPX_SOURCE_DIR ${CMAKE_MODULE_PATH} DIRECTORY)
set(HPX_SOURCE_DIR "${CMAKE_MODULE_PATH}/..")
message("HPX source dir is ${HPX_SOURCE_DIR}")

include_directories(${HPX_SOURCE_DIR}/tests/unit/parallel)

#link_directories("${HPXBOOST_ROOT}/lib")
#include_directories(${Boost_INCLUDE_DIR})

hpx_add_compile_flag(-bigobj)

if(BGQ)
  add_hpx_executable(bgq_test
      ESSENTIAL
      SOURCES bgq_test.cpp
      COMPONENT_DEPENDENCIES)
endif()

macro(add_simple name)
 add_executable(${name} ${name}.cpp)
  hpx_setup_target(${name})
  target_link_libraries(${name}
    iostreams_component
    ${Boost_LIBRARIES}
  )
endmacro()

add_simple(future_reduce)
add_simple(benchmark_transfers)
add_simple(serialize_buffer)
add_simple(scan_in_place)
add_simple(inclusive_scan)
add_simple(exclusive_scan)

#add_subdirectory(network_storage)

#add_subdirectory(test_component)

