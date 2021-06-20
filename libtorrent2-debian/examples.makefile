#!/usr/bin/make -f 
#
# Simple makefile for libtorrent-rasterbar's examples.
# Copyright © 2009 Cristian Greco <cristian.debian@gmail.com>
# This file is released in the public domain.
#
# Please note that you need to install the following packages in order to build
# these example programs:
#  - libtorrent-rasterbar-dev
#  - libboost-program-options*-dev
#  - libboost-regex*-dev
# (where the `*' means the same version of boost development packages which
# libtorrent-rasterbar-dev actually depends on).

CXX = g++

CXXFLAGS = -ftemplate-depth-50 -DBOOST_MULTI_INDEX_DISABLE_SERIALIZATION

TORRENT_CFLAGS = $(shell pkg-config libtorrent-rasterbar --cflags)
TORRENT_LIBS = $(shell pkg-config libtorrent-rasterbar --libs)

BOOST_PROGRAM_OPTIONS_LIBS = -lboost_program_options-mt
BOOST_REGEX_LIBS = -lboost_regex-mt

examples_BIN = client_test dump_torrent make_torrent simple_client enum_if

all: $(examples_BIN)

client_test: client_test.cpp
	@rm -f client_test
	$(CXX) $(CXXFLAGS) $(TORRENT_CFLAGS) -o $@ $<  $(TORRENT_LIBS) $(BOOST_PROGRAM_OPTIONS_LIBS) $(BOOST_REGEX_LIBS)

dump_torrent: dump_torrent.cpp
	@rm -f dump_torrent
	$(CXX) $(CXXFLAGS) $(TORRENT_CFLAGS) -o $@ $< $(TORRENT_LIBS)

make_torrent: make_torrent.cpp
	@rm -f make_torrent
	$(CXX) $(CXXFLAGS) $(TORRENT_CFLAGS) -o $@ $< $(TORRENT_LIBS)

simple_client: simple_client.cpp
	@rm -f simple_client
	$(CXX) $(CXXFLAGS) $(TORRENT_CFLAGS) -o $@ $< $(TORRENT_LIBS)

enum_if: enum_if.cpp
	@rm -f enum_if
	$(CXX) $(CXXFLAGS) $(TORRENT_CFLAGS) -o $@ $< $(TORRENT_LIBS)

clean:
	@rm -f $(examples_BIN)
