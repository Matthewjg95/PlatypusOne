add_library(platypus_camera INTERFACE)
add_library(platypus::camera ALIAS platypus_camera)

target_include_directories(platypus_camera INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(platypus_camera INTERFACE platypus::hal)

# Raw Bayer handling (RAW10 unpack, 2x2 binning, white balance,
# auto-exposure) — portable, so host CI tests it.
add_library(platypus_camera_bayer STATIC src/bayer/Bayer.cpp)
add_library(platypus::camera_bayer ALIAS platypus_camera_bayer)
target_link_libraries(platypus_camera_bayer PUBLIC platypus::camera)
platypus_set_warnings(platypus_camera_bayer)

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

    # CSI raw backend (IMX219 on camss): media graph + RDI raw capture.
    add_library(platypus_camera_csi STATIC
        src/csi/CsiCamera.cpp)
    add_library(platypus::camera_csi ALIAS platypus_camera_csi)
    target_include_directories(platypus_camera_csi PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(platypus_camera_csi PUBLIC platypus::camera platypus::camera_bayer)
    target_link_libraries(platypus_camera_csi PRIVATE Threads::Threads)
    platypus_set_warnings(platypus_camera_csi)
endif()
