
%define package_name	rb_libtorrent
%define orig_name libtorrent
%define major 0
%define minor .10
%define patch .1
%define version	%{major}%{minor}
%define candidate -rc1
%define lib_name %mklibname %{package_name} %{major}
%define release	%mkrel 4

Summary     	: libtorrent is a C++ bittorrent library.
Name			: %{package_name}
Version			: %{version}
Release			: %{release}
License			: GPL
Group       	: Development/C++
Source0			: http://www.rasterbar.com/products/libtorrent/libtorrent-%{version}.tar.gz
URL				: http://www.rasterbar.com
BuildRequires	: boost >= 1.33.1
BuildRoot		:  %{_tmppath}/%{orig_name}-%{version}-%{release}-root
Patch0			: file_progress_arvid.patch.bz2

%description
 libtorrent is a C++ library that aims to be a good alternative 
to all the other bittorrent implementations around.


%package -n %{package_name}-devel
Group			: Development/C++
Summary			: Development files for %{package_name}
Conflicts		: libtorrent7-devel

%description -n %{package_name}-devel
 Development files for %{package_name}

%package -n %{lib_name}
Group			: Development/C++
Summary			: Library files for %{package_name}
Conflicts		: libtorrent7

%description -n %{lib_name}
 Library files for %{package_name}

%prep
%setup -n %{orig_name}-%{version}

%build
%configure --prefix=%{_prefix}
%make


%install
%makeinstall

# Create directories for the package 
install -d %{buildroot}%{_includedir}/%{orig_name}
install -d %{buildroot}%{_libdir}/pkgconfig

%clean
rm -rf %{buildroot}

# The binaries
%files  %(orig_name)
%defattr(0644, root, root, 0755)
%{_bindir}/*

# Documentation
%defattr(-, root, root)
%doc README AUTHORS INSTALL COPYING ChangeLog NEWS
%doc docs/*


# Devel Package
%files -n %{package_name}-devel
%defattr(-,root,root,-)
%dir %{_includedir}/%{orig_name}/
%dir %{_includedir}/%{orig_name}/asio/
%dir %{_includedir}/%{orig_name}/asio/detail/
%dir %{_includedir}/%{orig_name}/asio/impl/
%dir %{_includedir}/%{orig_name}/asio/ip/
%dir %{_includedir}/%{orig_name}/asio/ip/detail/
%dir %{_includedir}/%{orig_name}/asio/ssl/
%dir %{_includedir}/%{orig_name}/asio/ssl/detail/
%{_includedir}/%{orig_name}/*.hpp
%{_includedir}/%{orig_name}/asio/*.hpp
%{_includedir}/%{orig_name}/asio/detail/*.hpp
%{_includedir}/%{orig_name}/asio/impl/*.ipp
%{_includedir}/%{orig_name}/asio/ip/*.hpp
%{_includedir}/%{orig_name}/asio/ip/detail/*.hpp
%{_includedir}/%{orig_name}/asio/ssl/*.hpp
%{_includedir}/%{orig_name}/asio/ssl/detail/*.hpp





%{_libdir}/%{orig_name}.a
%{_libdir}/%{orig_name}.la
%{_libdir}/%{orig_name}.so
%{_libdir}/pkgconfig/libtorrent.pc


# Library Package
%files -n %{lib_name}
%defattr(-,root,root,-)
%_libdir/%{orig_name}.so.*


%changelog

* Wed Aug 23 2006 Christophe Dumez <chris@qbittorrent.org> 10.0.1-2006mdk
- Added patch for Torrent Properties crash fix

* Sat Jul 1 2006 %{packager} %{version}-%{release}
- fixed a bug where the requested number of peers in a tracker request could
  be too big.
- fixed a bug where empty files were not created in full allocation mode.
- fixed a bug in storage that would, in rare cases, fail to do a
  complete check.
- exposed more settings for tweaking parameters in the piece-picker,
  downloader and uploader (http_settings replaced by session_settings).
- tweaked default settings to improve high bandwidth transfers.
- improved the piece picker performance and made it possible to download
  popular pieces in sequence to improve disk performance.
- added the possibility to control upload and download limits per peer.
- fixed problem with re-requesting skipped pieces when peer was sending pieces
  out of fifo-order.
- added support for http seeding (the GetRight protocol)
- renamed identifiers called 'id' in the public interface to support linking
  with Objective.C++
- changed the extensions protocol to use the new one, which is also
  implemented by uTorrent.
- factorized the peer_connection and added web_peer_connection which is
  able to download from http-sources.
- converted the network code to use asio (resulted in slight api changes
  dealing with network addresses).
- made libtorrent build in vc7 (patches from Allen Zhao)
- fixed bug caused when binding outgoing connections to a non-local interface.
- add_torrent() will now throw if called while the session object is
  being closed.
- added the ability to limit the number of simultaneous half-open
  TCP connections. Flags in peer_info has been added.


* Thu Jun 1 2006 %{packager} %{version}-%{release}
- Initial Build for Mandriva Linux 
