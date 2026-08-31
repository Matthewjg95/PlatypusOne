add_library(platypus_vision STATIC
    src/ScoutAnalyzer.cpp)
add_library(platypus::vision ALIAS platypus_vision)

target_include_directories(platypus_vision PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

# camera for hal::Frame; observation for evidence emission. Layering holds:
# vision depends on platform interfaces and a sibling service only.
target_link_libraries(platypus_vision PUBLIC
    platypus::camera
    platypus::observation)

platypus_set_warnings(platypus_vision)
