# Device shell: splash, home carousel, applet lifecycle (issue #51).
# Portable, so host CI tests it and tools/ui_preview renders it.
add_library(platypus_shell STATIC
    src/Applets.cpp
    src/Carousel.cpp
    src/Shell.cpp)
add_library(platypus::shell ALIAS platypus_shell)

target_include_directories(platypus_shell PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(platypus_shell PUBLIC
    platypus::appfw
    platypus::renderer)

platypus_set_warnings(platypus_shell)
