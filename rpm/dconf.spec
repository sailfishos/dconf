Name:       dconf
Summary:    A configuration system
Version:    0.40.0
Release:    1
License:    LGPLv2+
URL:        https://wiki.gnome.org/Projects/dconf
Source0:    %{name}-%{version}.tar.bz2
Source1:    user
Source2:    dconf-update
Patch1:     0001-service-Allow-D-Bus-activation-only-through-systemd.patch
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
Requires:       oneshot
Requires:       glib2 >= 2.44.0
BuildRequires:  pkgconfig(glib-2.0) >= 2.44.0
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  intltool
BuildRequires:  oneshot
BuildRequires:  meson
BuildRequires:  systemd

%description
dconf is a low-level configuration system. Its main purpose is to provide a
backend to the GSettings API in GLib.

%package devel
Summary:    Development files for %{name}
Requires:   %{name} = %{version}-%{release}

%description devel
Development files for %{name}.

%prep
%autosetup -p1 -n %{name}-%{version}/%{name}

%build
%meson -Dbash_completion=false \
       -Dman=false \
       -Dgtk_doc=false \
       -Dvapi=false

%meson_build

%install
%meson_install

mkdir -p %{buildroot}/%{_sysconfdir}/dconf/profile/
cp %SOURCE1 %{buildroot}/%{_sysconfdir}/dconf/profile/
mkdir -p %{buildroot}/%{_sysconfdir}/dconf/db/nemo.d/
mkdir -p %{buildroot}/%{_sysconfdir}/dconf/db/vendor.d/
mkdir -p %{buildroot}/%{_sysconfdir}/dconf/db/vendor-variant.d/
mkdir -p %{buildroot}/%{_oneshotdir}
cp -a %SOURCE2 %{buildroot}/%{_oneshotdir}

# Needed for ghosting
touch %{buildroot}/%{_sysconfdir}/dconf/db/nemo
touch %{buildroot}/%{_sysconfdir}/dconf/db/vendor
touch %{buildroot}/%{_sysconfdir}/dconf/db/vendor-variant

%post
/sbin/ldconfig || :
%{_bindir}/add-oneshot dconf-update || :

%postun
/sbin/ldconfig || :

%files
%defattr(-,root,root,-)
%license COPYING
%{_bindir}/dconf
%{_libdir}/gio/modules/libdconfsettings.so
%{_libdir}/libdconf.so.*
%{_libexecdir}/dconf-service
%{_datadir}/dbus-1/services/*
%{_userunitdir}/dconf.service
%dir %{_sysconfdir}/dconf
%dir %{_sysconfdir}/dconf/db
%{_sysconfdir}/dconf/profile
%{_sysconfdir}/dconf/db/nemo.d
%ghost %{_sysconfdir}/dconf/db/nemo
%{_sysconfdir}/dconf/db/vendor.d
%ghost %{_sysconfdir}/dconf/db/vendor
%{_sysconfdir}/dconf/db/vendor-variant.d
%ghost %{_sysconfdir}/dconf/db/vendor-variant
%attr(755, root, root) %{_oneshotdir}/dconf-update

%files devel
%defattr(-,root,root,-)
%{_includedir}/dconf
%{_libdir}/libdconf.so
%{_libdir}/pkgconfig/dconf.pc
