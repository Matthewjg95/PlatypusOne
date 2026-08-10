add_library(platypus_sensors INTERFACE)
add_library(platypus::sensors ALIAS platypus_sensors)

target_include_directories(platypus_sensors INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(platypus_sensors INTERFACE platypus::hal)
