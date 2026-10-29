QT = core gui widgets
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = ui-preview
INCLUDEPATH += ../tinypng
SOURCES += ui-preview.cpp
greaterThan(QT_MAJOR_VERSION, 5): CONFIG += c++17
win32-msvc*: QMAKE_CXXFLAGS += /utf-8
