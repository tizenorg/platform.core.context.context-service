Name:       context-service
Summary:    Context-Service
Version:    0.7.0
Release:    1
Group:      Service/Context
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1:	context-service.service
Source2:	org.tizen.context.conf

BuildRequires: cmake
BuildRequires: sed
BuildRequires: pkgconfig(libtzplatform-config)
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(libxml-2.0)
BuildRequires: pkgconfig(sqlite3)
BuildRequires: pkgconfig(capi-system-info)
BuildRequires: pkgconfig(capi-appfw-app-manager)
BuildRequires: pkgconfig(appsvc)
BuildRequires: pkgconfig(alarm-service)
BuildRequires: pkgconfig(notification)
BuildRequires: pkgconfig(capi-system-system-settings)

BuildRequires: pkgconfig(cynara-creds-gdbus)
BuildRequires: pkgconfig(cynara-client)
BuildRequires: pkgconfig(cynara-session)

BuildRequires: pkgconfig(context-common)
BuildRequires: pkgconfig(context)
BuildRequires: pkgconfig(context-provider)

Requires(preun): /usr/bin/systemctl
Requires(post): /usr/bin/systemctl
Requires(postun): /usr/bin/systemctl

%description
Context-Service

%prep
%setup -q

%build
MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`

export   CFLAGS+=" -Wextra -Wcast-align -Wcast-qual -Wshadow -Wwrite-strings -Wswitch-default"
export CXXFLAGS+=" -Wextra -Wcast-align -Wcast-qual -Wshadow -Wwrite-strings -Wswitch-default -Wnon-virtual-dtor"

export   CFLAGS+=" -Wno-unused-parameter -Wno-empty-body"
export CXXFLAGS+=" -Wno-unused-parameter -Wno-empty-body"

export   CFLAGS+=" -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-strict-aliasing -fno-unroll-loops -fsigned-char -fstrict-overflow -fno-common"
export CXXFLAGS+=" -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-strict-aliasing -fno-unroll-loops -fsigned-char -fstrict-overflow"
export CXXFLAGS+=" -std=c++11 -Wno-c++11-compat"

export   CFLAGS+=" -DTIZEN_ENGINEER_MODE"
export CXXFLAGS+=" -DTIZEN_ENGINEER_MODE"
export   FFLAGS+=" -DTIZEN_ENGINEER_MODE"

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
sed -i "s/^\tversion=\".*\"/\tversion=\"%{version}\"/g" packaging/context-service.xml
cp packaging/context-service.xml %{buildroot}%{_datadir}/packages/

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
%{_datadir}/packages/*.xml
