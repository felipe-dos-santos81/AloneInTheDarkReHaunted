# Unit tests for the engine-free mouse modules (FitdLib/mouse). These sources
# include only standard headers, so the test binary links neither FitdLib nor
# bgfx/SDL. Engine-coupled behaviour is covered by docs/mouse-gameplay-checklist.md.
set(MOUSE_DIR "${CMAKE_SOURCE_DIR}/FitdLib/mouse")

# Engine-free mouse sources compiled into the tests (tasks append to this list).
set(MOUSE_CORE_SOURCES
    "${MOUSE_DIR}/mouseGesture.cpp"
    "${MOUSE_DIR}/mousePick.cpp"
    "${MOUSE_DIR}/mouseNav.cpp"
)

file(GLOB ENGINE_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")

add_executable(engine_tests ${ENGINE_TEST_SOURCES} ${MOUSE_CORE_SOURCES})
target_include_directories(engine_tests PRIVATE "${THIRD_PARTY}/doctest" "${MOUSE_DIR}")
target_compile_definitions(engine_tests PRIVATE FITD_SOURCE_DIR="${CMAKE_SOURCE_DIR}/FitdLib")
target_compile_definitions(engine_tests PRIVATE FITD_EXE_DIR="${CMAKE_SOURCE_DIR}/Fitd")
set_target_properties(engine_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

add_test(NAME engine_tests COMMAND engine_tests)
