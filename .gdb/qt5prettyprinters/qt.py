# -*- coding: iso-8859-1 -*-
# Pretty-printers for Qt 4 and Qt 5.

# Copyright (C) 2009 Niko Sams <niko.sams@gmail.com>

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import gdb
import itertools
import re
import time

from helper import *

class QStringPrinter:

    def __init__(self, val):
        self.val = val

    def to_string(self):
        size = self.val['d']['size']
        ret = ""

        # The QString object might be not yet initialized. In this case size is a bogus value
        # and the following 2 lines might throw memory access error. Hence the try/catch.
        try:
            isQt4 = has_field(self.val['d'], 'data') # Qt4 has d->data, Qt5 doesn't.
            if isQt4:
                dataAsCharPointer = self.val['d']['data'].cast(gdb.lookup_type("char").pointer())
            else:
                dataAsCharPointer = (self.val['d'] + 1).cast(gdb.lookup_type("char").pointer())
            ret = dataAsCharPointer.string(encoding = 'UTF-16', length = size * 2)
        except Exception:
            # swallow the exception and return empty string
            pass
        return ret

    def display_hint (self):
        return 'string'

class QByteArrayPrinter:

    def __init__(self, val):
        self.val = val
        # Qt4 has 'data', Qt5 doesn't
        self.isQt4 = has_field(self.val['d'], 'data')

    class _iterator(Iterator):
        def __init__(self, data, size):
            self.data = data
            self.size = size
            self.count = 0

        def __iter__(self):
            return self

        def __next__(self):
            if self.count >= self.size:
                raise StopIteration
            count = self.count
            self.count = self.count + 1
            return ('[%d]' % count, self.data[count])

    def stringData(self):
        if self.isQt4:
            return self.val['d']['data']
        else:
            return self.val['d'].cast(gdb.lookup_type("char").const().pointer()) + self.val['d']['offset']

    def children(self):
        return self._iterator(self.stringData(), self.val['d']['size'])

    def to_string(self):
        #todo: handle charset correctly
        return self.stringData()

    def display_hint (self):
        return 'string'

class QListPrinter:
    "Print a QList"

    class _iterator(Iterator):
        def __init__(self, nodetype, d):
            self.nodetype = nodetype
            self.d = d
            self.count = 0

            #from QTypeInfo::isLarge
            isLarge = self.nodetype.sizeof > gdb.lookup_type('void').pointer().sizeof

            isPointer = self.nodetype.code == gdb.TYPE_CODE_PTR

            #unfortunately we can't use QTypeInfo<T>::isStatic as it's all inlined, so use
            #this list of types that use Q_DECLARE_TYPEINFO(T, Q_MOVABLE_TYPE)
            #(obviously it won't work for custom types)
            movableTypes = ['QRect', 'QRectF', 'QString', 'QMargins', 'QLocale', 'QChar', 'QDate', 'QTime', 'QDateTime', 'QVector',
               'QRegExpr', 'QPoint', 'QPointF', 'QByteArray', 'QSize', 'QSizeF', 'QBitArray', 'QLine', 'QLineF', 'QModelIndex', 'QPersitentModelIndex',
               'QVariant', 'QFileInfo', 'QUrl', 'QXmlStreamAttribute', 'QXmlStreamNamespaceDeclaration', 'QXmlStreamNotationDeclaration',
               'QXmlStreamEntityDeclaration', 'QPair<int, int>']
            #this list of types that use Q_DECLARE_TYPEINFO(T, Q_PRIMITIVE_TYPE) (from qglobal.h)
            primitiveTypes = ['bool', 'char', 'signed char', 'unsigned char', 'short', 'unsigned short', 'int', 'unsigned int', 'long', 'unsigned long', 'long long', 'unsigned long long', 'float', 'double']

            if movableTypes.count(self.nodetype.tag) or primitiveTypes.count(str(self.nodetype)):
               isStatic = False
            else:
               isStatic = not isPointer

            self.externalStorage = isLarge or isStatic #see QList::Node::t()


        def __iter__(self):
            return self

        def __next__(self):
            if self.count >= self.d['end'] - self.d['begin']:
                raise StopIteration
            count = self.count
            array = self.d['array'].address + self.d['begin'] + count
            if self.externalStorage:
                value = array.cast(gdb.lookup_type('QList<%s>::Node' % self.nodetype).pointer())['v']
            else:
                value = array
            self.count = self.count + 1
            return ('[%d]' % count, value.cast(self.nodetype.pointer()).dereference())

    def __init__(self, val, container, itype):
        self.d = val['d']
        self.container = container
        self.size = self.d['end'] - self.d['begin']
        if itype == None:
            self.itype = val.type.template_argument(0)
        else:
            self.itype = gdb.lookup_type(itype)

    def children(self):
        return self._iterator(self.itype, self.d)

    def to_string(self):
        return "%s<%s> (size = %s)" % ( self.container, self.itype, self.size )

class QVectorPrinter:
    "Print a QVector"

    class _iterator(Iterator):
        def __init__(self, nodetype, data, size):
            self.nodetype = nodetype
            self.data = data
            self.size = size
            self.count = 0

        def __iter__(self):
            return self

        def __next__(self):
            if self.count >= self.size:
                raise StopIteration
            count = self.count

            self.count = self.count + 1
            return ('[%d]' % count, self.data[count])

    def __init__(self, val, container):
        self.val = val
        self.container = container
        self.itype = self.val.type.template_argument(0)

    def children(self):
        isQt4 = has_field(self.val['d'], 'p') # Qt4 has 'p', Qt5 doesn't
        if isQt4:
            return self._iterator(self.itype, self.val['p']['array'], self.val['p']['size'])
        else:
            data = self.val['d'].cast(gdb.lookup_type("char").const().pointer()) + self.val['d']['offset']
            return self._iterator(self.itype, data.cast(self.itype.pointer()), self.val['d']['size'])

    def to_string(self):
        size = self.val['d']['size']

        return "%s<%s> (size = %s)" % ( self.container, self.itype, size )

class QLinkedListPrinter:
    "Print a QLinkedList"

    class _iterator(Iterator):
        def __init__(self, nodetype, begin, size):
            self.nodetype = nodetype
            self.it = begin
            self.pos = 0
            self.size = size

        def __iter__(self):
            return self

        def __next__(self):
            if self.pos >= self.size:
                raise StopIteration

            pos = self.pos
            val = self.it['t']
            self.it = self.it['n']
            self.pos = self.pos + 1
            return ('[%d]' % pos, val)

    def __init__(self, val):
        self.val = val
        self.itype = self.val.type.template_argument(0)

    def children(self):
        return self._iterator(self.itype, self.val['e']['n'], self.val['d']['size'])

    def to_string(self):
        size = self.val['d']['size']

        return "QLinkedList<%s> (size = %s)" % ( self.itype, size )

class QMapPrinter:
    "Print a QMap"

    class _iteratorQt4(Iterator):
        def __init__(self, val):
            self.val = val
            self.ktype = self.val.type.template_argument(0)
            self.vtype = self.val.type.template_argument(1)
            self.data_node = self.val['e']['forward'][0]
            self.count = 0

        def __iter__(self):
            return self

        def payload (self):
            if gdb.parse_and_eval:
                ret = int(gdb.parse_and_eval('QMap<%s, %s>::payload()' % (self.ktype, self.vtype)))
                if (ret): return ret;

            #if the inferior function call didn't work, let's try to calculate ourselves

            #we can't use QMapPayloadNode as it's inlined
            #as a workaround take the sum of sizeof(members)
            ret = self.ktype.sizeof
            ret += self.vtype.sizeof
            ret += gdb.lookup_type('void').pointer().sizeof

            #but because of data alignment the value can be higher
            #so guess it's aliged by sizeof(void*)
            #TODO: find a real solution for this problem
            ret += ret % gdb.lookup_type('void').pointer().sizeof

            #for some reason booleans are different
            if str(self.vtype) == 'bool':
                ret += 2

            ret -= gdb.lookup_type('void').pointer().sizeof

            return ret

        def concrete (self, data_node):
            node_type = gdb.lookup_type('QMapNode<%s, %s>' % (self.ktype, self.vtype)).pointer()
            return (data_node.cast(gdb.lookup_type('char').pointer()) - self.payload()).cast(node_type)

        def __next__(self):
            if self.data_node == self.val['e']:
                raise StopIteration
            node = self.concrete(self.data_node).dereference()
            if self.count % 2 == 0:
                item = node['key']
            else:
                item = node['value']
                self.data_node = node['forward'][0]

            result = ('[%d]' % self.count, item)
            self.count = self.count + 1
            return result

    class _iteratorQt5:
        def __init__(self, val):
            realtype = val.type.strip_typedefs()
            keytype = realtype.template_argument(0)
            valtype = realtype.template_argument(1)
            node_type = gdb.lookup_type('QMapData<' + keytype.name + ',' + valtype.name + '>::Node')
            self.node_p_type = node_type.pointer()
            self.root = val['d']['header']
            self.current = None
            self.next_is_key = True
            self.i = -1
            # we store the path here to avoid keeping re-fetching
            # values from the inferior (also, skips the pointer
            # arithmetic involved in using the parent pointer)
            self.path = []

        def __iter__(self):
            return self

        def moveToNextNode(self):
            if self.current is None:
                # find the leftmost node
                if not self.root['left']:
                    return False
                self.current = self.root
                while self.current['left']:
                    self.path.append(self.current)
                    self.current = self.current['left']
            elif self.current['right']:
                self.path.append(self.current)
                self.current = self.current['right']
                while self.current['left']:
                    self.path.append(self.current)
                    self.current = self.current['left']
            else:
                last = self.current
                self.current = self.path.pop()
                while self.current['right'] == last:
                    last = self.current
                    self.current = self.path.pop()
                # if there are no more parents, we are at the root
                if len(self.path) == 0:
                    return False
            return True

        def __next__(self):
            if self.next_is_key:
                if not self.moveToNextNode():
                    raise StopIteration
                self.current_typed = self.current.reinterpret_cast(self.node_p_type)
                self.next_is_key = False
                self.i += 1
                return ('key' + str(self.i), self.current_typed['key'])
            else:
                self.next_is_key = True
                return ('value' + str(self.i), self.current_typed['value'])

        def next(self):
            return self.__next__()

    def __init__(self, val, container):
        self.val = val
        self.container = container

    def children(self):
        if self.val['d']['size'] == 0:
            return []

        isQt4 = has_field(self.val, 'e') # Qt4 has 'e', Qt5 doesn't
        if isQt4:
            return self._iteratorQt4(self.val)
        else:
            return self._iteratorQt5(self.val)

    def to_string(self):
        size = self.val['d']['size']

        return "%s<%s, %s> (size = %s)" % ( self.container, self.val.type.template_argument(0), self.val.type.template_argument(1), size )

    def display_hint (self):
        return 'map'

class QHashPrinter:
    "Print a QHash"

    class _iterator(Iterator):
        def __init__(self, val):
            self.val = val
            self.d = self.val['d']
            self.ktype = self.val.type.template_argument(0)
            self.vtype = self.val.type.template_argument(1)
            self.end_node = self.d.cast(gdb.lookup_type('QHashData::Node').pointer())
            self.data_node = self.firstNode()
            self.count = 0

        def __iter__(self):
            return self

        def hashNode (self):
            "Casts the current QHashData::Node to a QHashNode and returns the result. See also QHash::concrete()"
            return self.data_node.cast(gdb.lookup_type('QHashNode<%s, %s>' % (self.ktype, self.vtype)).pointer())

        def firstNode (self):
            "Get the first node, See QHashData::firstNode()."
            e = self.d.cast(gdb.lookup_type('QHashData::Node').pointer())
            #print "QHashData::firstNode() e %s" % e
            bucketNum = 0
            bucket = self.d['buckets'][bucketNum]
            #print "QHashData::firstNode() *bucket %s" % bucket
            n = self.d['numBuckets']
            #print "QHashData::firstNode() n %s" % n
            while n:
                #print "QHashData::firstNode() in while, n %s" % n;
                if bucket != e:
                    #print "QHashData::firstNode() in while, return *bucket %s" % bucket
                    return bucket
                bucketNum += 1
                bucket = self.d['buckets'][bucketNum]
                #print "QHashData::firstNode() in while, new bucket %s" % bucket
                n -= 1
            #print "QHashData::firstNode() return e %s" % e
            return e


        def nextNode (self, node):
            "Get the nextNode after the current, see also QHashData::nextNode()."
            #print "******************************** nextNode"
            #print "nextNode: node %s" % node
            next = node['next'].cast(gdb.lookup_type('QHashData::Node').pointer())
            e = next

            #print "nextNode: next %s" % next
            if next['next']:
                #print "nextNode: return next"
                return next

            #print "nextNode: node->h %s" % node['h']
            #print "nextNode: numBuckets %s" % self.d['numBuckets']
            start = (node['h'] % self.d['numBuckets']) + 1
            bucketNum = start
            #print "nextNode: start %s" % start
            bucket = self.d['buckets'][start]
            #print "nextNode: bucket %s" % bucket
            n = self.d['numBuckets'] - start
            #print "nextNode: n %s" % n
            while n:
                #print "nextNode: in while; n %s" % n
                #print "nextNode: in while; e %s" % e
                #print "nextNode: in while; *bucket %s" % bucket
                if bucket != e:
                    #print "nextNode: in while; return bucket %s" % bucket
                    return bucket
                bucketNum += 1
                bucket = self.d['buckets'][bucketNum]
                n -= 1
            #print "nextNode: return e %s" % e
            return e

        def __next__(self):
            "GDB iteration, first call returns key, second value and then jumps to the next hash node."
            if self.data_node == self.end_node:
                raise StopIteration

            node = self.hashNode()

            if self.count % 2 == 0:
                item = node['key']
            else:
                item = node['value']
                self.data_node = self.nextNode(self.data_node)

            self.count = self.count + 1
            return ('[%d]' % self.count, item)

    def __init__(self, val, container):
        self.val = val
        self.container = container

    def children(self):
        return self._iterator(self.val)

    def to_string(self):
        size = self.val['d']['size']

        return "%s<%s, %s> (size = %s)" % ( self.container, self.val.type.template_argument(0), self.val.type.template_argument(1), size )

    def display_hint (self):
        return 'map'

class QDatePrinter:

    def __init__(self, val):
        self.val = val

    def to_string(self):
        julianDay = self.val['jd']

        if julianDay == 0:
            return "invalid QDate"

        # Copied from Qt sources
        if julianDay >= 2299161:
            # Gregorian calendar starting from October 15, 1582
            # This algorithm is from Henry F. Fliegel and Thomas C. Van Flandern
            ell = julianDay + 68569;
            n = (4 * ell) / 146097;
            ell = ell - (146097 * n + 3) / 4;
            i = (4000 * (ell + 1)) / 1461001;
            ell = ell - (1461 * i) / 4 + 31;
            j = (80 * ell) / 2447;
            d = ell - (2447 * j) / 80;
            ell = j / 11;
            m = j + 2 - (12 * ell);
            y = 100 * (n - 49) + i + ell;
        else:
            # Julian calendar until October 4, 1582
            # Algorithm from Frequently Asked Questions about Calendars by Claus Toendering
            julianDay += 32082;
            dd = (4 * julianDay + 3) / 1461;
            ee = julianDay - (1461 * dd) / 4;
            mm = ((5 * ee) + 2) / 153;
            d = ee - (153 * mm + 2) / 5 + 1;
            m = mm + 3 - 12 * (mm / 10);
            y = dd - 4800 + (mm / 10);
            if y <= 0:
                --y;
        return "%d-%02d-%02d" % (y, m, d)

class QTimePrinter:

    def __init__(self, val):
        self.val = val

    def to_string(self):
        ds = self.val['mds']

        if ds == -1:
            return "invalid QTime"

        MSECS_PER_HOUR = 3600000
        SECS_PER_MIN = 60
        MSECS_PER_MIN = 60000

        hour = ds / MSECS_PER_HOUR
        minute = (ds % MSECS_PER_HOUR) / MSECS_PER_MIN
        second = (ds / 1000)%SECS_PER_MIN
        msec = ds % 1000
        return "%02d:%02d:%02d.%03d" % (hour, minute, second, msec)

class QDateTimePrinter:

    def __init__(self, val):
        self.val = val

    def to_string(self):
        time_t = gdb.parse_and_eval("reinterpret_cast<const QDateTime*>(%s)->toSecsSinceEpoch()" % self.val.address)
        return time.ctime(int(time_t))

class QUrlPrinter:

    def __init__(self, val):
        self.val = val

    def to_string(self):
        # first try to access the Qt 5 data
        try:
            int_type = gdb.lookup_type('int')
            string_type = gdb.lookup_type('QString')
            string_pointer = string_type.pointer()

            addr = self.val['d'].cast(gdb.lookup_type('char').pointer())
            # skip QAtomicInt ref
            addr += int_type.sizeof
            # handle int port
            port = addr.cast(int_type.pointer()).dereference()
            addr += int_type.sizeof
            # handle QString scheme
            scheme = QStringPrinter(addr.cast(string_pointer).dereference()).to_string()
            addr += string_type.sizeof
            # handle QString username
            username = QStringPrinter(addr.cast(string_pointer).dereference()).to_string()
            addr += string_type.sizeof
            # skip QString password
            addr += string_type.sizeof
            # handle QString host
            host = QStringPrinter(addr.cast(string_pointer).dereference()).to_string()
            addr += string_type.sizeof
            # handle QString path
            path = QStringPrinter(addr.cast(string_pointer).dereference()).to_string()
            addr += string_type.sizeof
            # handle QString query
            query = QStringPrinter(addr.cast(string_pointer).dereference()).to_string()
            addr += string_type.sizeof
            # handle QString fragment
            fragment = QStringPrinter(addr.cast(string_pointer).dereference()).to_string()

            url = ""
            if len(scheme) > 0:
                # TODO: always adding // is apparently not compliant in all cases
                url += scheme + "://"
            if len(host) > 0:
                if len(username) > 0:
                    url += username + "@"
                url += host
                if port != -1:
                    url += ":" + str(port)
            url += path
            if len(query) > 0:
                url += "?" + query
            if len(fragment) > 0:
                url += "#" + fragment

            return url
        except:
            pass
        # then try to print directly, but that might lead to issues (see http://sourceware-org.1504.n7.nabble.com/help-Calling-malloc-from-a-Python-pretty-printer-td284031.html)
        try:
            return gdb.parse_and_eval("reinterpret_cast<const QUrl*>(%s)->toString((QUrl::FormattingOptions)QUrl::PrettyDecoded)" % self.val.address)
        except:
            pass
        # if everything fails, maybe we deal with Qt 4 code
        try:
            return self.val['d']['encodedOriginal']
        except RuntimeError:
            #if no debug information is available for Qt, try guessing the correct address for encodedOriginal
            #problem with this is that if QUrlPrivate members get changed, this fails
            offset = gdb.lookup_type('int').sizeof
            offset += offset % gdb.lookup_type('void').pointer().sizeof #alignment
            offset += gdb.lookup_type('QString').sizeof * 6
            offset += gdb.lookup_type('QByteArray').sizeof
            encodedOriginal = self.val['d'].cast(gdb.lookup_type('char').pointer());
            encodedOriginal += offset
            encodedOriginal = encodedOriginal.cast(gdb.lookup_type('QByteArray').pointer()).dereference();
            encodedOriginal = encodedOriginal['d']['data'].string()
            return encodedOriginal

class QSetPrinter:
    "Print a QSet"

    def __init__(self, val):
        self.val = val

    class _iterator(Iterator):
        def __init__(self, hashIterator):
            self.hashIterator = hashIterator
            self.count = 0

        def __iter__(self):
            return self

        def __next__(self):
            if self.hashIterator.data_node == self.hashIterator.end_node:
                raise StopIteration

            node = self.hashIterator.hashNode()

            item = node['key']
            self.hashIterator.data_node = self.hashIterator.nextNode(self.hashIterator.data_node)

            self.count = self.count + 1
            return ('[%d]' % (self.count-1), item)

    def children(self):
        hashPrinter = QHashPrinter(self.val['q_hash'], None)
        hashIterator = hashPrinter._iterator(self.val['q_hash'])
        return self._iterator(hashIterator)

    def to_string(self):
        size = self.val['q_hash']['d']['size']

        return "QSet<%s> (size = %s)" % ( self.val.type.template_argument(0), size )


class QCharPrinter:

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return unichr(self.val['ucs'])

    def display_hint (self):
        return 'string'

class QUuidPrinter:

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return "QUuid({%x-%x-%x-%x%x-%x%x%x%x%x%x})" % (int(self.val['data1']), int(self.val['data2']), int(self.val['data3']),
                                            int(self.val['data4'][0]), int(self.val['data4'][1]),
                                            int(self.val['data4'][2]), int(self.val['data4'][3]),
                                            int(self.val['data4'][4]), int(self.val['data4'][5]),
                                            int(self.val['data4'][6]), int(self.val['data4'][7]))

    def display_hint (self):
        return 'string'

class QVariantPrinter:

    def __init__(self, val):
        self.val = val

    def to_string(self):
        d = self.val['d']

        if d['is_null']:
            return "QVariant(NULL)"

        data_type = d['type']
        type_str = ("type = %d" % data_type)
        try:
            typeAsCharPointer = (gdb.parse_and_eval("QVariant::typeToName(%d)" % data_type).cast(gdb.lookup_type("char").pointer()))
            if typeAsCharPointer:
                type_str = typeAsCharPointer.string(encoding = 'UTF-8')
        except Exception as e:
            pass

        data = d['data']
        is_shared = d['is_shared']
        value_str = ""
        if is_shared:
            private_shared = data['shared'].dereference()
            value_str = "PrivateShared(%s)" % hex(private_shared['ptr'])
        elif type_str.startswith("type = "):
            value_str = str(data['ptr'])
        else:
            type_obj = None
            try:
                type_obj = gdb.lookup_type(type_str)
            except Exception as e:
                value_str = str(data['ptr'])
            if type_obj:
                if type_obj.sizeof > type_obj.pointer().sizeof:
                    value_ptr = data['ptr'].reinterpret_cast(type_obj.const().pointer())
                    value_str = str(value_ptr.dereference())
                else:
                    value_ptr = data['c'].address.reinterpret_cast(type_obj.const().pointer())
                    value_str = str(value_ptr.dereference())

        return "QVariant(%s, %s)" % (type_str, value_str)

pretty_printers_dict = {}

def register_qt_printers (obj):
    if obj == None:
        obj = gdb

    obj.pretty_printers.append(FunctionLookup(gdb, pretty_printers_dict))

def build_dictionary ():
    pretty_printers_dict[re.compile('^QString$')] = lambda val: QStringPrinter(val)
    pretty_printers_dict[re.compile('^QByteArray$')] = lambda val: QByteArrayPrinter(val)
    pretty_printers_dict[re.compile('^QList<.*>$')] = lambda val: QListPrinter(val, 'QList', None)
    pretty_printers_dict[re.compile('^QStringList$')] = lambda val: QListPrinter(val, 'QStringList', 'QString')
    pretty_printers_dict[re.compile('^QQueue')] = lambda val: QListPrinter(val, 'QQueue', None)
    pretty_printers_dict[re.compile('^QVector<.*>$')] = lambda val: QVectorPrinter(val, 'QVector')
    pretty_printers_dict[re.compile('^QStack<.*>$')] = lambda val: QVectorPrinter(val, 'QStack')
    pretty_printers_dict[re.compile('^QLinkedList<.*>$')] = lambda val: QLinkedListPrinter(val)
    pretty_printers_dict[re.compile('^QMap<.*>$')] = lambda val: QMapPrinter(val, 'QMap')
    pretty_printers_dict[re.compile('^QMultiMap<.*>$')] = lambda val: QMapPrinter(val, 'QMultiMap')
    pretty_printers_dict[re.compile('^QHash<.*>$')] = lambda val: QHashPrinter(val, 'QHash')
    pretty_printers_dict[re.compile('^QMultiHash<.*>$')] = lambda val: QHashPrinter(val, 'QMultiHash')
    pretty_printers_dict[re.compile('^QDate$')] = lambda val: QDatePrinter(val)
    pretty_printers_dict[re.compile('^QTime$')] = lambda val: QTimePrinter(val)
    pretty_printers_dict[re.compile('^QDateTime$')] = lambda val: QDateTimePrinter(val)
    pretty_printers_dict[re.compile('^QUrl$')] = lambda val: QUrlPrinter(val)
    pretty_printers_dict[re.compile('^QSet<.*>$')] = lambda val: QSetPrinter(val)
    pretty_printers_dict[re.compile('^QChar$')] = lambda val: QCharPrinter(val)
    pretty_printers_dict[re.compile('^QUuid')] = lambda val: QUuidPrinter(val)
    pretty_printers_dict[re.compile('^QVariant')] = lambda val: QVariantPrinter(val)


build_dictionary ()
