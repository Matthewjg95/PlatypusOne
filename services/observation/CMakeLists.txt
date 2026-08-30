add_library(platypus_observation STATIC
    src/Json.cpp
    src/Observation.cpp)
add_library(platypus::observation ALIAS platypus_observation)

target_include_directories(platypus_observation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(platypus_observation PUBLIC cxx_std_20)

platypus_set_warnings(platypus_observation)
