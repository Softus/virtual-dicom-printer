# Copyright (C) 2013-2017 Softus Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; version 2.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

isEmpty(PREFIX): PREFIX   = /usr
DEFINES += PREFIX=$$PREFIX

QT          += core network
QT          -= gui
LIBS        += -ldcmpstat -ldcmnet -ldcmdata -ldcmimgle -ldcmdsig -ldcmsr -ldcmtls -ldcmqrdb -lxml2 -loflog -lofstd -lz
unix:LIBS   += -lwrap -lssl
win32:LIBS  += -lws2_32 -ladvapi32 -lnetapi32

# GCC tuning
*-g++*:QMAKE_CXXFLAGS += -std=c++0x -Wno-multichar

# Tell qmake to use pkg-config to find OCR library.
CONFIG += link_pkgconfig

!notesseract:packagesExist(tesseract) {
    PKGCONFIG += tesseract
    DEFINES += WITH_TESSERACT
}

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

target.path=$$PREFIX/bin
man.files=virtual-dicom-printer.1
man.path=$$PREFIX/share/man/man1
cfg.files=virtual-dicom-printer.conf
cfg.path=/etc/xdg/softus.org
initd.files=init.d/virtual-dicom-printer
initd.path=/etc/rc.d/init.d
systemd.files=systemd/virtual-dicom-printer.service
systemd.path=/lib/systemd/system

INSTALLS += target man cfg initd systemd
