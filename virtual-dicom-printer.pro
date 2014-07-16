#-------------------------------------------------
#
# Project created by QtCreator 2014-07-08T15:00:28
#
#-------------------------------------------------

QT          += core network
QT          -= gui
LIBS        += -ldcmpstat -ldcmnet -ldcmdata -ldcmimgle -loflog -lofstd -lz -ltesseract
unix:LIBS   += -lwrap -lssl
# GCC tuning
*-g++*:QMAKE_CXXFLAGS += -std=c++0x -Wno-multichar

TARGET = virtual-dicom-printer
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app
SOURCES += main.cpp \
    printscp.cpp \
    storescp.cpp \
    transcyrillic.cpp

HEADERS += \
    printscp.h \
    product.h \
    storescp.h \
    transcyrillic.h
