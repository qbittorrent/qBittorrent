#!/usr/bin/env python
# -*- coding: utf-8 -*-

# BSD License
# Copyright (c) 2006, Fabien Devaux <fab AT gnux DOT info>
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
#     * Neither the name of the <ORGANIZATION> nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH 
# DAMAGE.

#VERSION: 1.00

# Author:
#  Fabien Devaux <fab AT gnux DOT info>
# Contributors:
#  Christophe Dumez <chris@qbittorrent.org> (qbittorrent integration)
#  Thanks to gab #gcu @ irc.freenode.net (multipage support on PirateBay)
#  Thanks to Elias <gekko04@users.sourceforge.net> (torrentreactor and isohunt search engines)
#
# Licence: BSD

import sys
import threading
import os
import glob

THREADED = True

################################################################################
# Every engine should have a "search" method taking
# a space-free string as parameter (ex. "family+guy")
# it should call prettyPrinter() with a dict as parameter.
# The keys in the dict must be: link,name,size,seeds,leech,engine_url
# As a convention, try to list results by decrasing number of seeds or similar
################################################################################

supported_engines = []

engines = glob.glob(os.path.join(os.path.dirname(__file__), 'engines','*.py'))
for engine in engines:
	e = engine.split(os.sep)[-1][:-3]
	if len(e.strip()) == 0: continue
	if e.startswith('_'): continue
	try:
		exec "from engines.%s import %s"%(e,e)
		supported_engines.append(e)
	except:
		pass

class EngineLauncher(threading.Thread):
	def __init__(self, engine, what):
		threading.Thread.__init__(self)
		self.engine = engine
		self.what = what
	def run(self):
		self.engine.search(self.what)

if __name__ == '__main__':
	if len(sys.argv) < 2:
		raise SystemExit('./nova.py [all|engine1[,engine2]*] <keywords>\navailable engines: %s'%
				(','.join(supported_engines)))

	if len(sys.argv) == 2:
		if sys.argv[1] == "--supported_engines":
			print ','.join(supported_engines)
			sys.exit(0)
		elif sys.argv[1] == "--supported_engines_infos":
			res = []
			for e in supported_engines:
				exec "res.append(%s().name+'|'+%s().url)"%(e,e)
			print ','.join(res)
			sys.exit(0)
		else:
			raise SystemExit('./nova.py [all|engine1[,engine2]*] <keywords>\navailable engines: %s'%
					(','.join(supported_engines)))

	engines_list = [e.lower() for e in sys.argv[1].strip().split(',')]

	if 'all' in engines_list:
		engines_list = supported_engines

	what = '+'.join(sys.argv[2:])

	threads = []
	for engine in engines_list:
		try:
			if THREADED:
				exec "l = EngineLauncher(%s(), what)" % engine
				threads.append(l)
				l.start()
			else:
				engine().search(what)
		except:
			pass
	if THREADED:
		for t in threads:
			t.join()
