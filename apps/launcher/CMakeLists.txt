add_executable(platypus_launcher
    src/main.cpp
    src/LauncherApp.cpp
    src/HostSimBoard.cpp
    src/Win32SimDisplay.cpp)

target_include_directories(platypus_launcher PRIVATE src)

target_link_libraries(platypus_launcher PRIVATE
    platypus::appfw
    platypus::app_engineering_scout
    platypus::app_settings
    platypus::camera
    platypus::sensors
    platypus::audio)

if(UNIX)
    # UNO Q composition path: --board unoq with an optional linked display.
    target_link_libraries(platypus_launcher PRIVATE
        platypus::board_unoq
        platypus::display_linked)
endif()

platypus_set_warnings(platypus_launcher)
