cmake_minimum_required(VERSION 3.21)
project(Lumina VERSION 0.1.0 DESCRIPTION "Lumina media player (Qt 6 / QML / libVLC)" LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(LUMINA_BUILD_TESTS "Build the test suite" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Sql Concurrent Network Svg)
find_package(LibVLC REQUIRED)
message(STATUS "libVLC: ${LIBVLC_LIBRARY} (${LibVLC_VERSION})")

# ---------------------------------------------------------------- core library
file(GLOB_RECURSE LUMINA_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
list(FILTER LUMINA_CORE_SOURCES EXCLUDE REGEX ".*/src/main\\.cpp$")

add_library(lumina_core STATIC ${LUMINA_CORE_SOURCES})
target_include_directories(lumina_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(lumina_core PUBLIC cxx_std_17)
target_link_libraries(lumina_core PUBLIC
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Sql Qt6::Concurrent Qt6::Network Qt6::Svg
    LibVLC::LibVLC)
target_compile_definitions(lumina_core PUBLIC
    LUMINA_VERSION="${PROJECT_VERSION}" QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
if(MSVC)
    target_compile_options(lumina_core PUBLIC /W4 /permissive- /utf-8)
else()
    target_compile_options(lumina_core PUBLIC -Wall -Wextra -Wpedantic)
endif()

# ------------------------------------------------------------------ executable
qt_add_executable(lumina src/main.cpp)
if(WIN32)
    # Embed the application icon (exe, taskbar, Explorer).
    enable_language(RC)
    target_sources(lumina PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources/lumina.rc")
endif()
set_target_properties(lumina PROPERTIES WIN32_EXECUTABLE ON MACOSX_BUNDLE ON)
target_link_libraries(lumina PRIVATE lumina_core)

file(GLOB_RECURSE LUMINA_QML CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "qml/*.qml")
foreach(f IN LISTS LUMINA_QML)
    if(f MATCHES "(Theme|Actions)\\.qml$")
        set_source_files_properties("${f}" PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
    endif()
endforeach()
file(GLOB_RECURSE LUMINA_ICONS CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "resources/icons/*.svg")

qt_add_qml_module(lumina
    URI Lumina.UI
    VERSION 1.0
    QML_FILES ${LUMINA_QML}
)

file(GLOB LUMINA_FONTS CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "resources/fonts/*.otf")
qt_add_resources(lumina lumina_assets PREFIX "/" FILES ${LUMINA_ICONS} ${LUMINA_FONTS})

# ------------------------------------------------------------------- testing
if(LUMINA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ------------------------------------------------------------------ install
include(GNUInstallDirs)
install(TARGETS lumina RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} BUNDLE DESTINATION .)
