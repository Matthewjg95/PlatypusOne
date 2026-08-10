add_library(platypus_geometry INTERFACE)
add_library(platypus::geometry ALIAS platypus_geometry)

target_include_directories(platypus_geometry INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
