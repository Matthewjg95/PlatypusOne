# Engineering Scout ships as a static library the launcher links, like
# settings; becomes a MODULE library when appfw/plugins lands.
add_library(platypus_app_engineering_scout STATIC
    src/EngineeringScoutApp.cpp)
add_library(platypus::app_engineering_scout ALIAS platypus_app_engineering_scout)

target_include_directories(platypus_app_engineering_scout PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(platypus_app_engineering_scout PUBLIC
    platypus::appfw
    platypus::observation)

platypus_set_warnings(platypus_app_engineering_scout)
