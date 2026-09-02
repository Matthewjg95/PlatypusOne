add_library(platypus_display INTERFACE)
add_library(platypus::display ALIAS platypus_display)

target_include_directories(platypus_display INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(platypus_display INTERFACE platypus::hal)

# LinkedDisplay — IDisplay over the presentation link (ADR-0001 prototype
# path). The core is portable and host-tested; the POSIX serial transport is
# the UNO Q-side piece and builds on UNIX only.
find_package(Threads REQUIRED)
add_library(platypus_display_linked STATIC
    src/linked/LinkedDisplay.cpp)
if(UNIX)
    target_sources(platypus_display_linked PRIVATE src/linked/SerialTransport.cpp)
endif()
add_library(platypus::display_linked ALIAS platypus_display_linked)
target_include_directories(platypus_display_linked PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(platypus_display_linked
    PUBLIC platypus::display
    PRIVATE Threads::Threads)
platypus_set_warnings(platypus_display_linked)
