%define ver      0.2.6
%define rel      1
%define prefix   /usr

Summary: Voodoo Tracker
Name: VoodooTracker
Version: %ver
Release: %rel
Copyright: GPL
Group: Applications/Sound
Source: http://www.syncomm.org/voodoo/VoodooTracker-%{ver}.tar.gz
BuildRoot: /var/tmp/voodoo-%{PACKAGE_VERSION}-root
URL: http://www.syncomm.org/voodoo
Docdir: %{prefix}/doc
Requires: gnome-libs >= 1.0.0

%description
Voodoo Tracker is a project that aims to harness and extend the
power of conventional trackers. Imagine self contained digital studio;
complete and ready for your modern music needs. Additionally
Voodoo will provide an interface that is designed for live
performances. No other tracker will let you cross fade, sequence, and
mix your favorite mods in realtime. Voodoo Tracker will be the 
turntables of the digital generation. 

%prep
%setup

%build
libtoolize --copy --force
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%prefix
make

%install
rm -rf $RPM_BUILD_ROOT

make prefix=$RPM_BUILD_ROOT%{prefix} install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root)

%doc AUTHORS COPYING ChangeLog NEWS README
%{prefix}/bin/*