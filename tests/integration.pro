QT = core gui widgets network
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = integration
INCLUDEPATH += ../tinypng
SOURCES += integration.cpp \
  ../tinypng/Dispatcher.cpp ../tinypng/Compress.cpp ../tinypng/Utility.cpp \
  ../tinypng/Config.cpp ../tinypng/Console.cpp ../tinypng/TableModel.cpp \
  ../tinypng/MyTableView.cpp ../tinypng/MyOverlay.cpp ../tinypng/Scanner.cpp \
  ../tinypng/MainWindow.cpp ../tinypng/SettingWindow.cpp
HEADERS += ../tinypng/Dispatcher.h ../tinypng/Compress.h ../tinypng/Config.h \
  ../tinypng/Console.h ../tinypng/TableModel.h ../tinypng/MyTableView.h \
  ../tinypng/MyOverlay.h ../tinypng/Scanner.h \
  ../tinypng/MainWindow.h ../tinypng/SettingWindow.h
greaterThan(QT_MAJOR_VERSION, 5): CONFIG += c++17
win32-msvc*: QMAKE_CXXFLAGS += /utf-8

SOURCES += ../tinypng/Theme.cpp
HEADERS += ../tinypng/Theme.h

RESOURCES += ../tinypng/Resource.qrc
