add_library(platypus_audio INTERFACE)
add_library(platypus::audio ALIAS platypus_audio)

target_include_directories(platypus_audio INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(platypus_audio INTERFACE platypus::hal)
