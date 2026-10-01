QT += core gui quick testlib
CONFIG += testcase c++20
TEMPLATE = app
TARGET = tst_omawrite

INCLUDEPATH += ../src
SOURCES += \
    tst_omawrite.cpp \
    ../src/backend.cpp \
    ../src/markdownhighlighter.cpp
HEADERS += \
    ../src/backend.h \
    ../src/markdownhighlighter.h

QT += widgets printsupport quickcontrols2 quickdialogs2 dbus
LIBS += -lmd4c

HEADERS += ../src/qsourcehighlite/qsourcehighliter.h ../src/qsourcehighlite/languagedata.h ../src/qsourcehighlite/qsourcehighliterthemes.h
SOURCES += ../src/qsourcehighlite/qsourcehighliter.cpp ../src/qsourcehighlite/languagedata.cpp ../src/qsourcehighlite/qsourcehighliterthemes.cpp
