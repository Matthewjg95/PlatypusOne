# Portable core: layout, screens, capture and the synthetic camera. Builds
# everywhere so tools/ui_preview can render the kiosk's session flow on a
# host, and so the logic is compiled by every CI job, not only Linux.
add_library(platypus_scout_kiosk_core STATIC KioskCore.cpp)
add_library(platypus::scout_kiosk_core ALIAS platypus_scout_kiosk_core)
target_include_directories(platypus_scout_kiosk_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(platypus_scout_kiosk_core PUBLIC
    platypus::ai
    platypus::app_engineering_scout
    platypus::camera
    platypus::observation
    platypus::renderer
    platypus::session
    platypus::vision)
platypus_set_warnings(platypus_scout_kiosk_core)

# The kiosk itself drives V4L2 and KMS directly: Linux only.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(scout_kiosk main.cpp)
    target_link_libraries(scout_kiosk PRIVATE
        platypus::scout_kiosk_core
        platypus::camera_v4l2
        platypus::display_drm)
    platypus_set_warnings(scout_kiosk)
endif()
