cmake_minimum_required(VERSION 3.16)

project(IDE VERSION 0.1 LANGUAGES CXX)

add_compile_definitions(PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(IDE
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        resources.qrc
        syntaxhighlighter.h syntaxhighlighter.cpp
        terminal.h terminal.cpp
        loginwindow.h loginwindow.cpp loginwindow.ui
        authenticator.h authenticator.cpp
        storage.h storage.cpp
        config.h


    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET IDE APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(IDE SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(IDE
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(IDE PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network)
target_link_libraries(IDE PRIVATE Qt${QT_VERSION_MAJOR}::Core)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.IDE)
endif()
set_target_properties(IDE PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS IDE
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Bundle a portable MinGW-w64 toolchain next to IDE.exe so Run works on
# Windows with no separate compiler install. See windows/README.md for
# where to obtain one -- this step is a no-op (with a warning) until a
# toolchain has actually been placed there.
if(WIN32)
    set(MINGW_BUNDLE_DIR "${CMAKE_SOURCE_DIR}/windows/mingw64")
    if(EXISTS "${MINGW_BUNDLE_DIR}")
        add_custom_command(TARGET IDE POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${MINGW_BUNDLE_DIR}" "$<TARGET_FILE_DIR:IDE>/mingw64"
            COMMENT "Copying bundled MinGW-w64 toolchain next to IDE.exe"
        )
        install(DIRECTORY "${MINGW_BUNDLE_DIR}/" DESTINATION "${CMAKE_INSTALL_BINDIR}/mingw64")
    else()
        message(STATUS "windows/mingw64/ not found -- IDE.exe will fall back to g++ on PATH. "
                        "See windows/README.md to bundle a toolchain before packaging a release.")
    endif()
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(IDE)
endif()
