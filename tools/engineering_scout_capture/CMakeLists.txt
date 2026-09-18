# Development harness, not an app: apps/ hold IApp implementations; this is a
# one-shot CLI for exercising the capture vertical slice on bench and UNO Q.
add_executable(engineering_scout_capture
    main.cpp)

# vision + ai make this the one executable that runs the whole physical path:
# camera -> measurement -> classification -> evidence record.
target_link_libraries(engineering_scout_capture PRIVATE
    platypus::camera
    platypus::observation
    platypus::vision
    platypus::ai)

if(UNIX)
    target_link_libraries(engineering_scout_capture PRIVATE platypus::camera_v4l2)
endif()

platypus_set_warnings(engineering_scout_capture)
