QT += core gui widgets printsupport qml quick quickcontrols2 quickdialogs2 dbus

CONFIG += c++20 release
TARGET = omawrite
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/markdownhighlighter.h \
    src/systemtheme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/markdownhighlighter.cpp \
    src/systemtheme.cpp

RESOURCES += src/resources.qrc
LIBS += -lmd4c

HEADERS += src/qsourcehighlite/qsourcehighliter.h src/qsourcehighlite/languagedata.h src/qsourcehighlite/qsourcehighliterthemes.h
SOURCES += src/qsourcehighlite/qsourcehighliter.cpp src/qsourcehighlite/languagedata.cpp src/qsourcehighlite/qsourcehighliterthemes.cpp
