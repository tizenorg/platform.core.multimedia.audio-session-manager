Name:       audio-session-manager
Summary:    Audio Session Manager
Version:    0.3.1
Release:    0
Group:      Multimedia/Service
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1001: audio-session-manager.manifest
Requires(post): /sbin/ldconfig
Requires(post): /usr/bin/vconftool
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(mm-common)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(gio-2.0)

%description
Audio Session Manager package.

%package devel
Summary:    Audio Session Manager package  (devel)
Group:      Multimedia/Development
Requires:   %{name} = %{version}-%{release}

%description devel
Audio Session Manager package  (devel) package.
%devel_desc

%package sdk-devel
Summary:    Audio Session Manager development package for sdk release
Group:      Multimedia/Development
Requires:   %{name}-devel = %{version}-%{release}

%description sdk-devel
Audio Session Manager development package for sdk release package.
%devel_desc
SDK Release.

%prep
%setup -q
cp %{SOURCE1001} .

%build

%autogen --disable-static --noconfigure
LDFLAGS="$LDFLAGS -Wl,--rpath=%{prefix}/lib -Wl,--hash-style=both -Wl,--as-needed "; export LDFLAGS
CFLAGS="%{optflags} -fvisibility=hidden -DSUPPORT_CONTAINER -DMM_DEBUG_FLAG -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\"" ; export CFLAGS
%configure --disable-static --disable-security
make %{?jobs:-j%jobs}

%install
%make_install

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/libaudio-session-mgr.so.*
%{_bindir}/asm_testsuite

%files devel
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_includedir}/mmf/audio-session-manager-types.h
%{_includedir}/mmf/audio-session-manager.h

%files sdk-devel
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_includedir}/mmf/audio-session-manager-types.h
%{_includedir}/mmf/audio-session-manager.h
%{_libdir}/libaudio-session-mgr.so
%{_libdir}/pkgconfig/audio-session-mgr.pc
