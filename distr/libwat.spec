#Workaround for 64 bit CPUs
%define _lib lib

Summary: LibWAT is an open source library that encapsulates the protocols used to communicate with Wireless GSM modules.
Name: libwat
Version: 2.1.0
Release: 1%{dist}
License: GPL
Group: Utilities/System
Source: %{name}-%{version}.tgz
BuildRoot: %{_tmppath}/%{name}-%{version}-root
URL: http://www.sangoma.com
Vendor: Sangoma Technologies
Packager: Bryan Walters <bwalters@sangoma.com>
BuildRequires: cmake

%description
LibWAT is an open source library that encapsulates the protocols used to communicate with Wireless GSM modules.
LibWAT is a dependency for Asterisk and DAHDI if GSM signaling is used.

%package devel
Summary: Libwat libraries and header files for libwat development
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
The static libraries and header files needed for building additional plugins/modules

%prep
%setup -n %{name}-%{version}

%build
mkdir -p build
cd build
cmake ../CMakeLists.txt
cd ..
make

%install
make DESTDIR=$RPM_BUILD_ROOT install

%post

%clean
cd $RPM_BUILD_DIR
%{__rm} -rf %{name}-%{version}
%{__rm} -rf /var/log/%{name}-%{version}-%{release}.make.err
%{__rm} -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root)
%{_libdir}/libwat.so
%{_libdir}/libwat.so.*

%files devel
%defattr(-, root, root)
%{_includedir}/libwat.h
%{_includedir}/wat_declare.h
