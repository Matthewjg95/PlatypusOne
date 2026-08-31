add_library(platypus_observation STATIC
    src/CaptureService.cpp
    src/Json.cpp
    src/Observation.cpp)
add_library(platypus::observation ALIAS platypus_observation)

target_include_directories(platypus_observation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(platypus_observation PUBLIC cxx_std_20)

target_link_libraries(platypus_observation PUBLIC platypus::camera)

platypus_set_warnings(platypus_observation)
