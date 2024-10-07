#VERSION: 1.47

# Author:
#  Fabien Devaux <fab AT gnux DOT info>
# Contributors:
#  Christophe Dumez <chris@qbittorrent.org> (qbittorrent integration)
#  Thanks to gab #gcu @ irc.freenode.net (multipage support on PirateBay)
#  Thanks to Elias <gekko04@users.sourceforge.net> (torrentreactor and isohunt search engines)
#
# Licence: BSD

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#    * Redistributions of source code must retain the above copyright notice,
#      this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#    * Neither the name of the author nor the names of its contributors may be
#      used to endorse or promote products derived from this software without
#      specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

import importlib
import pathlib
import sys
import urllib.parse
from enum import Enum
from glob import glob
from multiprocessing import Pool, cpu_count
from os import path
from typing import Dict, Iterable, Iterator, List, Optional, Sequence, Set, Tuple, Type

THREADED: bool = True
try:
    MAX_THREADS: int = cpu_count()
except NotImplementedError:
    MAX_THREADS = 1

Category = Enum('Category', ['all', 'movies', 'tv', 'music', 'games', 'anime', 'software', 'pictures', 'books'])


################################################################################
# Every engine should have a "search" method taking
# a space-free string as parameter (ex. "family+guy")
# it should call prettyPrinter() with a dict as parameter.
# The keys in the dict must be: link,name,size,seeds,leech,engine_url
# As a convention, try to list results by decreasing number of seeds or similar
################################################################################


EngineName = str


class Engine:
    url: str
    name: EngineName
    supported_categories: Dict[str, str]

    def __init__(self) -> None:
        pass

    def search(self, what: str, cat: str = Category.all.name) -> None:
        pass

    def download_torrent(self, info: str) -> None:
        pass


# global state
engine_dict: Dict[EngineName, Optional[Type[Engine]]] = {}


def list_engines() -> List[EngineName]:
    """ List all engines,
        including broken engines that fail on import

        Faster than initialize_engines

        Return list of all engines
    """
    found_engines = []

    for engine_path in glob(path.join(path.dirname(__file__), 'engines', '*.py')):
        engine_name = path.basename(engine_path).split('.')[0].strip()
        if len(engine_name) == 0 or engine_name.startswith('_'):
            continue
        found_engines.append(engine_name)

    return found_engines


def get_engine(engine_name: EngineName) -> Optional[Type[Engine]]:
    if engine_name in engine_dict:
        return engine_dict[engine_name]

    # when import fails, engine is None
    engine = None
    try:
        # import engines.[engine]
        engine_module = importlib.import_module("engines." + engine_name)
        engine = getattr(engine_module, engine_name)
    except Exception:
        pass
    engine_dict[engine_name] = engine
    return engine


def initialize_engines(found_engines: Iterable[EngineName]) -> Set[EngineName]:
    """ Import available engines

        Return set of available engines
    """
    supported_engines = set()

    for engine_name in found_engines:
        # import engine
        engine = get_engine(engine_name)
        if engine is None:
            continue
        supported_engines.add(engine_name)

    return supported_engines


def engines_to_xml(supported_engines: Iterable[EngineName]) -> Iterator[str]:
    """ Generates xml for supported engines """
    tab = " " * 4

    for engine_name in supported_engines:
        search_engine = get_engine(engine_name)
        if search_engine is None:
            continue

        supported_categories = ""
        if hasattr(search_engine, "supported_categories"):
            supported_categories = " ".join((key
                                             for key in search_engine.supported_categories.keys()
                                             if key != Category.all.name))

        yield "".join((tab, "<", engine_name, ">\n",
                       tab, tab, "<name>", search_engine.name, "</name>\n",
                       tab, tab, "<url>", search_engine.url, "</url>\n",
                       tab, tab, "<categories>", supported_categories, "</categories>\n",
                       tab, "</", engine_name, ">\n"))


def displayCapabilities(supported_engines: Iterable[EngineName]) -> None:
    """
    Display capabilities in XML format
    <capabilities>
      <engine_short_name>
        <name>long name</name>
        <url>http://example.com</url>
        <categories>movies music games</categories>
      </engine_short_name>
    </capabilities>
    """
    xml = "".join(("<capabilities>\n",
                   "".join(engines_to_xml(supported_engines)),
                   "</capabilities>"))
    print(xml)


def run_search(engine_list: Tuple[Optional[Type[Engine]], str, Category]) -> bool:
    """ Run search in engine

        @param engine_list Tuple with engine, query and category

        @retval False if any exceptions occurred
        @retval True  otherwise
    """
    engine_class, what, cat = engine_list
    if engine_class is None:
        return False

    try:
        engine = engine_class()
        # avoid exceptions due to invalid category
        if hasattr(engine, 'supported_categories'):
            if cat.name in engine.supported_categories:
                engine.search(what, cat.name)
        else:
            engine.search(what)

        return True
    except Exception:
        return False


def main(args: Sequence[str]) -> None:
    # qbt tend to run this script in 'isolate mode' so append the current path manually
    current_path = str(pathlib.Path(__file__).parent.resolve())
    if current_path not in sys.path:
        sys.path.append(current_path)

    found_engines = list_engines()

    def show_usage() -> None:
        print("./nova2.py all|engine1[,engine2]* <category> <keywords>", file=sys.stderr)
        print("found engines: " + ','.join(found_engines), file=sys.stderr)
        print("to list available engines: ./nova2.py --capabilities [--names]", file=sys.stderr)

    if not args:
        show_usage()
        sys.exit(1)
    elif args[0] == "--capabilities":
        supported_engines = initialize_engines(found_engines)
        if "--names" in args:
            print(",".join(supported_engines))
            return
        displayCapabilities(supported_engines)
        return
    elif len(args) < 3:
        show_usage()
        sys.exit(1)

    cat = args[1].lower()
    try:
        category = Category[cat]
    except KeyError:
        print(" - ".join(('Invalid category', cat)), file=sys.stderr)
        sys.exit(1)

    # get only unique engines with set
    engines_list = set(e.lower() for e in args[0].strip().split(','))

    if not engines_list:
        # engine list is empty. Nothing to do here
        return

    if 'all' in engines_list:
        # use all supported engines
        # note: this can be slower than passing a list of supported engines
        # because initialize_engines will also try to import not-supported engines
        engines_list = initialize_engines(found_engines)
    else:
        # discard not-found engines
        engines_list = {engine for engine in engines_list if engine in found_engines}

    what = urllib.parse.quote(' '.join(args[2:]))
    params = ((get_engine(engine_name), what, category) for engine_name in engines_list)

    if THREADED:
        # child process spawning is controlled min(number of searches, number of cpu)
        with Pool(min(len(engines_list), MAX_THREADS)) as pool:
            pool.map(run_search, params)
    else:
        # py3 note: map is needed to be evaluated for content to be executed
        all(map(run_search, params))


if __name__ == "__main__":
    main(sys.argv[1:])
