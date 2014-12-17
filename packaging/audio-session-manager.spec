Name:       audio-session-manager
Summary:    Audio Session Manager
<<<<<<< HEAD
Version:    0.2.7
=======
Version:    0.4.6
>>>>>>> 319c01a... Remove RESUME CANCEL feature
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
CFLAGS="%{optflags} -fvisibility=hidden -DMM_DEBUG_FLAG -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\"" ; export CFLAGS
%reconfigure --disable-static
%__make %{?jobs:-j%jobs}

%install
%make_install

%post
/sbin/ldconfig
vconftool set -t int memory/Sound/SoundStatus "0" -g 29 -f -i

%postun -p /sbin/ldconfig

%files
%manifest %{name}.manifest
%license LICENSE
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
