add_library(platypus_filesystem STATIC
    src/ProjectStore.cpp
    src/SettingsStore.cpp)
add_library(platypus::filesystem ALIAS platypus_filesystem)

target_include_directories(platypus_filesystem PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(platypus_filesystem PUBLIC platypus::hal)

platypus_set_warnings(platypus_filesystem)
