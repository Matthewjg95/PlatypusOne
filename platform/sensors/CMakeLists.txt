add_library(platypus_sensors INTERFACE)
add_library(platypus::sensors ALIAS platypus_sensors)

target_include_directories(platypus_sensors INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(platypus_sensors INTERFACE platypus::hal)

# VL53L8CX line protocol (the UNO Q MCU sketch's records) — portable, so host
# CI tests the parser against recorded bench lines.
add_library(platypus_tof STATIC src/tof/TofProtocol.cpp)
add_library(platypus::tof ALIAS platypus_tof)
target_link_libraries(platypus_tof PUBLIC platypus::sensors)
platypus_set_warnings(platypus_tof)

# Bridge Monitor client (TCP to arduino-router) — Linux only.
if(UNIX)
    add_library(platypus_tof_monitor STATIC src/tof/TofMonitorClient.cpp)
    add_library(platypus::tof_monitor ALIAS platypus_tof_monitor)
    target_include_directories(platypus_tof_monitor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(platypus_tof_monitor PUBLIC platypus::tof)
    find_package(Threads REQUIRED)
    target_link_libraries(platypus_tof_monitor PRIVATE Threads::Threads)
    platypus_set_warnings(platypus_tof_monitor)
endif()
