Summary: Virtual printer for DICOM.
Name: virtual-dicom-printer
Provides: virtual-dicom-printer
Version: 1.3
Release: 1%{?dist}
License: LGPL-2.1+
Source: %{name}.tar.gz
URL: http://softus.org/products/virtual-dicom-printer
Vendor: Softus Inc. <contact@softus.org>
Packager: Softus Inc. <contact@softus.org>

Requires: dcmtk, redhat-lsb-core
BuildRequires: make, gcc-c++
BuildRequires: dcmtk-devel, openssl-devel, tesseract-devel, tcp_wrappers-devel, libxml2-devel

%{?rhl:Requires: qt5-qtbase}
%{?rhl:BuildRequires: qt5-qtbase-devel}

%{?fedora:Requires: qt5}
%{?fedora:BuildRequires: qt-devel}

%description
Virtual printer for DICOM.

Works as a proxy and spooler for a real printer(s).
Also, all prints may be archived in a DICOM storage server.

%global debug_package %{nil}

%define _rpmfilename %%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm

%prep
%setup -c %{name}
 
%build
qmake-qt5 PREFIX=%{_prefix} QMAKE_CFLAGS+="%optflags" QMAKE_CXXFLAGS+="%optflags";
make -j 2 %{?_smp_mflags};

%install
make install INSTALL_ROOT="%buildroot";

%files
%defattr(-,root,root)
%config(noreplace) %{_sysconfdir}/xdg/softus.org/%{name}.conf
%config(noreplace) %{_sysconfdir}/sysconfig/%{name}
%{_initddir}/virtual-dicom-printer
%{_mandir}/man1/%{name}.1.gz
%{_bindir}/%{name}

%changelog
* Mon Mar 6 2017 Pavel Bludov <pbludov@gmail.com>
+ Version 1.2
- Change centos dependencies
