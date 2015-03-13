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
find_package(Boost 1.56
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
#set(HPX_SOURCE_DIR "${CMAKE_MODULE_PATH}/..")
message("HPX source dir is ${HPX_SOURCE_DIR}")

#link_directories("${HPXBOOST_ROOT}/lib")
#include_directories(${Boost_INCLUDE_DIR})


if(BGQ)
  add_hpx_executable(bgq_test
      ESSENTIAL
      SOURCES bgq_test.cpp
      COMPONENT_DEPENDENCIES)
endif()

add_executable(future_reduce future_reduce.cpp)
hpx_setup_target(future_reduce)
target_link_libraries(future_reduce
    iostreams_component
    ${Boost_LIBRARIES}
)

add_executable(benchmark_transfers benchmark_transfers.cpp)
hpx_setup_target(benchmark_transfers)
target_link_libraries(benchmark_transfers
    iostreams_component
    ${Boost_LIBRARIES}
)
add_executable(serialize_buffer serialize_buffer.cpp)
hpx_setup_target(serialize_buffer)
target_link_libraries(serialize_buffer
    iostreams_component
    ${Boost_LIBRARIES}
)

add_subdirectory(network_storage)

#add_subdirectory(test_component)

