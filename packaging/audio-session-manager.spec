Name:       audio-session-manager
Summary:    Audio Session Manager
Version:    0.2.7
Release:    0
Group:      System/Libraries
License:    Apache License, Version 2.0
URL:        http://source.tizen.org
Source0:    %{name}-%{version}.tar.gz
Requires(post): /sbin/ldconfig
Requires(post): /usr/bin/vconftool
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(mm-common)
BuildRequires:  pkgconfig(sysman)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(avsysaudio)
BuildRequires:  pkgconfig(security-server)


%description
audio-session-manager development package 



%package devel
Summary:    Audio-session-manager package  (devel)
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
Audio-session-manager development package  (devel)


%package sdk-devel
Summary:    auido-session-manager development package for sdk release
Group:      Development/Libraries
Requires:   %{name}-devel = %{version}-%{release}

%description sdk-devel
auido-session-manager development package for sdk release for audio-session


%prep
%setup -q -n audio-session-manager-%{version}


%build

%autogen --disable-static --noconfigure
LDFLAGS="$LDFLAGS -Wl,--rpath=%{prefix}/lib -Wl,--hash-style=both -Wl,--as-needed "; export LDFLAGS
CFLAGS="%{optflags} -fvisibility=hidden -DMM_DEBUG_FLAG -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\"" ; export CFLAGS
%configure --disable-static --enable-security
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install



%post 
/sbin/ldconfig

vconftool set -t int memory/Sound/SoundStatus "0" -g 29 -f -i

%postun 
/sbin/ldconfig

%files
%manifest audio-session-manager.manifest
%defattr(-,root,root,-)
%{_libdir}/libaudio-session-mgr.so.*
%{_bindir}/asm_testsuite

%files devel
%defattr(-,root,root,-)
%{_includedir}/mmf/audio-session-manager-types.h
%{_includedir}/mmf/audio-session-manager.h


%files sdk-devel
%defattr(-,root,root,-)
%{_includedir}/mmf/audio-session-manager-types.h
%{_includedir}/mmf/audio-session-manager.h
%{_libdir}/libaudio-session-mgr.so
%{_libdir}/pkgconfig/audio-session-mgr.pc


