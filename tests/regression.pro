QT = core gui svg
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = regression
INCLUDEPATH += ../tinypng
SOURCES += regression.cpp ../tinypng/Utility.cpp
greaterThan(QT_MAJOR_VERSION, 5): CONFIG += c++17
win32-msvc*: QMAKE_CXXFLAGS += /utf-8
