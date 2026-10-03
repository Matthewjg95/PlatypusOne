# CAD and image handoff: outline -> Outline Forge JSON / DXF, image -> PNG.
# Depends only on the shared geometry types (layering rule).
add_library(platypus_export STATIC
    src/OutlineExport.cpp
    src/Png.cpp)
add_library(platypus::export ALIAS platypus_export)
target_include_directories(platypus_export PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(platypus_export PUBLIC cxx_std_20)
target_link_libraries(platypus_export PUBLIC platypus::geometry)
platypus_set_warnings(platypus_export)
