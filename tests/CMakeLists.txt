# Dependency-free assert-based tests for now; migrate to Catch2 in
# third_party once vendoring policy is decided (ROADMAP: infra/test-framework).
add_executable(platypus_tests
    test_app_registry.cpp
    test_event_queue.cpp
    test_fastener_classifier.cpp
    test_linked_display.cpp
    test_mcu_framing.cpp
    test_observation.cpp
    test_presentation_framing.cpp
    test_renderer.cpp
    test_scout_analyzer.cpp
    test_scout_capture.cpp
    test_scout_card.cpp
    test_scout_validation.cpp
    test_settings_store.cpp)

target_link_libraries(platypus_tests PRIVATE
    platypus::ai
    platypus::app_engineering_scout
    platypus::appfw
    platypus::display_linked
    platypus::observation
    platypus::renderer
    platypus::scout_validation
    platypus::vision)

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

# PTYs + GNU linker wrapping test the real Linux serial implementation.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(serial_mcu_bridge_tests test_serial_mcu_bridge.cpp)
    target_link_libraries(serial_mcu_bridge_tests PRIVATE platypus::board_unoq)
    target_link_options(serial_mcu_bridge_tests PRIVATE -Wl,--wrap=tcsetattr)
    target_compile_options(serial_mcu_bridge_tests PRIVATE -UNDEBUG)
    platypus_set_warnings(serial_mcu_bridge_tests)
    add_test(NAME serial_mcu_bridge_tests COMMAND serial_mcu_bridge_tests)
    set_tests_properties(serial_mcu_bridge_tests PROPERTIES TIMEOUT 10)
endif()
