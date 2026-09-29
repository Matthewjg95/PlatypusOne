# Bring-up tool for the local display: what DRM exposes, what DrmDisplay
# chose, and a colour test pattern (docs/hardware/TEST_CHECKLISTS.md §3).
add_executable(display_probe main.cpp)
target_link_libraries(display_probe PRIVATE platypus::display_drm)
platypus_set_warnings(display_probe)
