Name:       context-service
Summary:    Context-Service
Version:    0.8.1
Release:    1
Group:      Service/Context
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1:	context-service.service

%define BUILD_PROFILE %{?profile}%{!?profile:%{?tizen_profile_name}}

%define SYSTEM_SERVICE	0
%define LEGACY_SECURITY	0

%if "%{?BUILD_PROFILE}" == "tv"
ExcludeArch: %{arm} aarch64 %ix86 x86_64
%endif

%if %{SYSTEM_SERVICE}
%define SERVICE_UNIT_DIR %{_unitdir}
%else
%define SERVICE_UNIT_DIR %{_unitdir_user}
%endif

BuildRequires: cmake
BuildRequires: pkgconfig(gmodule-2.0)
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(capi-system-info)
BuildRequires: pkgconfig(capi-system-device)
BuildRequires: pkgconfig(capi-appfw-app-manager)
BuildRequires: pkgconfig(capi-appfw-package-manager)
BuildRequires: pkgconfig(appsvc)
BuildRequires: pkgconfig(notification)

%if %{LEGACY_SECURITY}
%else
BuildRequires: pkgconfig(cynara-creds-gdbus)
BuildRequires: pkgconfig(cynara-client)
BuildRequires: pkgconfig(cynara-session)
%endif

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
export CXXFLAGS+=" -std=c++0x"

#export   CFLAGS+=" -Wcast-qual"
#export CXXFLAGS+=" -Wcast-qual"

#export   CFLAGS+=" -DTIZEN_ENGINEER_MODE"
#export CXXFLAGS+=" -DTIZEN_ENGINEER_MODE"
#export   FFLAGS+=" -DTIZEN_ENGINEER_MODE"

cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DMAJORVER=${MAJORVER} -DFULLVER=%{version} \
							   -DSYSTEM_SERVICE=${SYSTEM_SERVICE} \
							   -DLEGACY_SECURITY=${LEGACY_SECURITY}
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}%{SERVICE_UNIT_DIR}
mkdir -p %{buildroot}%{_datadir}/license
mkdir -p %{buildroot}%{_datadir}/packages
mkdir -p %{buildroot}/opt/data/context-service
install -m 0644 %{SOURCE1} %{buildroot}%{SERVICE_UNIT_DIR}
cp LICENSE %{buildroot}%{_datadir}/license/%{name}
%if ! %{LEGACY_SECURITY}
mkdir -p %{buildroot}%{_sysconfdir}/dbus-1/session.d
install -m 0644 packaging/org.tizen.context.conf %{buildroot}%{_sysconfdir}/dbus-1/session.d/
%endif

%post
mkdir -p %{SERVICE_UNIT_DIR}/default.target.wants
ln -s ../context-service.service %{SERVICE_UNIT_DIR}/default.target.wants/
/sbin/ldconfig
#systemctl daemon-reload

%preun
#if [ $1 == 0 ]; then
#    systemctl stop context-service
#fi

%postun
rm -f %{SERVICE_UNIT_DIR}/default.target.wants/context-service.service
#systemctl daemon-reload
/sbin/ldconfig

%files
%manifest packaging/%{name}.manifest
%if ! %{LEGACY_SECURITY}
%config %{_sysconfdir}/dbus-1/session.d/*
%endif
%{_bindir}/*
%{SERVICE_UNIT_DIR}/context-service.service
%{_datadir}/license/%{name}
