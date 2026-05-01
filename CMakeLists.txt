cmake_minimum_required(VERSION 3.21)

project(CreationTheoryMorph VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
find_package(OpenGL REQUIRED)

set(APP_SOURCES
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/GLViewport.h
    src/GLViewport.cpp
    src/MeshObject.h
    src/MeshObject.cpp
    src/StlLoader.h
    src/StlLoader.cpp
    src/ObjLoader.h
    src/ObjLoader.cpp
    src/HistoryEntry.h
    src/TimelineWidget.h
    src/TimelineWidget.cpp
    src/SceneBrowserWidget.h
    src/SceneBrowserWidget.cpp
    src/PreferencesDialog.h
    src/PreferencesDialog.cpp
    src/Version.h
    src/Units.h
)

qt_add_executable(CreationTheoryMorph WIN32 MACOSX_BUNDLE ${APP_SOURCES})

target_include_directories(CreationTheoryMorph PRIVATE src)

target_link_libraries(CreationTheoryMorph PRIVATE
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    OpenGL::GL
)

if(APPLE)
    set_target_properties(CreationTheoryMorph PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Creation Theory Morph"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.creationtheory.creationtheorymorph"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
endif()
