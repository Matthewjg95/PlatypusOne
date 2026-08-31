add_library(platypus_ai STATIC
    src/FastenerClassifier.cpp)
add_library(platypus::ai ALIAS platypus_ai)

target_include_directories(platypus_ai PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

# vision for ScoutAnalysis (and observation transitively). Layering holds:
# ai depends on sibling services only.
target_link_libraries(platypus_ai PUBLIC
    platypus::vision)

platypus_set_warnings(platypus_ai)
