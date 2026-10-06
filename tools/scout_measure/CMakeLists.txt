# Offline bridge from an image file to the Scout analyzer, so camera
# characterization (tools/camera_characterize) measures repeatability with the
# exact measurement code the product runs, not a re-implementation.
add_executable(scout_measure
    main.cpp)

target_link_libraries(scout_measure PRIVATE
    platypus::camera
    platypus::vision)

platypus_set_warnings(scout_measure)
