# Offline UI render harness — see main.cpp. Not an app: one-shot CLI.
add_executable(ui_preview
    main.cpp)

target_link_libraries(ui_preview PRIVATE
    platypus::ai
    platypus::app_engineering_scout
    platypus::renderer
    platypus::vision)

platypus_set_warnings(ui_preview)
