# Unit tests for the engine-free modules: mouse gameplay (FitdLib/mouse), the
# HD character model math and body file reader (FitdLib/models), the accessibility assists
# (FitdLib/assist), font composition (FitdLib/text) and collision rules (FitdLib/physics, header-only). These sources include only
# standard headers, so the test binary links neither FitdLib nor bgfx/SDL.
# Engine-coupled mouse behaviour is covered by docs/mouse-gameplay-checklist.md.
set(MOUSE_DIR "${CMAKE_SOURCE_DIR}/FitdLib/mouse")
set(MODELS_DIR "${CMAKE_SOURCE_DIR}/FitdLib/models")
set(ASSIST_DIR "${CMAKE_SOURCE_DIR}/FitdLib/assist")
set(PHYSICS_DIR "${CMAKE_SOURCE_DIR}/FitdLib/physics")
set(TEXT_DIR "${CMAKE_SOURCE_DIR}/FitdLib/text")

# Engine-free mouse sources compiled into the tests.
set(MOUSE_CORE_SOURCES
    "${MOUSE_DIR}/mouseGesture.cpp"
    "${MOUSE_DIR}/mousePick.cpp"
    "${MOUSE_DIR}/mousePoly.cpp"
    "${MOUSE_DIR}/mouseNav.cpp"
)

# Engine-free model sources compiled into the tests.
set(MODELS_CORE_SOURCES
    "${MODELS_DIR}/bodyPose.cpp"
    "${MODELS_DIR}/renderCamera.cpp"
    "${MODELS_DIR}/hdmMesh.cpp"
    "${MODELS_DIR}/skinnedBody.cpp"
    "${MODELS_DIR}/mipChain.cpp"
    "${MODELS_DIR}/modelLight.cpp"
)

# Engine-free assist sources compiled into the tests.
set(ASSIST_CORE_SOURCES
    "${ASSIST_DIR}/counterRule.cpp"
    "${ASSIST_DIR}/paceRule.cpp"
)

# Engine-free text sources compiled into the tests.
set(TEXT_CORE_SOURCES
    "${TEXT_DIR}/fontCompose.cpp"
    "${TEXT_DIR}/uiText.cpp"
)
# uiText.cpp holds literal UTF-8; MSVC would read it in the system code page.
if(MSVC)
    set_source_files_properties("${CMAKE_SOURCE_DIR}/FitdLib/text/uiText.cpp" PROPERTIES COMPILE_OPTIONS "/utf-8")
endif()

file(GLOB ENGINE_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")

add_executable(engine_tests ${ENGINE_TEST_SOURCES} ${MOUSE_CORE_SOURCES} ${MODELS_CORE_SOURCES} ${ASSIST_CORE_SOURCES} ${TEXT_CORE_SOURCES})
target_include_directories(engine_tests PRIVATE "${THIRD_PARTY}/doctest" "${MOUSE_DIR}" "${MODELS_DIR}" "${ASSIST_DIR}" "${PHYSICS_DIR}" "${TEXT_DIR}")
target_compile_definitions(engine_tests PRIVATE FITD_SOURCE_DIR="${CMAKE_SOURCE_DIR}/FitdLib")
target_compile_definitions(engine_tests PRIVATE FITD_EXE_DIR="${CMAKE_SOURCE_DIR}/Fitd")
# Binary fixtures shared with the Python tools' tests (tests/tools/model_helpers.py).
target_compile_definitions(engine_tests PRIVATE FITD_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
set_target_properties(engine_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

add_test(NAME engine_tests COMMAND engine_tests)
