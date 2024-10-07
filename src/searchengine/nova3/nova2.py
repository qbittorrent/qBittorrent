#VERSION: 1.48

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
import traceback
import urllib.parse
import xml.etree.ElementTree as ET
from collections.abc import Iterable
from enum import Enum
from glob import glob
from multiprocessing import Pool, cpu_count
from os import path
from typing import Optional

THREADED: bool = True
try:
    MAX_THREADS: int = cpu_count()
except NotImplementedError:
    MAX_THREADS = 1

Category = Enum('Category', ['all', 'anime', 'books', 'games', 'movies', 'music', 'pictures', 'software', 'tv'])


################################################################################
# Every engine should have a "search" method taking
# a space-free string as parameter (ex. "family+guy")
# it should call prettyPrinter() with a dict as parameter.
# The keys in the dict must be: link,name,size,seeds,leech,engine_url
# As a convention, try to list results by decreasing number of seeds or similar
################################################################################


EngineModuleName = str  # the filename of the engine plugin


class Engine:
    url: str
    name: str
    supported_categories: dict[str, str]

    def __init__(self) -> None:
        pass

    def search(self, what: str, cat: str = Category.all.name) -> None:
        pass

    def download_torrent(self, info: str) -> None:
        pass


# global state
engine_dict: dict[EngineModuleName, Optional[type[Engine]]] = {}


def list_engines() -> list[EngineModuleName]:
    """ List all engines,
        including broken engines that would fail on import

        Return list of all engines' module name
    """

    names = []

    for engine_path in glob(path.join(path.dirname(__file__), 'engines', '*.py')):
        engine_module_name = path.basename(engine_path).split('.')[0].strip()
        if len(engine_module_name) == 0 or engine_module_name.startswith('_'):
            continue
        names.append(engine_module_name)

    return sorted(names)


def import_engine(engine_module_name: EngineModuleName) -> Optional[type[Engine]]:
    if engine_module_name in engine_dict:
        return engine_dict[engine_module_name]

    # when import fails, return `None`
    engine_class = None
    try:
        # import engines.[engine_module_name]
        engine_module = importlib.import_module(f"engines.{engine_module_name}")
        engine_class = getattr(engine_module, engine_module_name)
    except Exception:
        pass

    engine_dict[engine_module_name] = engine_class
    return engine_class


def get_capabilities(engines: Iterable[EngineModuleName]) -> str:
    """
    Return capabilities in XML format
    <capabilities>
      <engine_module_name>
        <name>long name</name>
        <url>http://example.com</url>
        <categories>movies music games</categories>
      </engine_module_name>
    </capabilities>
    """

    capabilities_element = ET.Element('capabilities')

    for engine_module_name in engines:
        engine_class = import_engine(engine_module_name)
        if engine_class is None:
            continue

        engine_module_element = ET.SubElement(capabilities_element, engine_module_name)

        ET.SubElement(engine_module_element, 'name').text = engine_class.name
        ET.SubElement(engine_module_element, 'url').text = engine_class.url

        supported_categories = ""
        if hasattr(engine_class, "supported_categories"):
            supported_categories = " ".join((key
                                             for key in sorted(engine_class.supported_categories.keys())
                                             if key != Category.all.name))
        ET.SubElement(engine_module_element, 'categories').text = supported_categories

    ET.indent(capabilities_element)
    return ET.tostring(capabilities_element, 'unicode')


def run_search(search_params: tuple[type[Engine], str, Category]) -> bool:
    """ Run search in engine

        @param search_params Tuple with engine, query and category

        @retval False if any exceptions occurred
        @retval True  otherwise
    """

    engine_class, what, cat = search_params
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
        traceback.print_exc()
        return False


if __name__ == "__main__":
    def main() -> int:
        # qbt tend to run this script in 'isolate mode' so append the current path manually
        current_path = str(pathlib.Path(__file__).parent.resolve())
        if current_path not in sys.path:
            sys.path.append(current_path)

        # https://docs.python.org/3/library/sys.html#sys.exit
        class ExitCode(Enum):
            OK = 0
            AppError = 1
            ArgError = 2

        found_engines = list_engines()

        prog_name = sys.argv[0]
        prog_usage = (f"Usage: {prog_name} all|engine1[,engine2]* <category> <keywords>\n"
                      f"To list available engines: {prog_name} --capabilities [--names]\n"
                      f"Found engines: {','.join(found_engines)}")

        if "--capabilities" in sys.argv:
            if "--names" in sys.argv:
                print(",".join((e for e in found_engines if import_engine(e) is not None)))
                return ExitCode.OK.value

            print(get_capabilities(found_engines))
            return ExitCode.OK.value
        elif len(sys.argv) < 4:
            print(prog_usage, file=sys.stderr)
            return ExitCode.ArgError.value

        # get unique engines
        engs = set(arg.strip().lower() for arg in sys.argv[1].split(','))
        engines = found_engines if 'all' in engs else [e for e in found_engines if e in engs]

        cat = sys.argv[2].lower()
        try:
            category = Category[cat]
        except KeyError:
            print(f"Invalid category: {cat}", file=sys.stderr)
            return ExitCode.ArgError.value

        what = urllib.parse.quote(' '.join(sys.argv[3:]))
        params = ((engine_class, what, category) for e in engines if (engine_class := import_engine(e)) is not None)

        search_success = False
        if THREADED:
            processes = max(min(len(engines), MAX_THREADS), 1)
            with Pool(processes) as pool:
                search_success = all(pool.map(run_search, params))
        else:
            search_success = all(map(run_search, params))

        return ExitCode.OK.value if search_success else ExitCode.AppError.value

    sys.exit(main())
