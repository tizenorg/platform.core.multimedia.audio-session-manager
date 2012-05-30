Name:       audio-session-manager
Summary:    Audioxi Session Manager
Version:	0.1.19
Release:    1
Group:      TO_BE/FILLED_IN
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1001: packaging/audio-session-manager.manifest 
Requires(post): /sbin/ldconfig
Requires(post): /usr/bin/vconftool
Requires(postun): /sbin/ldconfig
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
Requires:   %{name}-devel = %{version}-%{release}

%description sdk-devel
auido-session-manager development package for sdk release for audio-session


%prep
%setup -q -n audio-session-manager-%{version}


%build
cp %{SOURCE1001} .

%autogen --disable-static --noconfigure
LDFLAGS="$LDFLAGS -Wl,--rpath=%{prefix}/lib -Wl,--hash-style=both -Wl,--as-needed "; export LDFLAGS
CFLAGS="%{optflags} -fvisibility=hidden -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\"" ; export CFLAGS
%configure --disable-static --enable-security
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install



%post 
/sbin/ldconfig

vconftool set -t int memory/Sound/SoundStatus "0" -i

mkdir -p /etc/rc.d/rc3.d
mkdir -p /etc/rc.d/rc4.d
ln -s /etc/rc.d/init.d/audiosessionmanager /etc/rc.d/rc3.d/S30audiosessionmanager
ln -s /etc/rc.d/init.d/audiosessionmanager /etc/rc.d/rc4.d/S30audiosessionmanager

%postun 
/sbin/ldconfig

rm -f /etc/rc.d/rc3.d/S30audiosessionmanager
rm -f /etc/rc.d/rc4.d/S30audiosessionmanager


%files
%manifest audio-session-manager.manifest
%{_sysconfdir}/rc.d/init.d/audiosessionmanager
%{_bindir}/audio-session-mgr-server
%{_libdir}/libaudio-session-mgr.so.*

%files devel
%manifest audio-session-manager.manifest
%{_includedir}/mmf/audio-session-manager-types.h
%{_includedir}/mmf/audio-session-manager.h

%files sdk-devel
%manifest audio-session-manager.manifest
%{_includedir}/mmf/audio-session-manager-types.h
%{_includedir}/mmf/audio-session-manager.h
%{_libdir}/libaudio-session-mgr.so
%{_libdir}/pkgconfig/audio-session-mgr.pc


