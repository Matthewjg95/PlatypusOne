# The Dream Lab demo loop on the UNO Q's own display: live preview, capture,
# evidence card. A composition root over already-tested pieces; Linux-only
# because it drives V4L2 and KMS directly.
add_executable(scout_kiosk main.cpp)
target_link_libraries(scout_kiosk PRIVATE
    platypus::ai
    platypus::app_engineering_scout
    platypus::camera_v4l2
    platypus::display_drm
    platypus::observation
    platypus::renderer
    platypus::vision)
platypus_set_warnings(scout_kiosk)
