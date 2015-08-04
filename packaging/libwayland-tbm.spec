Name:		libwayland-tbm
Version:	0.1.1
Release:	0
Summary:	Wayland TBM Protocol
License:	MIT
Group:		Graphics & UI Framework/Wayland Window System
URL:		http://www.tizen.org/

Source: %name-%version.tar.gz
Source1001:	%name.manifest
BuildRequires:  autoconf automake
BuildRequires:  libtool
BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(wayland-server)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(libtbm)
BuildRequires:  pkgconfig(libdrm)

%description
Wayland TBM Protocol for TIZEN

# libwayland-tbm-server package
%package -n libwayland-tbm-server
Group:		Graphics & UI Framework/Wayland Window System
Summary:	Wayland TBM server library

%description -n libwayland-tbm-server
Wayland tbm is a protocol for graphics memory management for TIZEN

# libwayland-tbm-client package
%package -n libwayland-tbm-client
Group:		Graphics & UI Framework/Wayland Window System
Summary:	Wayland TBM client library

%description -n libwayland-tbm-client
Wayland tbm is a protocol for graphics memory management for TIZEN

# libwayland-tbm devel package
%package devel
Summary:    Development header files
Group:		Graphics & UI Framework/Development
Requires:	libwayland-tbm-server = %version
Requires:	libwayland-tbm-client = %version
Requires:   pkgconfig(libtbm)

%description devel
Development header files for use with Wayland protocol

%prep
%setup -q
cp %{SOURCE1001} .

%build
%reconfigure
make %{?_smp_mflags}

%install
%make_install

%post -n libwayland-tbm-client -p /sbin/ldconfig
%postun -n libwayland-tbm-client -p /sbin/ldconfig

%post -n libwayland-tbm-server -p /sbin/ldconfig
%postun -n libwayland-tbm-server -p /sbin/ldconfig

%files -n libwayland-tbm-server
%manifest %{name}.manifest
%license COPYING
%defattr(-,root,root)
%_libdir/libwayland-tbm-server.so.0*

%files -n libwayland-tbm-client
%manifest %{name}.manifest
%license COPYING
%defattr(-,root,root)
%_libdir/libwayland-tbm-client.so.0*

%files devel
%manifest %{name}.manifest
%defattr(-,root,root)
%_includedir/wayland-tbm*.h
%_libdir/libwayland-tbm*.so
%_libdir/pkgconfig/wayland-tbm*.pc
%doc README TODO

%changelog
