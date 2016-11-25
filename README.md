virtual-dicom-printer
=========

[![Build Status](https://api.travis-ci.org/Softus/virtual-dicom-printer.svg?branch=master)](https://travis-ci.org/Softus/virtual-dicom-printer)
[![Build status](https://ci.appveyor.com/api/projects/status/82ofqvp1710uwq3o?svg=true)](https://ci.appveyor.com/project/pbludov/virtual-dicom-printer)

Introduction
============

Virtual printer for DICOM.
  Works as a proxy and spooler for a real printer(s).
  Also, all prints may be archived in a DICOM storage server.

Requirements
============

* [Qt](http://qt-project.org/) 5.0.2 or higher;
* [DCMTK](http://dcmtk.org/) 3.6.0 or higher;

Installation
============

Debian/Ubuntu/Mint
------------------

1. Install build dependecies

        sudo apt install libdcmtk2-dev libleptonica-dev libqt5network5 \
        libwrap0-dev libssl-dev libtesseract-dev qt5-default

2. Make

        qmake virtual-dicom-printer.pro
        make

3. Install

        sudo make install

4. Create Package

        dpkg-buildpackage -us -uc -I.git -I*.sh -rfakeroot

Windows (Visual Studio)
-----------------------

1. Install build dependecies

  * [CMake](https://cmake.org/download/)
  * [pkg-config](http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/)
  * [Qt 5.5 MSVC](https://download.qt.io/archive/qt/5.5/)
  * [DCMTK (optional)](http://dcmtk.org/dcmtk.php.en)

2. Build 3-rd party libraries

        # DCMTK
        cd dcmtk
        mkdir build && cd build
        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=c:\usr -G "Visual Studio <version>"
        cmake --build . --target install

3. Make

        qmake-qt5 
        nmake -f Makefile.Release
