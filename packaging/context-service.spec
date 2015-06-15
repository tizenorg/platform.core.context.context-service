Name:       context-service
Summary:    Context-Service
Version:    0.4.4
Release:    1
Group:      System/Service
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1:	context-service.service
Source2:	org.tizen.context.service

BuildRequires: cmake
BuildRequires: sed
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(libxml-2.0)
BuildRequires: pkgconfig(sqlite3)
BuildRequires: pkgconfig(capi-system-info)
BuildRequires: pkgconfig(capi-appfw-app-manager)
BuildRequires: pkgconfig(appsvc)
BuildRequires: pkgconfig(pkgmgr-info)
BuildRequires: pkgconfig(capi-security-privilege-manager)
BuildRequires: pkgconfig(alarm-service)
BuildRequires: pkgconfig(notification)

BuildRequires: pkgconfig(clips)
BuildRequires: pkgconfig(context-common)
BuildRequires: pkgconfig(context)
BuildRequires: context-internal

BuildRequires: pkgconfig(device-context-provider)
BuildRequires: pkgconfig(statistics-context-provider)
BuildRequires: pkgconfig(place-context-provider)

Requires(preun): /usr/bin/systemctl
Requires(post): sys-assert
Requires(post): /usr/bin/systemctl
Requires(post): /usr/bin/sqlite3
Requires(postun): /usr/bin/systemctl

%ifarch %{arm}
%define ARCH arm
%else
%define ARCH i586
%endif

%description
Context-Service

%prep
%setup -q

%build
MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`

export   CFLAGS+=" -Wextra -Wcast-align -Wcast-qual -Wshadow -Wwrite-strings -Wswitch-default"
export CXXFLAGS+=" -Wextra -Wcast-align -Wcast-qual -Wshadow -Wwrite-strings -Wswitch-default -Wnon-virtual-dtor -Wno-c++0x-compat"

export   CFLAGS+=" -Wno-unused-parameter -Wno-empty-body"
export CXXFLAGS+=" -Wno-unused-parameter -Wno-empty-body"

export   CFLAGS+=" -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-strict-aliasing -fno-unroll-loops -fsigned-char -fstrict-overflow -fno-common"
export CXXFLAGS+=" -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-strict-aliasing -fno-unroll-loops -fsigned-char -fstrict-overflow"
#export CXXFLAGS+=" -std=c++0x"

export   CFLAGS+=" -DTIZEN_ENGINEER_MODE"
export CXXFLAGS+=" -DTIZEN_ENGINEER_MODE"
export   FFLAGS+=" -DTIZEN_ENGINEER_MODE"

cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DARCH=%{ARCH} -DMAJORVER=${MAJORVER} -DFULLVER=%{version} -DPROFILE=%{?tizen_profile_name}
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}%{_unitdir}
#mkdir -p %{buildroot}%{_datadir}/dbus-1/services
mkdir -p %{buildroot}%{_datadir}/license
mkdir -p %{buildroot}%{_datadir}/packages
mkdir -p %{buildroot}/opt/dbspace
mkdir -p %{buildroot}/opt/data/context-service
sqlite3 %{buildroot}/opt/dbspace/.context-service.db "PRAGMA journal_mode = PERSIST;"
sqlite3 %{buildroot}/opt/dbspace/.context-service.db "CREATE TABLE VERSION (VERSION TEXT);"
sqlite3 %{buildroot}/opt/dbspace/.context-service.db "INSERT INTO VERSION VALUES ('%{version}');"
install -m 0644 %{SOURCE1} %{buildroot}%{_unitdir}
#install -m 0644 %{SOURCE2} %{buildroot}%{_datadir}/dbus-1/services/
cp LICENSE %{buildroot}%{_datadir}/license/%{name}
sed -i "s/^\tversion=\".*\"/\tversion=\"%{version}\"/g" packaging/context-service.xml
cp packaging/context-service.xml %{buildroot}%{_datadir}/packages/
cp data/access-config.xml %{buildroot}/opt/data/context-service/
cp data/trigger-template.json %{buildroot}/opt/data/context-service/
sh data/template-json-to-sql.sh data/trigger-template.json > %{buildroot}/opt/data/context-service/trigger-template.sql

%post
sqlite3 -echo /opt/dbspace/.context-service.db < /opt/data/context-service/trigger-template.sql
mkdir -p %{_unitdir}/graphical.target.wants
ln -s ../context-service.service %{_unitdir}/graphical.target.wants/
/sbin/ldconfig
systemctl daemon-reload
#if [ $1 == 1 ]; then
#    systemctl restart context-service
#fi

%preun
if [ $1 == 0 ]; then
    systemctl stop context-service
fi

%postun
rm -f %{_unitdir}/graphical.target.wants/context-service.service
systemctl daemon-reload
/sbin/ldconfig

%files
%manifest packaging/%{name}.manifest
%defattr(-,root,root,-)
%{_bindir}/*
%{_unitdir}/context-service.service
#%{_datadir}/dbus-1/services/org.tizen.context.service
%{_datadir}/license/%{name}
%{_datadir}/packages/*.xml
%defattr(0600,system,system,-)
/opt/data/context-service/*
%config(noreplace) /opt/dbspace/.context-service.db*
