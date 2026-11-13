cmake_minimum_required(VERSION 3.20)

project(IconViewerRebuild VERSION 1.0.0 LANGUAGES CXX RC)

if (NOT MSVC)
    message(FATAL_ERROR "IconViewer Rebuild currently targets MSVC (rc.exe + the Windows SDK).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(IconViewerRebuild WIN32
    src/common.cpp
    src/strings.cpp
    src/bitmap.cpp
    src/icofile.cpp
    src/dataobject.cpp
    src/settings.cpp
    src/preview.cpp
    src/main.cpp
    res/app.rc
)

target_include_directories(IconViewerRebuild PRIVATE src res)
target_compile_definitions(IconViewerRebuild PRIVATE UNICODE _UNICODE)
target_compile_options(IconViewerRebuild PRIVATE /W4 /utf-8 $<$<CONFIG:Release>:/O2>)
target_link_libraries(IconViewerRebuild PRIVATE
    comctl32 comdlg32 shell32 shlwapi user32 gdi32 ole32 advapi32 uuid gdiplus)

set_target_properties(IconViewerRebuild PROPERTIES OUTPUT_NAME "IconViewerRebuild")

# res/app.rc already embeds our own RT_MANIFEST; stop the linker generating a second one.
target_link_options(IconViewerRebuild PRIVATE /MANIFEST:NO)

# Console helpers used while developing (optional)
add_executable(test_load tests/test_load.cpp src/common.cpp src/bitmap.cpp src/icofile.cpp
    src/strings.cpp)
target_include_directories(test_load PRIVATE src res)
target_compile_definitions(test_load PRIVATE UNICODE _UNICODE)
target_compile_options(test_load PRIVATE /W4 /utf-8)
target_link_libraries(test_load PRIVATE user32 gdi32 shlwapi ole32 gdiplus)

add_executable(test_save tests/test_save.cpp src/common.cpp src/bitmap.cpp src/icofile.cpp
    src/strings.cpp)
target_include_directories(test_save PRIVATE src res)
target_compile_definitions(test_save PRIVATE UNICODE _UNICODE)
target_compile_options(test_save PRIVATE /W4 /utf-8)
target_link_libraries(test_save PRIVATE user32 gdi32 shlwapi ole32 gdiplus)