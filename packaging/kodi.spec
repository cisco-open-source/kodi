Name: kodi
Version: 14.0b3
Release: 0.2.beta3
Summary: Media center

License: GPLv2+ and GPLv3+
Group: Applications/Multimedia
URL: http://www.xbmc.org/
Source0: %{name}-%{version}.tar.xz

# Gbp-Ignore-Patches: 0
Patch1: 0001-remove_java_deps_from_bootstrap.patch

# Kodi is the renamed XBMC project
Obsoletes: xbmc < 14.0-1
Obsoletes: xbmc-eventclients < 14.0-1

ExcludeArch: ppc64

BuildRequires: avahi-devel
BuildRequires: bluez-devel
BuildRequires: boost-devel
BuildRequires: bzip2-devel
BuildRequires: cmake
BuildRequires: dbus-devel
BuildRequires: desktop-file-utils
BuildRequires: e2fsprogs-devel
#BuildRequires: enca-devel
BuildRequires: expat-devel
BuildRequires: ffmpeg-devel-static
BuildRequires: flac-devel
BuildRequires: flex
BuildRequires: fontconfig-devel
#BuildRequires: fontpackages-devel
BuildRequires: freetype-devel
BuildRequires: fribidi-devel
BuildRequires: gettext-devel
BuildRequires: glib2-devel
BuildRequires: gperf
BuildRequires: libjasper-devel
#BuildRequires: java-devel
#BuildRequires: lame-devel
BuildRequires: libass-devel >= 0.9.7
BuildRequires: libcap-devel
BuildRequires: libcdio-devel
BuildRequires: libcurl-devel
BuildRequires: libjpeg-devel
BuildRequires: libmodplug-devel
BuildRequires: libmpeg2-devel
BuildRequires: libogg-devel
BuildRequires: libpng-devel
BuildRequires: libtiff-devel
BuildRequires: libtool
%ifnarch %{arm} mipsel
BuildRequires: libva-devel
BuildRequires: libvdpau-devel
%endif
BuildRequires: libvorbis-devel
BuildRequires: libxml2-devel
BuildRequires: libxslt-devel
BuildRequires: lzo-devel
BuildRequires: pkgconfig(wayland-client)
BuildRequires: pkgconfig(wayland-egl)
# ARM uses GLES
%ifarch %{arm} mipsel
BuildRequires: pkgconfig(egl)
BuildRequires: pkgconfig(glesv2)
%endif
BuildRequires: nasm
BuildRequires: pcre-devel
BuildRequires: libpulse-devel
BuildRequires: python-devel
BuildRequires: sqlite-devel
BuildRequires: swig
BuildRequires: systemd-devel
BuildRequires: taglib-devel >= 1.8
BuildRequires: tinyxml-devel
#BuildRequires: tre-devel
#BuildRequires: trousers-devel
BuildRequires: yajl-devel
BuildRequires: zlib-devel
BuildRequires: doxygen
BuildRequires: libxkbcommon-devel
BuildRequires: pixman-devel
BuildRequires: cairo-devel
BuildRequires: weston-devel
BuildRequires: libopenssl-devel
BuildRequires: makeinfo
BuildRequires: curl
BuildRequires: unzip
BuildRequires: zip
BuildRequires: gnutls
BuildRequires: libgnutls-devel

#BuildRequires: nfs-utils-devel

#Requires: google-roboto-fonts

# This is just symlinked to, but needed both at build-time
# and for installation
#Requires: python-imaging


%description
Kodi is a free cross-platform media-player jukebox and entertainment hub.
Kodi can play a spectrum of of multimedia formats, and featuring playlist, 
audio visualizations, slideshow, and weather forecast functions, together 
third-party plugins.


%package devel
Summary: Development files needed to compile C programs against kodi
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
Kodi is a free cross-platform media-player jukebox and entertainment hub.
If you want to develop programs which use Kodi's libraries, you need to 
install this package.


%package eventclients
Summary: Media center event client remotes

%description eventclients
This package contains support for using Kodi with the PS3 Remote, the Wii
Remote, a J2ME based remote and the command line xbmc-send utility.

%package eventclients-devel
Summary: Media center event client remotes development files
Requires:	%{name}-eventclients = %{version}-%{release}
Requires:	%{name}-devel = %{version}-%{release}

%description eventclients-devel
This package contains the development header files for the eventclients
library.


%prep
%setup -q -n %{name}-%{version}

# Gbp-Patch-Macros
%patch1 -p1

  # Remove hdhomerun from the build.
  pushd xbmc/filesystem/
    rm HDHomeRunFile.cpp HDHomeRunFile.h
    rm HDHomeRunDirectory.cpp HDHomeRunDirectory.h
    sed -i Makefile.in -e '/HDHomeRunFile\.cpp/d'
    sed -i Makefile.in -e '/HDHomeRunDirectory\.cpp/d'
    sed -i DirectoryFactory.cpp -e '/HomeRun/d'
    sed -i FileFactory.cpp -e '/HomeRun/d'
  popd

%build
chmod +x bootstrap
./bootstrap
# Can't use export nor %%configure (implies using export), because
# the Makefile pile up *FLAGS in this case.

./configure \
--prefix=%{_prefix} --bindir=%{_bindir} --includedir=%{_includedir} \
--libdir=%{_libdir} --datadir=%{_datadir} \
--with-lirc-device=/var/run/lirc/lircd \
--enable-goom \
--enable-pulse \
--disable-libcec \
--disable-ssh \
--disable-mysql \
--disable-webserver \
--disable-ssh \
--disable-samba \
--disable-texturepacker \
--disable-dvdcss \
--disable-optimizations --disable-debug \
%ifnarch %{arm} mipsel
--enable-gl \
--disable-gles \
--enable-vdpau \
%else
--enable-gles \
--disable-vdpau \
--disable-vaapi \
--disable-neon \
%endif
CFLAGS="$RPM_OPT_FLAGS -fPIC -I/usr/include/samba-4.0/ -D__STDC_CONSTANT_MACROS" \
CXXFLAGS="$RPM_OPT_FLAGS -fPIC -I/usr/include/samba-4.0/ -D__STDC_CONSTANT_MACROS" \
LDFLAGS="-fPIC" \
ASFLAGS=-fPIC

make %{?_smp_mflags} VERBOSE=1


%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install
make -C tools/EventClients DESTDIR=$RPM_BUILD_ROOT install 
# remove the doc files from unversioned /usr/share/doc/xbmc, they should be in versioned docdir
rm -r $RPM_BUILD_ROOT/%{_datadir}/doc/

desktop-file-install \
 --dir=${RPM_BUILD_ROOT}%{_datadir}/applications \
 $RPM_BUILD_ROOT%{_datadir}/applications/xbmc.desktop

# Normally we are expected to build these manually. But since we are using
# the system Python interpreter, we also want to use the system libraries
install -d $RPM_BUILD_ROOT%{_libdir}/xbmc/addons/script.module.pil/lib
ln -s %{python_sitearch}/PIL $RPM_BUILD_ROOT%{_libdir}/xbmc/addons/script.module.pil/lib/PIL
#install -d $RPM_BUILD_ROOT%{_libdir}/xbmc/addons/script.module.pysqlite/lib
#ln -s %{python_sitearch}/pysqlite2 $RPM_BUILD_ROOT%{_libdir}/xbmc/addons/script.module.pysqlite/lib/pysqlite2

# Use external Roboto font files instead of bundled ones
ln -sf %{_fontbasedir}/google-roboto/Roboto-Regular.ttf ${RPM_BUILD_ROOT}%{_datadir}/xbmc/addons/skin.confluence/fonts/
ln -sf %{_fontbasedir}/google-roboto/Roboto-Bold.ttf ${RPM_BUILD_ROOT}%{_datadir}/xbmc/addons/skin.confluence/fonts/


%post
/bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :


%postun
if [ $1 -eq 0 ] ; then
    /bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null
    /usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
fi


%posttrans
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :


%files
%defattr(-,root,root)
%doc copying.txt CONTRIBUTORS LICENSE.GPL README
%doc docs
%{_bindir}/xbmc
%{_bindir}/xbmc-standalone
%{_libdir}/xbmc
%{_datadir}/xbmc
%{_datadir}/xsessions/XBMC.desktop
%{_datadir}/applications/xbmc.desktop
%{_datadir}/icons/hicolor/*/*/*.png


%files devel
%{_includedir}/xbmc


%files eventclients
%python_sitelib/xbmc
%dir %{_datadir}/pixmaps/xbmc
%{_datadir}/pixmaps/xbmc/*.png
%{_bindir}/xbmc-j2meremote
%{_bindir}/xbmc-ps3d
%{_bindir}/xbmc-ps3remote
%{_bindir}/xbmc-send
%{_bindir}/xbmc-wiiremote


%files eventclients-devel
%{_includedir}/xbmc/xbmcclient.h

