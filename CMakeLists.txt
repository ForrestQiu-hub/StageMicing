cmake_minimum_required(VERSION 3.24)
project(StageMicing VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(StageMicing WIN32
    src/main.cpp
    src/App.cpp
    src/TopBar.cpp
    src/LeftPanel.cpp
    src/StageView.cpp
    src/RightPanel.cpp
    src/StatusBar.cpp
    src/Draw.cpp
    src/Pdf.cpp
    src/L10n.cpp
    src/Model.cpp
    src/ProjectIO.cpp
    src/Cabling.cpp
    src/Catalog.cpp
    src/Presets.cpp
    src/Assets.cpp
    src/Lock.cpp
    src/Assoc.cpp
    src/app.rc
)

target_include_directories(StageMicing PRIVATE src)
target_compile_definitions(StageMicing PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(StageMicing PRIVATE gdiplus.lib comctl32.lib comdlg32.lib bcrypt.lib)

if (MSVC)
    target_compile_options(StageMicing PRIVATE /W3 /utf-8 /permissive- /Zc:__cplusplus /MP)
endif()

set_target_properties(StageMicing PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:StageMicing>")

# The sources live in dev\ now, but the shipped assets stay in the repository
# root (they are copied next to the shipped StageMicing.exe), so the build tree
# takes them from ..\assets.  Pointing at ${CMAKE_SOURCE_DIR}/assets here would
# look in dev\assets, which does not exist.
add_custom_command(TARGET StageMicing POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/../assets"
            "$<TARGET_FILE_DIR:StageMicing>/assets"
    COMMENT "Copying symbol assets next to the executable")

# Also place the finished executable in the project root, so it can be started
# directly from E:\Stage Micing Scene\StageMicing.exe (assets/ already lives there).
add_custom_command(TARGET StageMicing POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:StageMicing>"
            "${CMAKE_SOURCE_DIR}/StageMicing.exe"
    COMMENT "Copying StageMicing.exe to the project root")
