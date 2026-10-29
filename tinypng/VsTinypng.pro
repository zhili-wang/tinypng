# Qt Widgets desktop application (Qt 5.14+ / Qt 6).

TARGET = VsTinypng
VERSION = 1.2.0

QT = core gui widgets network

CONFIG += c++11
greaterThan(QT_MAJOR_VERSION, 5): CONFIG += c++17

HEADERS = \
   $$PWD/Compress.h \
   $$PWD/Dispatcher.h \
   $$PWD/Config.h \
   $$PWD/Console.h \
   $$PWD/GridTableHeaderModel.h \
   $$PWD/GridTableHeaderView.h \
   $$PWD/MainWindow.h \
   $$PWD/MyOverlay.h \
   $$PWD/MyTableView.h \
   $$PWD/resource.h \
   $$PWD/Scanner.h \
   $$PWD/SettingWindow.h \
   $$PWD/SingleApplication.h \
   $$PWD/TableHeaderItem.h \
   $$PWD/TableModel.h \
   $$PWD/Utility.h \
   $$PWD/WorkspaceUi.h \
   $$PWD/QueueAppearance.h \

SOURCES = \
   $$PWD/Compress.cpp \
   $$PWD/Dispatcher.cpp \
   $$PWD/Config.cpp \
   $$PWD/Console.cpp \
   $$PWD/GridTableHeaderModel.cpp \
   $$PWD/GridTableHeaderView.cpp \
   $$PWD/main.cpp \
   $$PWD/MainWindow.cpp \
   $$PWD/MyOverlay.cpp \
   $$PWD/MyTableView.cpp \
   $$PWD/Scanner.cpp \
   $$PWD/SettingWindow.cpp \
   $$PWD/SingleApplication.cpp \
   $$PWD/TableHeaderItem.cpp \
   $$PWD/TableModel.cpp \
   $$PWD/Utility.cpp \

INCLUDEPATH = \
    $$PWD/.

RC_ICONS = logo.ico
ICON = icon.icns
RESOURCES += \
    Resource.qrc

#DEFINES = 

win32-msvc*: QMAKE_CXXFLAGS += /utf-8

SOURCES += $$PWD/Theme.cpp
HEADERS += $$PWD/Theme.h
