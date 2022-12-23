cmake_minimum_required(VERSION 3.5)

project(qsane LANGUAGES CXX C)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(GIT_COMMIT_LAST_TAG "0.0.0")
find_package(Git)
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE GIT_COMMIT_LAST_TAG
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

configure_file("src/version.h.in" "${CMAKE_SOURCE_DIR}/src/_version.h")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DQT_NO_CAST_TO_ASCII
                #-DQT_NO_CAST_FROM_ASCII
                -DQT_NO_URL_CAST_FROM_STRING
                -DQT_NO_CAST_FROM_BYTEARRAY
                -DQT_NO_SIGNALS_SLOTS_KEYWORDS
                -DQT_USE_QSTRINGBUILDER
                -DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT
                #-DQT_STRICT_ITERATORS
                -DQT_NO_KEYWORDS
                -DQT_DISABLE_DEPRECATED_BEFORE=0x060000
                -DQT_NO_FOREACH)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(SOURCES
  libs/qtpropertybrowser/src/qtbuttonpropertybrowser.cpp
  libs/qtpropertybrowser/src/qteditorfactory.cpp
  libs/qtpropertybrowser/src/qtgroupboxpropertybrowser.cpp
  libs/qtpropertybrowser/src/qtpropertybrowser.cpp
  libs/qtpropertybrowser/src/qtpropertybrowserutils.cpp
  libs/qtpropertybrowser/src/qtpropertymanager.cpp
  libs/qtpropertybrowser/src/qttreepropertybrowser.cpp
  libs/qtpropertybrowser/src/qtvariantproperty.cpp
  src/main.cpp
  src/GraphicsImageItem.cpp
  src/Scanner.cpp
  src/MainWindow.cpp
  src/MainWindow.ui
  src/PageView.cpp
  src/CropRect.cpp
  src/DevicePropertyBrowser.cpp
  src/WorkerThread.cpp
  src/resources.qrc
)

file(GLOB_RECURSE HEADERS include *.h *.hpp)
if(WIN32)
    set(HEADERS ${HEADERS} src/resources.rc src/qt5.natvis)
endif()
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})

set(TRANSLATIONS
  translations/lang_de.ts
)
find_package(Qt5LinguistTools)
qt5_add_translation(QM_FILES ${TRANSLATIONS})
add_custom_target(translations ALL DEPENDS ${QM_FILES})

add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE ${SOURCES} ${HEADERS})
add_dependencies(${PROJECT_NAME} translations)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets sane)

target_include_directories(${PROJECT_NAME} PRIVATE src libs)
