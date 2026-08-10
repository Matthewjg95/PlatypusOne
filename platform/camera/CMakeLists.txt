add_library(platypus_camera INTERFACE)
add_library(platypus::camera ALIAS platypus_camera)

target_include_directories(platypus_camera INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(platypus_camera INTERFACE platypus::hal)
