add_library(platypus_appfw STATIC
    src/AppRegistry.cpp)
add_library(platypus::appfw ALIAS platypus_appfw)

target_include_directories(platypus_appfw PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(platypus_appfw PUBLIC
    platypus::hal
    platypus::renderer
    platypus::filesystem)

platypus_set_warnings(platypus_appfw)
