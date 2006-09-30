
%define name qbittorrent
%define major 0
%define minor 6
%define patch 1
%define version	%{major}.%{minor}.%{patch}
%define release %mkrel 2
%define _iconsdir %{_datadir}/icons
%define _mandir %_datadir/man

Name:           %{name}
Summary:        A Bittorrent Client using C++ / Qt4
Version:        %{version}
Release:        %{release}
Source0:        http://sourceforge.net/projects/qbittorrent/%{name}-%{version}.tar.gz
URL:            http://sourceforge.net/projects/qbittorrent
Vendor:			http://qbittorrent.sourceforge.net/
Group:          Internet/File Transfer
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-buildroot
License:        GPL
BuildRequires:	libqt4-devel >= 4.1.2, libqtgui4 >= 4.1.2, libqtcore4 >= 4.1.2, libqtxml4 >= 4.1.2, libqtnetwork4 >= 4.1.2, rb_libtorrent-devel >= 0.10-3, libcurl3-devel
Requires:		libqtgui4 >= 4.1.2, libqtcore4 >= 4.1.2, libqtxml4 >= 4.1.2, libqtnetwork4 >= 4.1.2, librb_libtorrent0 >= 0.10-3, python >= 2.3, libcurl3

%description
A Bittorrent client using C++ / libtorrent and a Qt4 Graphical User Interface.
It aims to be as fast as possible and to provide multi-OS, unicode support.

%prep
%setup

%build
# Export the Environment variables
export QTDIR=%_prefix/%_lib/qt4
export KDEDIR=%_prefix
export LD_LIBRARY_PATH=$QTDIR/%_lib:$KDEDIR/%_lib:$LD_LIBRARY_PATH
export PATH=$QTDIR/bin:$KDEDIR/bin:$PATH


# Change to the Source directory and configure
#cd src
CFLAGS="%optflags" CXXFLAGS="%optflags" \
./configure --prefix=%{buildroot}%{_prefix}

# Necessary to remove old compiled files.. if they exist
make clean

%make

%install
%makeinstall --directory=src


# Create the menu directory
install -d %{buildroot}%{_menudir}

# Build the Menu
#<package> <section> <file_in> <file_out> [requires] [title]
kdedesktop2mdkmenu.pl %{name} "%{group}" %{buildroot}%{_datadir}/applications/qBittorrent.desktop %{buildroot}%{_menudir}/%{name}

%clean
%{__rm} -rf %{buildroot}

%post
/sbin/ldconfig
%{update_menus}

%postun
/sbin/ldconfig
%{clean_menus}


%files
%defattr(-,root,root)
%doc README INSTALL NEWS COPYING AUTHORS TODO Changelog
%doc %{_mandir}/man1/*.bz2


# The binaries
%_bindir/*


# Icon files
# Hi and Lo colour icons various sizes
%_iconsdir/hicolor/128x128/apps/qbittorrent.png
%_iconsdir/hicolor/16x16/apps/qbittorrent.png
%_iconsdir/hicolor/192x192/apps/qbittorrent.png
%_iconsdir/hicolor/22x22/apps/qbittorrent.png
%_iconsdir/hicolor/24x24/apps/qbittorrent.png
%_iconsdir/hicolor/32x32/apps/qbittorrent.png
%_iconsdir/hicolor/36x36/apps/qbittorrent.png
%_iconsdir/hicolor/48x48/apps/qbittorrent.png
%_iconsdir/hicolor/64x64/apps/qbittorrent.png
%_iconsdir/hicolor/72x72/apps/qbittorrent.png
%_iconsdir/hicolor/96x96/apps/qbittorrent.png
## %_iconsdir/hicolor/scalable/apps/qbittorrent.svgz


# Desktop Link
%_datadir/applications/qBittorrent.desktop

# The qbittorrent Menu directory
%dir %{_menudir}
%{_menudir}/%{name}


%changelog

* Wed Aug 23 2006 - Christophe Dumez <chris@qbittorrent.org> - 0.6.0-0.1.2006mdk
- FEATURE: Rewritten the download list from scratch (more flexible)
- FEATURE: Rewritten the search results list from scratch (more flexible)
- FEATURE: Rewritten the torrent properties list from scratch (more flexible)
- FEATURE: Improved and cleaned up search engine code
- FEATURE: Search results are now displayed in real time (not sequentially)
- FEATURE: Added two command lines parameters (--version, --help)
- FEATURE: Added a popup menu for download list
- FEATURE: Double-click on an item now toggles the paused state of a download
- FEATURE: Improved code to be more portable (Windows & MacOS versions should arrive soon)
- FEATURE: Allow to toggle selected state of a file within a torrent using double-click
- FEATURE: Remember columns width in download and search results lists
- BUGFIX: Don't use pkg-config for libcurl anymore (easier to compile)
- BUGFIX: Fixed ETA calculation when downloading while connecting
- BUGFIX: Download progress is now displayed correctly during first seconds of execution (was 0% before)
- BUGFIX: Code cleanup & optimization
- BUGFIX: Fixed sorting in download list
- BUGFIX: Fixed sorting in search results list
- BUGFIX: Fixed Parameters passing between instances
- BUGFIX: Fixed missing icon for clear action in infoBar popup menu
- BUGFIX: Fixed truncated lines in search results
- BUGFIX: Don't refresh download list when user is in search tab (save CPU)
- BUGFIX: Don't update Progress/DL Speed/ETA for finished downloads (save CPU)
- BUGFIX: Save selected search engines only when they have changed (faster program exit)
- COSMETIC: Increased icon size in toolbar from 24px to 32px
- COSMETIC: Display a progress bar to visualize each download progress
- COSMETIC: Size of each result in search are displayed in user friendly units
- COSMETIC: Display a progress bar to visualize each file progress within a torrent
- COSMETIC: Renamed 'ratio' to 'Session ratio' (makes more sense)
- COSMETIC: Improved layout of torrent properties window when maximized
- COSMETIC: Now number of search results is updated in real time
- COSMETIC: Remember last window size
- COSMETIC: Improved splash screen look
- COSMETIC: Improved default width of columns in download and search results lists

* Tue Aug 08 2006 - Christophe Dumez <chris@qbittorrent.org> - 0.5.0-0.1.20060mdk
 - FEATURE: Improved "Download from url" feature (now supports https, ftp & redirections)
 - FEATURE: Added a torrent creation tool
 - FEATURE: Display progress for each file within a torrent
 - FEATURE: Based on new libtorrent v0.10 (lot of improvements)
 - FEATURE: Now possible to clear log textbox (popup menu)
 - FEATURE: Added two search engines (isohunt, torrentreactor)
 - FEATURE: Now Display share ratio on main window
 - FEATURE: Use OSD (On Screen Display) when a download or a search is finished
 - FEATURE: Allow only one instance of qBittorrent (and add new parameters to download list)
 - FEATURE: Remember last selected search engines in search tab
 - FEATURE: Improved search engines status output (Aborted, timed out, finished, no results)
 - FEATURE: qBittorrent can now update search plugin from qbittorrent.org
 - I18N: Added Slovak, Italian, Portuguese, Romanian and Traditional Chinese languages
 - BUGFIX: Fixed ThePirateBay parser for search engine (website had changed)
 - BUGFIX: Fixed filenames for results from ThePirateBay search engine
 - BUGFIX: Fixed unicode support for ThePirateBay search engine
 - BUGFIX: Now search results are sorted by seeds
 - BUGFIX: Overwrite nova.py search plugin only if it is outdated
 - BUGFIX: Fixed possible division by 0 in ETA calculation
 - BUGFIX: Improved ETA calculation precision
 - BUGFIX: Fixed default tab in options
 - BUGFIX: When saving options, reconnect only when listening ports changed
 - COSMETIC: qBittorrent has now its own new logo
 - COSMETIC: Display status "downloading" if DL Speed > 0 (even when tracker is down)
 - COSMETIC: Added a splashscreen
 - COSMETIC: qBittorrent has new cute icons
 - COSMETIC: Display number of results in search tab
 - COSMETIC: Added icons for each item in download list according to its state
 - COSMETIC: Redesigned Locale settings
 - COSMETIC: Fixed search engines names width (were cut on the right)
 - COSMETIC: Moved search engines to the left of the window (better ui)

* Fri Jun 23 2006 - Christophe Dumez <chris@qbittorrent.org> - 0.4.1-0.1.20060mdk
- Not counting "protocol chatter" in UP/DL speed anymore
- Download speed is now 0 when download is finished
- Paused torrents remain paused when qbittorrent is re-started
- Added option "go to systray when minimizing"
- Added option "Clear finished downloads on exit"
- Added option "Ask user for confirmation on exit"
- Added "Stalled" status for downloads (colored in orange, paused are in red and finished in green)
- Fixed Search window layout on maximizing
- Fixed a bug that caused upload limit not to be always applied
- Added Bulgarian translation
- Updated Translations
- Code optimization

* Tue Jun 13 2006 - Christophe Dumez <chris@qbittorrent.org> - 0.4.0-0.1.20060mdk
- Added a search engine (supports Mininova & thepiratebay websites)
- Fixed critical bug: some options were not applied correctly to BT session
- Possibility to download a torrent file from an URL
- Added confirmation dialog on qbittorrent exit
- Enabled sorting in Download list
- Added Ukrainian translation
- Support urls as program parameters
- Added more actions to trayicon menu
- Fixed exception catching when retrieving fastresume data
- use Binary prefix standards from IEC 60027-2 for units (B, KiB, MiB, GiB, TiB)
- Iconification to systray when minimizing
- Code Cleanup & optimization

* Tue Jun 06 2006 - Christophe Dumez <chris@qbittorrent.org> - 0.3.1-0.1.20060mdk
- Fixed toolbar layout (spacing)
- Added Russian translation
- Resume also finished files on startup (for seeding)
- Added colors corresponding to download state
- Fixed a segfault when deleting a download (if no scan dir is set)

* Mon Jun 05 2006 - Christophe Dumez <chris@qbittorrent.org> - 0.3-0.1.20060mdk
- Fixed auto-resume (worked only once)
- Fixed BT_Backup dir creation on first startup (thanks Peter)
- Now min port and max port are inverted if (min port > max port)
- Fixed memory leaks
- Added qbittorrent man page
- Allow to disable max connections limit (default is disabled)
- Disable upload limit by default
- Added Menu Entry with icon (thanks Peter)
- Restructured directory, now Makefile is in main directory (not src/)
- Updated README / INSTALL

* Fri Jun 02 2006 - Christophe Dumez <chris@qbittorrent.org> 0.2.3-0.1.20060mdk
- Fixed ports checking function (user couldn't type the value he wanted)
- Check tracker errors list size and clear it if it becomes too big.
- qBittorrent does not remove .torrent file from scanned directory anymore
- Small cosmetic change

* Wed May 31 2006 Christophe Dumez <chris@qbittorrent.org> 0.2.2-0.1.20060mdk
- Fixed missing icons

* Sat May 27 2006 Jeffery Fernandez <developer@jefferyfernandez.id.au> 0.2.1-0.1.20060mdk
- Initial Build for Mandriva Linux

* Thu May 25 2006 Christophe Dumez <chris@qbittorrent.org>
- Fixed "make install" rule
- Disabled debug mode 

* Thu May 25 2006 Christophe Dumez <chris@qbittorrent.org> - v0.2
- Fixed a compatibility problem with some versions of qmake
- Added translations : Greek, Swedish
- Fixed Polish translation selection
- Fixed come warning because of two unexisting slots
- Improved "Apply" button behaviour in options
- Windows are now resizable

* Tue May 16 2006 Christophe Dumez <chris@qbittorrent.org> - v0.1
- Initial release (lack features & still need a lot of improvements)

