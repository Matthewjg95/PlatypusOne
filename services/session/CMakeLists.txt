# Scout sessions: repeated captures of one object, guidance, persistence and
# CAD export. Reads observation records by claim name - it never links vision
# or ai, so the dependency direction stays services -> contract.
add_library(platypus_session STATIC
    src/ObjectProfile.cpp
    src/Session.cpp
    src/SessionStore.cpp)
add_library(platypus::session ALIAS platypus_session)
target_include_directories(platypus_session PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(platypus_session PUBLIC cxx_std_20)
target_link_libraries(platypus_session PUBLIC
    platypus::export
    platypus::geometry
    platypus::observation)
platypus_set_warnings(platypus_session)
