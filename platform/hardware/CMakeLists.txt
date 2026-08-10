add_library(platypus_hal INTERFACE)
add_library(platypus::hal ALIAS platypus_hal)

target_include_directories(platypus_hal INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(platypus_hal INTERFACE cxx_std_20)

# UNO Q board driver — POSIX serial transport, so Linux/UNIX targets only.
# Host builds on Windows use HostSimBoard instead.
if(UNIX)
    add_library(platypus_board_unoq STATIC
        src/unoq/UnoQBoard.cpp
        src/unoq/SerialMcuBridge.cpp)
    add_library(platypus::board_unoq ALIAS platypus_board_unoq)
    target_include_directories(platypus_board_unoq PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(platypus_board_unoq PUBLIC platypus::hal)
    find_package(Threads REQUIRED)
    target_link_libraries(platypus_board_unoq PRIVATE Threads::Threads)
    platypus_set_warnings(platypus_board_unoq)
endif()
