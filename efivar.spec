Name:           efivar
Version:        0.7
Release:        1%{?dist}
Summary:        Tools to manage UEFI variables
License:        LGPLv2.1
URL:            https://github.com/vathpela/efivar
Requires:       %{name}-libs = %{version}-%{release}

BuildRequires:  popt-devel
Source0:        https://github.com/vathpela/%{name}/archive/%{version}.tar.gz

%description
efivar provides a simple command line interface to the UEFI variable facility.

%package libs
Summary: Library to manage UEFI variables

%description libs
Library to allow for the simple manipulation of UEFI variables.

%package devel
Summary: Development headers for libefivar
Requires: %{name}-libs = %{version}-%{release}

%description devel
development headers required to use libefivar.

%prep
%setup -q -n %{name}-%{version}
git init
git config user.email "shim-owner@fedoraproject.org"
git config user.name "Fedora Ninjas"
git add .
git commit -a -q -m "%{version} baseline."
git am %{patches} </dev/null

%build
make libdir=%{_libdir} bindir=%{_bindir} OPT_FLAGS="$RPM_OPT_FLAGS"

%install
rm -rf $RPM_BUILD_ROOT
%makeinstall

%clean
rm -rf $RPM_BUILD_ROOT

%post libs -p /sbin/ldconfig

%postun libs -p /sbin/ldconfig

%files
%doc COPYING README
%{_bindir}/efivar
%{_mandir}/man1/*

%files devel
%{_mandir}/man3/*
%{_includedir}/*
%{_libdir}/*.so
%{_libdir}/pkgconfig/*.pc

%files libs
%{_libdir}/*.so.*

%changelog
* Fri Oct 25 2013 Peter Jones <pjones@redhat.com> - 0.7-1
- Update package to 0.7
- adds --append support to the binary.

* Fri Sep 06 2013 Peter Jones <pjones@redhat.com> - 0.6-1
- Update package to 0.6
- fixes to documentation from lersek
- more validation of uefi guids
- use .xz for archives

* Thu Sep 05 2013 Peter Jones <pjones@redhat.com> - 0.5-0.1
- Update to 0.5

* Mon Jun 17 2013 Peter Jones <pjones@redhat.com> - 0.4-0.2
- Fix ldconfig invocation

* Mon Jun 17 2013 Peter Jones <pjones@redhat.com> - 0.4-0.1
- Initial spec file
