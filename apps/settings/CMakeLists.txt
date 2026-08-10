# Settings ships as a static library the launcher links; when dynamic plugin
# loading lands (ROADMAP: appfw/plugins) this becomes a MODULE library.
add_library(platypus_app_settings STATIC
    src/SettingsApp.cpp)
add_library(platypus::app_settings ALIAS platypus_app_settings)

target_include_directories(platypus_app_settings PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(platypus_app_settings PUBLIC platypus::appfw)

platypus_set_warnings(platypus_app_settings)
