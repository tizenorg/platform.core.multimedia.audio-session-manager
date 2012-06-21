Name:       audio-session-manager
Summary:    Audioxi Session Manager
Version:	0.1.19
Release:    1
Group:      System/Main
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source101:  audio-session-manager.service
Source1001: packaging/audio-session-manager.manifest 

Requires(post): /sbin/ldconfig
Requires(post): /usr/bin/systemctl
Requires(post): /usr/bin/vconftool
Requires(postun): /sbin/ldconfig
Requires(postun): /usr/bin/systemctl
Requires(preun): /usr/bin/systemctl

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

mkdir -p %{buildroot}/etc/rc.d/rc3.d
mkdir -p %{buildroot}/etc/rc.d/rc4.d
ln -s ../etc/rc.d/init.d/audiosessionmanager %{buildroot}/%{_sysconfdir}/rc.d/rc3.d/S30audiosessionmanager
ln -s ../etc/rc.d/init.d/audiosessionmanager %{buildroot}/%{_sysconfdir}/rc.d/rc4.d/S30audiosessionmanager

mkdir -p %{buildroot}%{_libdir}/systemd/system/multi-user.target.wants
install -m 0644 %SOURCE101 %{buildroot}%{_libdir}/systemd/system/
ln -s ../audio-session-manager.service %{buildroot}%{_libdir}/systemd/system/multi-user.target.wants/audio-session-manager.service


%preun
if [ $1 == 0 ]; then
    systemctl stop audio-session-manager.service
fi

%post 
/sbin/ldconfig
if [ $1 == 1 ]; then
    systemctl restart audio-session-manager.service
fi
vconftool set -t int memory/Sound/SoundStatus "0" -i

%postun 
/sbin/ldconfig
systemctl daemon-reload


%files
%manifest audio-session-manager.manifest
%{_sysconfdir}/rc.d/init.d/audiosessionmanager
%{_sysconfdir}/rc.d/rc3.d/S30audiosessionmanager
%{_sysconfdir}/rc.d/rc4.d/S30audiosessionmanager
%{_bindir}/audio-session-mgr-server
%{_libdir}/libaudio-session-mgr.so.*
%{_libdir}/systemd/system/audio-session-manager.service
%{_libdir}/systemd/system/multi-user.target.wants/audio-session-manager.service

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


