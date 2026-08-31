# Validation battery ships as a small library so the unit tests gate on the
# same battery the CLI reports from.
add_library(platypus_scout_validation STATIC
    ValidationSuite.cpp)
add_library(platypus::scout_validation ALIAS platypus_scout_validation)

target_include_directories(platypus_scout_validation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(platypus_scout_validation PUBLIC
    platypus::ai
    platypus::vision)

platypus_set_warnings(platypus_scout_validation)

add_executable(scout_validation
    main.cpp)
target_link_libraries(scout_validation PRIVATE platypus::scout_validation)
platypus_set_warnings(scout_validation)
