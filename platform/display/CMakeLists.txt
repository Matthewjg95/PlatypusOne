add_library(platypus_display INTERFACE)
add_library(platypus::display ALIAS platypus_display)

target_include_directories(platypus_display INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(platypus_display INTERFACE platypus::hal)
