Name:           efivar
Version:        0.5
Release:        0.1%{?dist}
Summary:        Tools to manage UEFI variables
License:        LGPLv2.1
URL:            https://github.com/vathpela/libefivar
BuildRequires:	popt-devel
# this archive is built from the tag corresponding with %{version}
# using the "archive" make target in the repository.
Source0:        %{name}-%{version}.tar.bz2

%description
efivar provides a simple command line interface to the UEFI variable facility.

%package libs
Summary: Library to manage UEFI variables

%description libs
Library to allow for the simple manipulation of UEFI variables.

%package devel
Summary: development headers for libefivar

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
%doc
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
* Thu Sep 05 2013 Peter Jones <pjones@redhat.com> - 0.5-0.1
- Update to 0.5

* Mon Jun 17 2013 Peter Jones <pjones@redhat.com> - 0.4-0.2
- Fix ldconfig invocation

* Mon Jun 17 2013 Peter Jones <pjones@redhat.com> - 0.4-0.1
- Initial spec file
