# Dependency-free assert-based tests for now; migrate to Catch2 in
# third_party once vendoring policy is decided (ROADMAP: infra/test-framework).
add_executable(platypus_tests
    test_app_registry.cpp
    test_event_queue.cpp
    test_mcu_framing.cpp
    test_observation.cpp
    test_presentation_framing.cpp
    test_renderer.cpp)

target_link_libraries(platypus_tests PRIVATE
    platypus::appfw
    platypus::observation
    platypus::renderer)

platypus_set_warnings(platypus_tests)

# The tests are assert-based, and Release configs define NDEBUG, which
# compiles every assert out — a Release test run would pass while verifying
# nothing. Undefine it for the test target only (target options come after
# the per-config /DNDEBUG, so the -U/ /U wins).
if(MSVC)
    target_compile_options(platypus_tests PRIVATE /UNDEBUG)
else()
    target_compile_options(platypus_tests PRIVATE -UNDEBUG)
endif()

add_test(NAME platypus_tests COMMAND platypus_tests)
