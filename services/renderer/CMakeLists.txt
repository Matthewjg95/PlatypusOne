add_library(platypus_renderer STATIC
    src/Renderer.cpp)
add_library(platypus::renderer ALIAS platypus_renderer)

target_include_directories(platypus_renderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(platypus_renderer PUBLIC
    platypus::hal
    platypus::display)

platypus_set_warnings(platypus_renderer)
