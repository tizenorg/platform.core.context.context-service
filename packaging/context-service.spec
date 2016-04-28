Name:       context-service
Summary:    Context-Service
Version:    0.7.5
Release:    1
Group:      Service/Context
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1:	context-service.service
Source2:	org.tizen.context.conf

%define BUILD_PROFILE %{?profile}%{!?profile:%{?tizen_profile_name}}

%if "%{?BUILD_PROFILE}" == "tv"
ExcludeArch: %{arm} aarch64 %ix86 x86_64
%endif

BuildRequires: cmake
BuildRequires: sed
BuildRequires: pkgconfig(gmodule-2.0)
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(capi-system-info)
BuildRequires: pkgconfig(capi-system-device)
BuildRequires: pkgconfig(capi-appfw-app-manager)
BuildRequires: pkgconfig(capi-appfw-package-manager)
BuildRequires: pkgconfig(appsvc)
BuildRequires: pkgconfig(notification)

BuildRequires: pkgconfig(cynara-creds-gdbus)
BuildRequires: pkgconfig(cynara-client)
BuildRequires: pkgconfig(cynara-session)

BuildRequires: pkgconfig(libcontext-server)
BuildRequires: pkgconfig(context)
BuildRequires: context-provider-devel
Requires: context-provider

Requires(preun): /usr/bin/systemctl
Requires(post): /usr/bin/systemctl
Requires(postun): /usr/bin/systemctl

%description
Context-Service

%prep
%setup -q

%build
MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`

export   CFLAGS+=" -Wextra -Wcast-align -Wshadow -Wwrite-strings -Wswitch-default -Wno-unused-parameter"
export CXXFLAGS+=" -Wextra -Wcast-align -Wshadow -Wwrite-strings -Wswitch-default -Wno-unused-parameter"

export   CFLAGS+=" -Wno-empty-body -fomit-frame-pointer -fno-optimize-sibling-calls"
export CXXFLAGS+=" -Wno-empty-body -fomit-frame-pointer -fno-optimize-sibling-calls"

export   CFLAGS+=" -fno-strict-aliasing -fno-unroll-loops -fsigned-char -fstrict-overflow"
export CXXFLAGS+=" -fno-strict-aliasing -fno-unroll-loops -fsigned-char -fstrict-overflow"

export   CFLAGS+=" -fno-common"
export CXXFLAGS+=" -Wnon-virtual-dtor"
export CXXFLAGS+=" -std=c++11 -Wno-c++11-compat"

#export   CFLAGS+=" -Wcast-qual"
#export CXXFLAGS+=" -Wcast-qual"

#export   CFLAGS+=" -DTIZEN_ENGINEER_MODE"
#export CXXFLAGS+=" -DTIZEN_ENGINEER_MODE"
#export   FFLAGS+=" -DTIZEN_ENGINEER_MODE"

cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DMAJORVER=${MAJORVER} -DFULLVER=%{version}
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}%{_unitdir_user}
mkdir -p %{buildroot}%{_datadir}/license
mkdir -p %{buildroot}%{_datadir}/packages
mkdir -p %{buildroot}/opt/data/context-service
install -m 0644 %{SOURCE1} %{buildroot}%{_unitdir_user}
cp LICENSE %{buildroot}%{_datadir}/license/%{name}
mkdir -p %{buildroot}%{_sysconfdir}/dbus-1/session.d
install -m 0644 %{SOURCE2} %{buildroot}%{_sysconfdir}/dbus-1/session.d/

%post
mkdir -p %{_unitdir_user}/default.target.wants
ln -s ../context-service.service %{_unitdir_user}/default.target.wants/
/sbin/ldconfig
#systemctl daemon-reload

%preun
#if [ $1 == 0 ]; then
#    systemctl stop context-service
#fi

%postun
rm -f %{_unitdir_user}/default.target.wants/context-service.service
#systemctl daemon-reload
/sbin/ldconfig

%files
%manifest packaging/%{name}.manifest
%config %{_sysconfdir}/dbus-1/session.d/*
%{_bindir}/*
%{_unitdir_user}/context-service.service
%{_datadir}/license/%{name}
