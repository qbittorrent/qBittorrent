#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Version: 2.01
# Changelog:
# - Use multiple threads to optimize speed

# Version: 2.0
# Changelog:
# - Fixed ThePirateBay search engine
# - Fixed Meganova search engine
# - Fixed Mininova search engine

# Version: 1.9
# Changelog:
# - Various fixes

# Version: 1.8
# Changelog:
# - Fixed links from isohunt

# Version: 1.7
# Changelog:
# - merged with qbittorrent branch (code cleanup, indentation mistakes)
# - separate standalone and slave mode
# - added btjunkie
# - added meganova
# - added multithreaded mode

# Version: 1.6
# Changelog:
# - size is now always returned in bytes
# - seeders/leechers are now always returned as integers
# - cleaned up code
# - results are now displayed in real time
# - fixed piratebay, torrentreactor

# Author:
#  Fabien Devaux <fab AT gnux DOT info>
# Contributors:
#  Christophe Dumez <chris@qbittorrent.org> (qbittorrent integration)
#  Thanks to gab #gcu @ irc.freenode.net (multipage support on PirateBay)
#  Thanks to Elias <gekko04@users.sourceforge.net> (torrentreactor and isohunt search engines)
#
# Licence: BSD

import sys
import urllib
import sgmllib
from xml.dom import minidom
import re
import os
import cgi
import traceback
import threading

STANDALONE = True
THREADED = True

if os.environ.has_key('QBITTORRENT'):
	STANDALONE = False

best_ratios = []

def prettyPrinter(dictionnary):
	print "%(link)s|%(name)s|%(size)s|%(seeds)s|%(leech)s|%(engine_url)s"%dictionnary

if STANDALONE:
	def termPrettyPrinter(dictionnary):
		if isinstance( dictionnary['size'], int):
			dictionnary['size'] = bytesToHuman(dictionnary['size'])
		try:
			print "%(seeds)5s/%(leech)5s | %(size)10s | %(name)s"%dictionnary
		except (UnicodeDecodeError, UnicodeEncodeError):
			print "%(seeds)5s/%(leech)5s | %(size)10s | <unprintable title>"%dictionnary
		try:
			print "wget '%s'"%dictionnary['link'].replace("'","\\'")
		except:
			pass
		dictionnary['seeds'] = int(	dictionnary['seeds'] ) or 0.00000001
		dictionnary['leech'] = int(	dictionnary['leech'] ) or 0.00000001
		best_ratios.append(dictionnary)

	globals()['prettyPrinter'] = termPrettyPrinter

def bytesToHuman(filesize):
	"""
	Convert float (size in bytes) to readable string
	"""
	decimators = ('k','M','G','T')
	unit = ''
	for n in range(len(decimators)):
		if filesize > 1100.0:
			filesize /= 1024.0
			unit = decimators[n]
	return '%.1f%sB'%(filesize, unit)

def anySizeToBytes(size_string):
	"""
	Convert a string like '1 KB' to '1024' (bytes)
	"""
	# separate integer from unit
	try:
		size, unit = size_string.split()
	except (ValueError, TypeError):
		try:
			size = size_string.strip()
			unit = ''.join([c for c in size if c.isalpha()])
			size = size[:-len(unit)]
		except(ValueError, TypeError):
			return -1

	size = float(size)
	short_unit = unit.upper()[0]

	# convert
	units_dict = { 'T': 40, 'G': 30, 'M': 20, 'K': 10 }
	if units_dict.has_key( short_unit ):
		size = size * 2**units_dict[short_unit]
	return int(size)

################################################################################
# Every engine should have a "search" method taking
# a space-free string as parameter (ex. "family+guy")
# it should call prettyPrinter() with a dict as parameter
# see above for dict keys
# As a convention, try to list results by decrasing number of seeds or similar
################################################################################

class PirateBay(object):
	url = 'http://thepiratebay.org'

	def __init__(self):
		self.results = []
		self.parser = self.SimpleSGMLParser(self.results, self.url)

	class SimpleSGMLParser(sgmllib.SGMLParser):
		def __init__(self, results, url, *args):
			sgmllib.SGMLParser.__init__(self)
			self.td_counter = None
			self.current_item = None
			self.results = results
			self.url = url
			self.code = 0

		def start_a(self, attr):
			params = dict(attr)
			if params['href'].startswith('/browse'):
				self.current_item = {}
				self.td_counter = 0
                        elif params['href'].startswith('/tor'):
				self.code = params['href'].split('/')[2]
			elif params['href'].startswith('http://torrents.thepiratebay.org/%s'%self.code):
				self.current_item['link']=params['href'].strip()
				self.td_counter = self.td_counter+1

		def handle_data(self, data):
			if self.td_counter == 1:
				if not self.current_item.has_key('name'):
					self.current_item['name'] = ''
				self.current_item['name']+= data.strip()
			if self.td_counter == 5:
				if not self.current_item.has_key('size'):
					self.current_item['size'] = ''
				self.current_item['size']+= data.strip()
			elif self.td_counter == 6:
				if not self.current_item.has_key('seeds'):
					self.current_item['seeds'] = ''
				self.current_item['seeds']+= data.strip()
			elif self.td_counter == 7:
				if not self.current_item.has_key('leech'):
					self.current_item['leech'] = ''
				self.current_item['leech']+= data.strip()

		def start_td(self,attr):
			if isinstance(self.td_counter,int):
				self.td_counter += 1
				if self.td_counter > 7:
					self.td_counter = None
					# Display item
					if self.current_item:
						self.current_item['engine_url'] = self.url
						self.current_item['size']= anySizeToBytes(self.current_item['size'])
						if not self.current_item['seeds'].isdigit():
							self.current_item['seeds'] = 0
						if not self.current_item['leech'].isdigit():
							self.current_item['leech'] = 0
						prettyPrinter(self.current_item)
						self.results.append('a')
	def search(self, what):
		ret = []
		i = 0
		order = 'se'
		while True:
			results = []
			parser = self.SimpleSGMLParser(results, self.url)
			dat = urllib.urlopen(self.url+'/search/%s/%u/0/0' % (what, i)).read()
			parser.feed(dat)
			parser.close()
			if len(results) <= 0:
				break
			i += 1

class Mininova(object):
	url = 'http://www.mininova.org'
	table_items = 'added cat name size seeds leech'.split()

	def search(self, what):
		order = 'seeds' # must be one in self.table_items

		def get_link(lnk):
                        lnks = lnk.getElementsByTagName('a')
                        if lnks.item(0).attributes.get('href').value.startswith('/faq'):
                          if len(lnks) > 1:
                            return self.url+lnks.item(1).attributes.get('href').value
                        else:
			 return self.url+lnks.item(0).attributes.get('href').value

		def get_text(txt):
			if txt.nodeType == txt.TEXT_NODE:
				return txt.toxml()
			else:
				return ''.join([ get_text(n) for n in txt.childNodes])
		dat = urllib.urlopen(self.url+'/search/%s/seeds'%(what,)).read().decode('utf-8', 'replace')
		dat = re.sub("<a href=\"http://www.boardreader.com/index.php.*\"", "<a href=\"plop\"", dat)
		x = minidom.parseString(dat.encode('utf-8', 'replace'))
		table = x.getElementsByTagName('table').item(0)
		if not table: return
		for tr in table.getElementsByTagName('tr'):
			tds = tr.getElementsByTagName('td')
			if tds:
				i = 0
				vals = {}
				for td in tds:
					if self.table_items[i] == 'name':
						vals['link'] = get_link(td).strip()
					vals[self.table_items[i]] = get_text(td).strip()
					i += 1
				vals['engine_url'] = self.url
				vals['size']= anySizeToBytes(vals['size'])
				if not vals['seeds'].isdigit():
					vals['seeds'] = 0
				if not vals['leech'].isdigit():
					vals['leech'] = 0
				prettyPrinter(vals)

# TODO: add multipage
class BtJunkie(object):
	url = 'http://btjunkie.org'

	def search(self, what):
		dat = urllib.urlopen(self.url+'/search?q=%s'%what).read().decode('utf8', 'replace')
		# I know it's not very readable, but the SGML parser feels in pain
		section_re = re.compile('(?s)href="/torrent\?do=download.*?<tr>')
		torrent_re = re.compile('(?s)href="(?P<link>.*?do=download[^"]+).*?'
		'class="BlckUnd">(?P<name>.*?)</a>.*?'
		'>(?P<size>\d+MB)</font>.*?'
		'>(?P<seeds>\d+)</font>.*?'
		'>(?P<leech>\d+)</font>')
		for match in section_re.finditer(dat):
			txt = match.group(0)
			m = torrent_re.search(txt)
			if m:
				torrent_infos = m.groupdict()
				torrent_infos['name'] = re.sub('</?font.*?>', '', torrent_infos['name'])
				torrent_infos['engine_url'] = self.url
				torrent_infos['size'] = anySizeToBytes(torrent_infos['size'])
				torrent_infos['link'] = self.url+torrent_infos['link']
				prettyPrinter(torrent_infos)

class MegaNova(object):
	url = 'http://www.meganova.org'

	def search(self, what):
		dat = urllib.urlopen(self.url+'/find-seeds/%s.html'%what).read().decode('utf8', 'replace')
		print 'url is ' + self.url+'/find-seeds/%s.html'%what
		# I know it's not very readable, but the SGML parser feels in pain

		section_re = re.compile('(?s)<td width="6%">.*?</tr')
		torrent_re = re.compile('(?s)href="(?P<link>/torrent/.*?)".*?'
		'<span.*?>(?P<name>.*?)</span>.*?'
		'>(?P<size>[0-9.]+\s+.B).*?'
		'>(?P<seeds>\d+)<.*?'
		'>(?P<leech>\d+)<')

		for match in section_re.finditer(dat):
			txt = match.group(0)
			m = torrent_re.search(txt)
			if m:
				torrent_infos = m.groupdict()
				torrent_infos['engine_url'] = self.url
				torrent_infos['size'] = anySizeToBytes(torrent_infos['size'])
				torrent_infos['link'] = self.url+torrent_infos['link']
				prettyPrinter(torrent_infos)

class Reactor(object):
	url = 'http://tr.searching.com'

	class SimpleSGMLParser(sgmllib.SGMLParser):
		def __init__(self, results, url, *args):
			sgmllib.SGMLParser.__init__(self)
			self.td_counter = None
			self.current_item = None
			self.results = results
			self.id = None
			self.url = url

		def start_a(self, attr):
			params = dict(attr)
			if params['href'].startswith('view.php'):
				self.current_item = {}
				self.td_counter = 0
				# extract the torrent id
				#I save it in a global variable for after create the link string
				equal = params['href'].find("=")
				self.id = str(int(params['href'][equal+1:]))

		def handle_data(self, data):
			if self.td_counter == 0:
				if not self.current_item.has_key('name'):
					self.current_item['name'] = ''
				self.current_item['name']+= data.strip()
			if self.td_counter == 1:
				if not self.current_item.has_key('size'):
					self.current_item['size'] = ''
				self.current_item['size']+= data.strip()
			elif self.td_counter == 2:
				if not self.current_item.has_key('seeds'):
					self.current_item['seeds'] = ''
				self.current_item['seeds']+= data.strip()
			elif self.td_counter == 3:
				if not self.current_item.has_key('leech'):
					self.current_item['leech'] = ''
				self.current_item['leech']+= data.strip()

		def start_td(self,attr):
			if isinstance(self.td_counter,int):
				self.td_counter += 1
				if self.td_counter > 7:
					self.td_counter = None
					# add item to results
					if self.current_item:
						self.current_item['link']='http://download.torrentreactor.net/download.php?name=%s&id=%s'%(cgi.escape(self.current_item['name']),self.id)
						self.current_item['engine_url'] = self.url
						self.current_item['size']= anySizeToBytes(self.current_item['size'])
						if not self.current_item['seeds'].isdigit():
							self.current_item['seeds'] = 0
						if not self.current_item['leech'].isdigit():
							self.current_item['leech'] = 0
						prettyPrinter(self.current_item)
						self.has_results = True
						self.results.append('a')

	def __init__(self):
		self.results = []
		self.parser = self.SimpleSGMLParser(self.results, self.url)

	def search(self, what):
		i = 0
		while True:
			results = []
			parser = self.SimpleSGMLParser(results, self.url)
			dat = urllib.urlopen(self.url+'/search.php?search=&words=%s&skip=%s'%(what,(i*50))).read().decode('utf-8', 'replace')
			parser.feed(dat)
			parser.close()
			if len(results) <= 0:
				break
			i += 1

class Isohunt(object):
	url = 'http://isohunt.com'

	class SimpleSGMLParser(sgmllib.SGMLParser):
		def __init__(self, results, url, *args):
			sgmllib.SGMLParser.__init__(self)
			self.td_counter = None
			self.current_item = None
			self.results = results
			self.url = url

		def start_tr(self, attr):
			params = dict(attr)
			if 'onclick' in params:
				Durl='http://isohunt.com/download'
				self.current_item = {}
				self.td_counter = 0
				try:
					self.current_item['link'] = '%s/%s'%(Durl, params['onclick'].split('/')[2])
				except IndexError:
					self.current_item['link'] = None

		def handle_data(self, data):
			if self.td_counter == 3:
				if not self.current_item.has_key('name'):
					self.current_item['name'] = ''
				self.current_item['name']+= data.strip()
			if self.td_counter == 4:
				if not self.current_item.has_key('size'):
					self.current_item['size'] = ''
				self.current_item['size']+= data.strip()
			if self.td_counter == 5:
				if not self.current_item.has_key('seeds'):
					self.current_item['seeds'] = ''
				self.current_item['seeds']+= data.strip()
			if self.td_counter == 6:
				if not self.current_item.has_key('leech'):
					self.current_item['leech'] = ''
				self.current_item['leech']+= data.strip()

		def start_td(self,attr):
			if isinstance(self.td_counter,int):
				self.td_counter += 1
				if self.td_counter > 7:
					self.td_counter = None
					# add item to results
					if self.current_item:
						self.current_item['engine_url'] = self.url
						self.current_item['size']= anySizeToBytes(self.current_item['size'])
						if not self.current_item.has_key('seeds') or not self.current_item['seeds'].isdigit():
							self.current_item['seeds'] = 0
						if not self.current_item.has_key('leech') or not self.current_item['leech'].isdigit():
							self.current_item['leech'] = 0
						if self.current_item['link'] is not None:
							prettyPrinter(self.current_item)
							self.results.append('a')

	def __init__(self):
		self.results = []
		self.parser = self.SimpleSGMLParser(self.results, self.url)

	def search(self, what):
		i = 1
		while True:
			results = []
			parser = self.SimpleSGMLParser(results, self.url)
			dat = urllib.urlopen(self.url+'/torrents.php?ihq=%s&ihp=%s'%(what,i)).read().decode('utf-8', 'replace')
			parser.feed(dat)
			parser.close()
			if len(results) <= 0:
				break
			i += 1

class EngineLauncher(threading.Thread):
	def __init__(self, engine, what):
		threading.Thread.__init__(self)
		self.engine = engine
		self.what = what
	def run(self):
		self.engine.search(self.what)

if __name__ == '__main__':
	available_engines_list = BtJunkie, MegaNova, Mininova, PirateBay, Reactor, Isohunt

	if len(sys.argv) < 2:
		raise SystemExit('./nova.py [all|engine1[,engine2]*] <keywords>\navailable engines: %s'%
				(','.join(e.__name__ for e in available_engines_list)))

	engines_list = [e.lower() for e in sys.argv[1].strip().split(',')]

	if 'all' in engines_list:
		engines_list = [e.__name__.lower() for e in available_engines_list]

	selected_engines = set(e for e in available_engines_list if e.__name__.lower() in engines_list)

	if not selected_engines:
		selected_engines = [BtJunkie]
		what = '+'.join(sys.argv[1:])
	else:
		what = '+'.join(sys.argv[2:])

	threads = []
	for engine in selected_engines:
		try:
			if THREADED:
				l = EngineLauncher( engine(), what )
				threads.append(l)
				l.start()
			else:
				engine().search(what)
		except:
			if STANDALONE:
				traceback.print_exc()
	if THREADED:
		for t in threads:
			t.join()

	best_ratios.sort(lambda a,b : cmp(a['seeds']-a['leech'], b['seeds']-b['leech']))

	max_results = 10

	print "########## TOP %d RATIOS ##########"%max_results

	for br in best_ratios:
		if br['seeds'] > 1: # avoid those with 0 leech to be max rated
			prettyPrinter(br)
			max_results -= 1
		if not max_results:
			break
