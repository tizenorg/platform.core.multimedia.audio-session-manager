Name:       audio-session-manager
Summary:    Audioxi Session Manager
Version:    0.1.14
Release:    1
Group:      TO_BE/FILLED_IN
License:    TO BE FILLED IN
Source0:    %{name}-%{version}.tar.gz
Requires(post): /sbin/ldconfig
Requires(post): /usr/bin/vconftool
Requires(postun): /sbin/ldconfig
BuildRequires:  vconf-keys-devel
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(mm-log)
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
Requires:   %{name} = %{version}-%{release}

%description sdk-devel
auido-session-manager development package for sdk release for audio-session


%prep
%setup -q -n audio-session-manager-%{version}


%build

%autogen --disable-static --noconfigure
LDFLAGS="$LDFLAGS -Wl,--rpath=%{prefix}/lib -Wl,--hash-style=both -Wl,--as-needed "; export LDFLAGS
CFLAGS="%{optflags} -fvisibility=hidden -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\"" ; export CFLAGS
%configure --disable-static --enable-security
make 

%install
rm -rf %{buildroot}
%make_install



%post 
/sbin/ldconfig

vconftool set -t int memory/Sound/SoundStatus "0" -i

%postun -p /sbin/ldconfig





%files
%defattr(-,root,root,-)
%{_sysconfdir}/rc.d/init.d/audiosessionmanager
%{_bindir}/audio-session-mgr-server
%{_libdir}/libaudio-session-mgr.so.*

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


