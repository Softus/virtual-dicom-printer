virtual-dicom-printer
=========

[![Buddy pipeline](https://app.buddy.works/pbludov/virtual-dicom-printer/pipelines/pipeline/129387/badge.svg?token=bf26fe8fed990190f11227bb2aa0c7d1e71118737795eed7b5069fff7106a015)](https://app.buddy.works/pbludov/virtual-dicom-printer/pipelines/pipeline/129387)
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

        sudo apt install lsb-release debhelper fakeroot libdcmtk2-dev \
          qt5-default libtesseract-dev

2. Make

        qmake virtual-dicom-printer.pro
        make

3. Install

        sudo make install

4. Create Package

        dpkg-buildpackage -us -uc -tc -I*.yml -Icache* -rfakeroot

CentOS
------

1. Install build dependecies

        sudo yum install -y redhat-lsb rpm-build git make cmake gcc-c++ \
          qt5-qtbase-devel tesseract-devel openssl-devel libxml2-devel git

2. Build DCMTK

        .ci/git-install.sh https://github.com/DCMTK/dcmtk.git DCMTK-3.6.3 \
          "-DCMAKE_INSTALL_PREFIX=/usr -DDCMTK_WITH_OPENSSL=OFF -DDCMTK_WITH_WRAP=OFF -DDCMTK_WITH_ICU=OFF -DDCMTK_WITH_ICONV=OFF"
3. Make

        qmake-qt5 virtual-dicom-printer.pro
        make

4. Install

        sudo make install

5. Create Package

        tar czf ../virtual-dicom-printer.tar.gz --exclude=cache* --exclude=debian \
          --exclude=*.yml * && rpmbuild -ta ../virtual-dicom-printer.tar.gz

Fedora
------

1. Install build dependecies

        sudo dnf install redhat-lsb rpm-build make gcc-c++ qt5-qtbase-devel \
          dcmtk-devel tesseract-devel openssl-devel libxml2-devel

2. Make

        qmake-qt5 virtual-dicom-printer.pro
        make

3. Install

        sudo make install

4. Create Package

        tar czf /tmp/virtual-dicom-printer.tar.gz * --exclude=.git && rpmbuild -ta /tmp/virtual-dicom-printer.tar.gz

Mageia
------

1. Install build dependecies

        sudo dnf install lsb-release rpm-build git make cmake gcc-c++ \
          qttools5 lib64qt5base5-devel lib64tesseract-devel git

2. Build DCMTK

        .ci/git-install.sh https://github.com/DCMTK/dcmtk.git DCMTK-3.6.3 \
          "-DCMAKE_INSTALL_PREFIX=/usr -DDCMTK_WITH_OPENSSL=OFF -DDCMTK_WITH_WRAP=OFF -DDCMTK_WITH_ICU=OFF -DDCMTK_WITH_ICONV=OFF"
3. Make

        qmake virtual-dicom-printer.pro
        make

4. Install

        sudo make install

5. Create Package

        tar czf ../virtual-dicom-printer.tar.gz --exclude=cache* --exclude=debian \
          --exclude=*.yml * && rpmbuild -ta ../virtual-dicom-printer.tar.gz

openSUSE
--------

1. Install build dependecies

        sudo zypper install lsb-release rpm-build make libqt5-qtbase-devel \
          dcmtk-devel tesseract-ocr-devel openssl-devel libxml2-devel

2. Make

        qmake-qt5 virtual-dicom-printer.pro
        make

3. Install

        sudo make install

4. Create Package

        tar czf /tmp/virtual-dicom-printer.tar.gz * --exclude=.git && rpmbuild -ta /tmp/virtual-dicom-printer.tar.gz

Windows (Visual Studio)
-----------------------

1. Install build dependecies

  * [CMake](https://cmake.org/download/)
  * [pkg-config](http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/)
  * [Qt 5.5 MSVC](https://download.qt.io/archive/qt/5.5/)
  * [DCMTK](http://dcmtk.org/dcmtk.php.en)

2. Build 3-rd party libraries

        # DCMTK
        cd dcmtk
        mkdir build && cd build
        cmake -Wno-dev .. -DCMAKE_INSTALL_PREFIX=c:\usr -G "Visual Studio <version>" \
          -DDCMTK_WITH_OPENSSL=OFF -DDCMTK_WITH_ICU=OFF -DDCMTK_WITH_ICONV=OFF
        cmake --build . --target install

3. Make

        qmake-qt5 
        nmake -f Makefile.Release
