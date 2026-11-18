# All dependencies are vendored under third_party/ so a fresh clone builds offline.

set(TP ${CMAKE_SOURCE_DIR}/third_party)

# ---- GoogleTest (source, v1.15.2) -------------------------------------------
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
add_subdirectory(${TP}/googletest ${CMAKE_BINARY_DIR}/googletest EXCLUDE_FROM_ALL)

# ---- OpenCV 4.10.0 (prebuilt opencv_world, Release only) -------------------
# Only the Release DLL is vendored: the Debug DLL is 126 MB, above GitHub's
# 100 MB file limit. Debug builds of watch_sim would mix the debug CRT with a
# release DLL (std::string crosses the boundary in cv::putText) -> use
# Release or RelWithDebInfo for the simulator. ap and ap_tests build in any config.
set(OPENCV_DLL ${TP}/opencv/bin/opencv_world4100.dll)
add_library(opencv_world SHARED IMPORTED GLOBAL)
set_target_properties(opencv_world PROPERTIES
  IMPORTED_LOCATION             ${OPENCV_DLL}
  IMPORTED_IMPLIB               ${TP}/opencv/lib/opencv_world4100.lib
  INTERFACE_INCLUDE_DIRECTORIES ${TP}/opencv/include
  MAP_IMPORTED_CONFIG_DEBUG     ""
  MAP_IMPORTED_CONFIG_MINSIZEREL "")

# ---- cvui 2.9.0-beta (single header) ----------------------------------------
add_library(cvui INTERFACE)
target_include_directories(cvui SYSTEM INTERFACE ${TP}/cvui)
