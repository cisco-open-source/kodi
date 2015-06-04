%define _fontsdir               %{_datadir}/fonts
%define _ttffontsdir            %{_fontsdir}/truetype

Name: kodi
Version: 15.01a
Release: 0.1.alpha1
Summary: Media center

License: GPL-2.0+ and GPL-3.0+
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
BuildRequires: libnettle-devel
BuildRequires: gmp-devel
BuildRequires: libhogweed
BuildRequires: libmicrohttpd-devel

#BuildRequires: nfs-utils-devel

Requires: google-roboto-fonts
Requires: python-xml

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
--enable-webserver \
--disable-ssh \
--disable-samba \
--disable-texturepacker \
--disable-dvdcss \
--disable-optimizations --disable-debug \
--disable-x11 \
--enable-wayland \
--enable-gles \
--disable-vdpau \
--disable-vaapi \
--disable-neon \

CFLAGS="$RPM_OPT_FLAGS -fPIC -I/usr/include/samba-4.0/ -D__STDC_CONSTANT_MACROS" \
CXXFLAGS="$RPM_OPT_FLAGS -fPIC -I/usr/include/samba-4.0/ -D__STDC_CONSTANT_MACROS" \
LDFLAGS="-fPIC" \
ASFLAGS=-fPIC

make %{?_smp_mflags} VERBOSE=1


%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install
#make -C tools/EventClients DESTDIR=$RPM_BUILD_ROOT install 
# remove the doc files from unversioned /usr/share/doc/xbmc, they should be in versioned docdir
rm -r $RPM_BUILD_ROOT/%{_datadir}/doc/

desktop-file-install \
 --dir=${RPM_BUILD_ROOT}%{_datadir}/applications \
 $RPM_BUILD_ROOT%{_datadir}/applications/kodi.desktop
mkdir -p $RPM_BUILD_ROOT%{_datadir}/applications/tizen
mv $RPM_BUILD_ROOT%{_datadir}/applications/kodi.desktop $RPM_BUILD_ROOT%{_datadir}/applications/tizen/kodi.desktop

# Normally we are expected to build these manually. But since we are using
# the system Python interpreter, we also want to use the system libraries
install -d $RPM_BUILD_ROOT%{_libdir}/xbmc/addons/script.module.pil/lib
ln -s %{python_sitearch}/PIL $RPM_BUILD_ROOT%{_libdir}/xbmc/addons/script.module.pil/lib/PIL
#install -d $RPM_BUILD_ROOT%{_libdir}/xbmc/addons/script.module.pysqlite/lib
#ln -s %{python_sitearch}/pysqlite2 $RPM_BUILD_ROOT%{_libdir}/xbmc/addons/script.module.pysqlite/lib/pysqlite2

# Use external Roboto font files instead of bundled ones
ln -sf %{_ttffontsdir}/Roboto-Regular.ttf ${RPM_BUILD_ROOT}%{_datadir}/xbmc/addons/skin.confluence/fonts/
ln -sf %{_ttffontsdir}/Roboto-Bold.ttf ${RPM_BUILD_ROOT}%{_datadir}/xbmc/addons/skin.confluence/fonts/


%post
/bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :


%postun
if [ $1 -eq 0 ] ; then
    /bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null
    /usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
fi


%files
%defattr(-,root,root)
%doc copying.txt CONTRIBUTORS LICENSE.GPL
#%doc docs
%{_bindir}/xbmc
%{_bindir}/xbmc-standalone
%{_bindir}/kodi
%{_bindir}/kodi-standalone
%{_libdir}/xbmc
%{_libdir}/kodi
%{_datadir}/xbmc
%{_datadir}/kodi
%{_datadir}/xsessions/xbmc.desktop
%{_datadir}/xsessions/kodi.desktop
%{_datadir}/applications/tizen/kodi.desktop
%{_datadir}/icons/hicolor/*/*/*.png


%files devel
%{_includedir}/xbmc
%{_includedir}/kodi

