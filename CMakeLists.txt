cmake_minimum_required(VERSION 3.24)
project(LuminaPhoto VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Concurrent)
find_package(Qt6 QUIET COMPONENTS Svg)
find_package(OpenImageIO CONFIG QUIET)

set(SOURCES
    src/main.cpp
    src/core/AppSettings.h
    src/core/AppSettings.cpp
    src/core/PhotoModels.h
    src/core/PsdParser.h
    src/core/PsdParser.cpp
    src/core/ImageLoader.h
    src/core/ImageLoader.cpp
    src/core/LibraryDatabase.h
    src/core/LibraryDatabase.cpp
    src/core/WindowsFileAssociation.h
    src/core/WindowsFileAssociation.cpp
    src/ui/SmoothUi.h
    src/ui/AlbumIcons.h
    src/ui/SettingsDialog.h
    src/ui/FileAssociationsDialog.h
    src/ui/QuickViewerWindow.h
    src/ui/QuickViewerWindow.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    resources/resources.qrc
)

if(WIN32)
    list(APPEND SOURCES resources/app_icon.rc)
endif()

add_executable(LuminaPhoto WIN32 ${SOURCES})

target_include_directories(LuminaPhoto PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(LuminaPhoto PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Concurrent
)

if(Qt6Svg_FOUND)
    target_compile_definitions(LuminaPhoto PRIVATE LUMINA_HAS_QT_SVG=1)
    target_link_libraries(LuminaPhoto PRIVATE Qt6::Svg)
endif()

if(OpenImageIO_FOUND)
    target_compile_definitions(LuminaPhoto PRIVATE LUMINA_HAS_OIIO=1)
    target_link_libraries(LuminaPhoto PRIVATE OpenImageIO::OpenImageIO)
endif()

if(WIN32)
    target_compile_definitions(LuminaPhoto PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_link_libraries(LuminaPhoto PRIVATE advapi32 shell32 ole32 windowscodecs)

    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)

    set(QT_PLUGINS_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<IF:$<CONFIG:Debug>,debug/,>Qt6/plugins")
    set(VCPKG_BIN_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/$<IF:$<CONFIG:Debug>,debug/,>bin")

    add_custom_command(TARGET LuminaPhoto POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${QT_PLUGINS_DIR}/platforms" "$<TARGET_FILE_DIR:LuminaPhoto>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${QT_PLUGINS_DIR}/styles" "$<TARGET_FILE_DIR:LuminaPhoto>/styles"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${QT_PLUGINS_DIR}/sqldrivers" "$<TARGET_FILE_DIR:LuminaPhoto>/sqldrivers"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${QT_PLUGINS_DIR}/imageformats" "$<TARGET_FILE_DIR:LuminaPhoto>/imageformats"
        COMMAND powershell -NoProfile -Command "if (Test-Path '${QT_PLUGINS_DIR}/iconengines') { Copy-Item -Path '${QT_PLUGINS_DIR}/iconengines' -Destination '$<TARGET_FILE_DIR:LuminaPhoto>/iconengines' -Recurse -Force }"
        COMMAND powershell -NoProfile -Command "Copy-Item -Path '${VCPKG_BIN_DIR}/*.dll' -Destination '$<TARGET_FILE_DIR:LuminaPhoto>' -Force -ErrorAction SilentlyContinue"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} "$<TARGET_FILE_DIR:LuminaPhoto>"
        COMMENT "Copying Qt plugins, vcpkg DLLs, and MSVC runtime..."
    )
endif()
