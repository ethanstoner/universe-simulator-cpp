cmake_minimum_required(VERSION 3.24)
project(universe-sim LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MSVC)
  # /Zc:preprocessor is not optional here. MSVC's legacy preprocessor mangles
  # raw string literals when they are stringized by a macro, and the test
  # assertions do exactly that (`CHECK(expr)` stringizes `expr`), so the JSON
  # tests fail to compile with "illegal escape sequence" without it.
  # /utf-8 keeps source and execution charsets consistent regardless of the
  # machine's locale.
  add_compile_options(/Zc:preprocessor /utf-8 /permissive-)
endif()

include(FetchContent)

# ---------------------------------------------------------------- dependencies

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
  GIT_SHALLOW TRUE)

# glm and imgui are consumed as plain sources. SOURCE_SUBDIR points at a path
# that does not exist, which makes MakeAvailable download them without running
# add_subdirectory() -- glm's own CMake build is unnecessary (it is header only)
# and imgui ships no CMakeLists at all.
FetchContent_Declare(glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR do-not-configure)
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.5
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR do-not-configure)

FetchContent_MakeAvailable(glfw glm imgui)

add_library(glm_headers INTERFACE)
target_include_directories(glm_headers INTERFACE ${glm_SOURCE_DIR})

# GLAD (vendored; generated with glad2 for gl:core=3.3, no extensions).
add_library(glad STATIC external/glad/src/gl.c)
target_include_directories(glad PUBLIC external/glad/include)

add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)

# ---------------------------------------------------------------------- simcore
# The simulation core is deliberately free of any OpenGL/GLFW/ImGui dependency
# so that it can be unit tested headlessly. See docs/ARCHITECTURE.md.
file(GLOB SIMCORE_SOURCES CONFIGURE_DEPENDS src/sim/*.cpp)
add_library(simcore STATIC ${SIMCORE_SOURCES})
target_include_directories(simcore PUBLIC src)
target_link_libraries(simcore PUBLIC glm_headers)

# ----------------------------------------------------------------- application
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
  src/main.cpp src/engine/*.cpp src/render/*.cpp src/ui/*.cpp)
add_executable(universe-sim ${APP_SOURCES})
target_link_libraries(universe-sim PRIVATE simcore glad glfw imgui glm_headers)
target_include_directories(universe-sim PRIVATE src)
if(WIN32)
  target_link_libraries(universe-sim PRIVATE opengl32)
else()
  find_package(OpenGL REQUIRED)
  target_link_libraries(universe-sim PRIVATE OpenGL::GL)
endif()

# Shaders and configs are read from disk at runtime. They are NOT copied next to
# the executable by a POST_BUILD step: CMake wraps those in a nested `cmd /C
# "cd /D <path> && ..."`, and this repository can live under a directory whose
# name contains an ampersand ("AI & Media"), which cmd re-parses as a command
# separator. Instead the source directory is baked in and AssetPaths searches
# for the first location that exists. See docs/ARCHITECTURE.md "Asset lookup".
target_compile_definitions(universe-sim PRIVATE
  UNIVERSE_SIM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# ----------------------------------------------------------------------- tests
enable_testing()
add_subdirectory(tests)

# The application self-test drives every UI-reachable state transition against a
# real GL context: scene loading, spawning, deleting, focusing, the reset
# buttons, every integrator and stabilisation mode, and emptying the scene
# entirely. It needs a working OpenGL 3.3 driver, so it is registered separately
# from the headless unit tests and can be excluded with `ctest -E selftest` on a
# machine without one.
add_test(NAME universe-sim-selftest COMMAND universe-sim --selftest)
set_tests_properties(universe-sim-selftest PROPERTIES LABELS "gpu" TIMEOUT 300)
