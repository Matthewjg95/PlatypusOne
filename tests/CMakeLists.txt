# Dependency-free assert-based tests for now; migrate to Catch2 in
# third_party once vendoring policy is decided (ROADMAP: infra/test-framework).
add_executable(platypus_tests
    test_app_registry.cpp
    test_event_queue.cpp
    test_mcu_framing.cpp
    test_renderer.cpp)

target_link_libraries(platypus_tests PRIVATE
    platypus::appfw
    platypus::renderer)

platypus_set_warnings(platypus_tests)

add_test(NAME platypus_tests COMMAND platypus_tests)
