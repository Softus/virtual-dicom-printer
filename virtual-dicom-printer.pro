# Copyright (C) 2013-2018 Softus Inc.
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

lessThan(QT_MAJOR_VERSION, 5): error (QT 5.0 or newer is required)

isEmpty(PREFIX): PREFIX = /usr
DEFINES     += PREFIX=$$PREFIX
CONFIG      += link_pkgconfig c++11
QT          += network
QT          -= gui
LIBS        += -ldcmpstat -ldcmnet -ldcmdata -ldcmimgle -ldcmdsig -ldcmsr -ldcmtls -ldcmqrdb -lxml2 -loflog -lofstd -lz
unix:LIBS   += -lssl
win32:LIBS  += -lws2_32 -ladvapi32 -lnetapi32

OPTIONAL_LIBS = lept tesseract
for (mod, OPTIONAL_LIBS) {
    modVer = $$system(pkg-config --silence-errors --modversion $$mod)
    isEmpty(modVer) {
        message("Optional package $$mod not installed")
    } else {
        message("Found $$mod version $$modVer")
        PKGCONFIG += $$mod
        DEFINES += WITH_$$upper($$replace(mod, \W, _))
    }
}

TARGET   = virtual-dicom-printer
CONFIG  += console
CONFIG  -= app_bundle

TEMPLATE = app
SOURCES += main.cpp \
    printscp.cpp \
    storescp.cpp \
    transcyrillic.cpp

HEADERS += \
    printscp.h \
    product.h \
    storescp.h \
    transcyrillic.h \
    qutf8settings.h \
    QUtf8Settings

target.path=$$PREFIX/bin
man.files=virtual-dicom-printer.1
man.path=$$PREFIX/share/man/man1
cfg.files=virtual-dicom-printer.conf
cfg.path=/etc/xdg/softus.org
syslog.files=99-virtual-dicom-printer.conf
syslog.path=/etc/rsyslog.d
systemd.files=virtual-dicom-printer.service
equals(OS_DISTRO, debian) | equals(OS_DISTRO, ubuntu) {
    systemd.path=/lib/systemd/system
} else {
    systemd.path=/usr/lib/systemd/system
}

INSTALLS += target man cfg syslog systemd
