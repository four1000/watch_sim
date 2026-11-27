cmake_minimum_required(VERSION 3.20)
# macOS only (ignored elsewhere): must be set before project(). Matches the
# deployment target of the static OpenCV from scripts/build_opencv_mac.sh.
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
project(watch_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows + MSVC uses the prebuilt OpenCV DLL vendored in third_party/opencv.
# macOS (Apple Clang) uses a static OpenCV built by scripts/build_opencv_mac.sh.
# The flags below are the same rules spelled for each compiler.
if(MSVC)
  set(AP_FLAGS   /W4 /WX /utf-8 /permissive- /GR- /EHs-c-)
  set(TEST_FLAGS /W4 /WX /utf-8 /permissive- /EHsc)
  set(SIM_FLAGS  /W4 /WX /utf-8 /permissive- /EHsc)
else()
  set(AP_FLAGS   -Wall -Wextra -Werror -fno-rtti -fno-exceptions)
  set(TEST_FLAGS -Wall -Wextra)
  set(SIM_FLAGS  -Wall -Wextra)
endif()

include(cmake/third_party.cmake)

if(MSVC)
# CMake puts /EHsc in the default flags for every target. Remove it here and
# opt in per target, so the ap library can really be built without exceptions.
string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

# -----------------------------------------------------------------------------
# ap : the portable application layer.
# Built with the embedded C++ subset enforced by the compiler:
#   /GR-              no RTTI (dynamic_cast / typeid will not compile)
#   /EHs-c- + _HAS_EXCEPTIONS=0   no exceptions
#   /W4 /WX           warnings are errors
# No OpenCV, no Win32, no <thread>, no heap after init.
# -----------------------------------------------------------------------------
file(GLOB_RECURSE AP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ap/src/*.cpp)
file(GLOB_RECURSE AP_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ap/include/*.h)
add_library(ap STATIC ${AP_SOURCES} ${AP_HEADERS})
target_include_directories(ap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ap/include)
target_compile_options(ap PRIVATE ${AP_FLAGS})
target_compile_definitions(ap PRIVATE _HAS_EXCEPTIONS=0)

# -----------------------------------------------------------------------------
# ap_tests : host-side unit tests for the AP layer (no OpenCV needed)
# -----------------------------------------------------------------------------
enable_testing()
include(GoogleTest)
file(GLOB AP_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
add_executable(ap_tests ${AP_TEST_SOURCES})
target_include_directories(ap_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(ap_tests PRIVATE ap GTest::gtest_main)
target_compile_options(ap_tests PRIVATE ${TEST_FLAGS})
gtest_discover_tests(ap_tests DISCOVERY_MODE PRE_TEST)

# -----------------------------------------------------------------------------
# watch_sim : Windows simulator (platform layer + composition root)
# -----------------------------------------------------------------------------
file(GLOB SIM_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/platform/sim/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/platform/sim/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp)
add_executable(watch_sim ${SIM_SOURCES})
target_include_directories(watch_sim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/platform)
target_link_libraries(watch_sim PRIVATE ap opencv_world cvui)
target_compile_options(watch_sim PRIVATE ${SIM_FLAGS})
target_compile_definitions(watch_sim PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
if(MSVC)
add_custom_command(TARGET watch_sim POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${OPENCV_DLL} $<TARGET_FILE_DIR:watch_sim>)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT watch_sim)

# -----------------------------------------------------------------------------
# Portable binary: cmake --install build --config Release --prefix dist
# Puts watch_sim.exe next to every non-system DLL it needs: opencv_world and
# the MSVC C++ runtime (app-local deployment, so no VC++ Redistributable install
# is required). Everything else it imports is part of Windows itself.
# -----------------------------------------------------------------------------
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
set(CMAKE_INSTALL_UCRT_LIBRARIES OFF)   # UCRT ships with Windows 10/11
include(InstallRequiredSystemLibraries)
install(TARGETS watch_sim RUNTIME DESTINATION . CONFIGURATIONS Release)
install(FILES ${OPENCV_DLL} DESTINATION . CONFIGURATIONS Release)
install(FILES docs/demo.keys DESTINATION . CONFIGURATIONS Release)
else()
# macOS: cmake --install build-mac --prefix dist-mac
# OpenCV is linked statically (scripts/build_opencv_mac.sh), so watch_sim is a
# single executable that only depends on macOS system libraries.
install(TARGETS watch_sim RUNTIME DESTINATION .)
install(FILES docs/demo.keys DESTINATION .)
endif()
