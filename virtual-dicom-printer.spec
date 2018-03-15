Name: virtual-dicom-printer
Provides: virtual-dicom-printer
Version: 1.3
Release: 1%{?dist}
License: LGPL-2.1+
Source: %{name}.tar.gz
URL: http://softus.org/products/virtual-dicom-printer
Vendor: Softus Inc. <contact@softus.org>
Packager: Softus Inc. <contact@softus.org>
Summary: Virtual printer for DICOM.

%description
Virtual printer for DICOM.

Works as a proxy and spooler for a real printer(s).
Also, all prints may be archived in a DICOM storage server.

%global debug_package %{nil}

Requires: dcmtk, redhat-lsb-core
BuildRequires: make, gcc-c++, systemd

%{?rhl:BuildRequires: qt5-qtbase-devel, dcmtk-devel, openssl-devel, tesseract-devel, libxml2-devel}

%{?fedora:BuildRequires: qt-devel, dcmtk-devel, openssl-devel, tesseract-devel, libxml2-devel}

%{?suse_version:BuildRequires: libqt5-qtbase-devel, dcmtk-devel, openssl-devel, tesseract-ocr-devel, libxml2-devel}

%if 0%{?mageia}
%define qmake qmake
BuildRequires: qttools5
%ifarch x86_64 amd64
BuildRequires: lib64qt5base5-devel, lib64tesseract-devel
%else
BuildRequires: libqt5base5-devel, libtesseract-devel
%endif
%else
%define qmake qmake-qt5
%endif

%prep
%setup -c %{name}
 
%build
%{qmake} PREFIX=%{_prefix} QMAKE_CFLAGS+="%optflags" QMAKE_CXXFLAGS+="%optflags";
make %{?_smp_mflags};

%install
make install INSTALL_ROOT="%buildroot";

%files
%defattr(-,root,root)
%config(noreplace) %{_sysconfdir}/xdg/softus.org/%{name}.conf
%config(noreplace) %{_sysconfdir}/rsyslog.d/99-%{name}.conf
%{_mandir}/man1/%{name}.1.*
%{_bindir}/%{name}
%{_unitdir}/%{name}.service

%pre
/usr/sbin/groupadd -r virtprint || :
/usr/sbin/useradd -rmb /var/lib -s /sbin/nologin -g virtprint virtprint || :
chown virtprint:virtprint /var/lib/virtprint
chmod 775 /var/lib/virtprint

%post
systemctl enable %{name}
service %{name} start || :

%preun
service %{name} stop || :

%postun
/usr/sbin/userdel virtprint || :
/usr/sbin/groupdel virtprint || :

%changelog
* Mon Mar 6 2017 Pavel Bludov <pbludov@gmail.com>
+ Version 1.2
- Change centos dependencies
