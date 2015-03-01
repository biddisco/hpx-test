set(tc_SRCS 
  test_component.cpp
)

add_hpx_component(test_component
    ESSENTIAL
    SOURCES ${tc_SRCS}
    COMPONENT_DEPENDENCIES)
target_link_libraries(test_component_component iostreamsd)

set_property(TARGET test_component_component APPEND
             PROPERTY COMPILE_DEFINITIONS
               "HPX_COMPONENT_EXPORTS" "HPX_DEBUG" 
               "HPX_COMPONENT_NAME=test_component"
               "HPX_COMPONENT_STRING=\"test_component\""
)


set(tc_app_SRCS 
  test_client.cpp
)

add_executable(test_component_client ${tc_app_SRCS})
hpx_setup_target(test_component_client)
target_link_libraries(test_component_client
    iostreams_component
    ${Boost_LIBRARIES}
)
