add_library(platypus_camera INTERFACE)
add_library(platypus::camera ALIAS platypus_camera)

target_include_directories(platypus_camera INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(platypus_camera INTERFACE platypus::hal)

# V4L2 UVC backend — Linux-only (the UNO Q capture path). Host builds on
# Windows use hal::testing::FakeCamera instead.
if(UNIX)
    add_library(platypus_camera_v4l2 STATIC
        src/v4l2/V4l2Camera.cpp)
    add_library(platypus::camera_v4l2 ALIAS platypus_camera_v4l2)
    target_include_directories(platypus_camera_v4l2 PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(platypus_camera_v4l2 PUBLIC platypus::camera)
    find_package(Threads REQUIRED)
    target_link_libraries(platypus_camera_v4l2 PRIVATE Threads::Threads)
    platypus_set_warnings(platypus_camera_v4l2)
endif()
