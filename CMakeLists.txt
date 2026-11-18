cmake_minimum_required(VERSION 3.20)
project(watch_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT MSVC)
  message(FATAL_ERROR "This project targets Windows + MSVC (OpenCV binaries in third_party/ are vc16 x64).")
endif()

include(cmake/third_party.cmake)

# CMake puts /EHsc in the default flags for every target. Remove it here and
# opt in per target, so the ap library can really be built without exceptions.
string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

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
target_compile_options(ap PRIVATE /W4 /WX /utf-8 /permissive- /GR- /EHs-c-)
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
target_compile_options(ap_tests PRIVATE /W4 /WX /utf-8 /permissive- /EHsc)
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
target_compile_options(watch_sim PRIVATE /W4 /WX /utf-8 /permissive- /EHsc)
target_compile_definitions(watch_sim PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
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
