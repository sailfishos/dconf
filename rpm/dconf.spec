Name:       dconf
Summary:    simple configuration storage system
Version:    0.18.0
Release:    1
Group:      System Environment/Base
License:    LGPLv2.1+
URL:        https://download.gnome.org/sources/dconf/
Source0:    https://download.gnome.org/sources/dconf/0.18/%{name}-%{version}.tar.xz
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(glib-2.0) >= 2.36.0
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  intltool

%description
DConf is a low-level key/value database designed for storing desktop
environment settings.

%package devel
Summary:    Development files for %{name}
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
Development files for %{name}.

%prep
%setup -q -n %{name}-%{version}/%{name}

%build
sed -i -e '/gtkdocize/d' autogen.sh
sed -i -e 's/GTK_DOC_CHECK.*//' configure.ac
echo "EXTRA_DIST = missing-gtk-doc" > docs/gtk-doc.make
export NOCONFIGURE=1
%autogen
%configure \
    --enable-man=no \
    --enable-gtk-doc=no \
    --disable-editor \
    --disable-nls

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}

%make_install

%post
/sbin/ldconfig
/usr/bin/gio-querymodules /usr/lib/gio/modules/

%postun
/sbin/ldconfig
/usr/bin/gio-querymodules /usr/lib/gio/modules/

%files
%defattr(-,root,root,-)
%{_bindir}/dconf
%{_libdir}/gio/modules/libdconfsettings.so
%{_libdir}/libdconf-dbus-*.so.*
%{_libdir}/libdconf.so.*
%{_libexecdir}/dconf-service
%{_datadir}/dbus-1/services/*

%files devel
%defattr(-,root,root,-)
%{_includedir}/dconf/*.h
%{_includedir}/dconf/client/*.h
%{_includedir}/dconf/common/*.h
%{_includedir}/dconf-dbus-1/*.h
%{_libdir}/libdconf-dbus-*.so
%{_libdir}/libdconf.so
%{_libdir}/pkgconfig/*.pc
%{_datadir}/vala/vapi/dconf.*
%{_datadir}/bash-completion/completions/dconf
