# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Collection of functions and classes to fix various encoding problems on
multiple platforms with python.
"""

import codecs
import locale
import os
import sys


# Prevents initializing multiple times.
_SYS_ARGV_PROCESSED = False


def complain(message):
  """If any exception occurs in this file, we'll probably try to print it
  on stderr, which makes for frustrating debugging if stderr is directed
  to our wrapper. So be paranoid about catching errors and reporting them
  to sys.__stderr__, so that the user has a higher chance to see them.
  """
  print >> sys.__stderr__, (
      isinstance(message, str) and message or repr(message))


def fix_default_encoding():
  """Forces utf8 solidly on all platforms.

  By default python execution environment is lazy and defaults to ascii
  encoding.

  http://uucode.com/blog/2007/03/23/shut-up-you-dummy-7-bit-python/
  """
  if sys.getdefaultencoding() == 'utf-8':
    return False

  # Regenerate setdefaultencoding.
  reload(sys)
  # Module 'sys' has no 'setdefaultencoding' member
  # pylint: disable=E1101
  sys.setdefaultencoding('utf-8')
  for attr in dir(locale):
    if attr[0:3] != 'LC_':
      continue
    aref = getattr(locale, attr)
    try:
      locale.setlocale(aref, '')
    except locale.Error:
      continue
    try:
      lang = locale.getlocale(aref)[0]
    except (TypeError, ValueError):
      continue
    if lang:
      try:
        locale.setlocale(aref, (lang, 'UTF-8'))
      except locale.Error:
        os.environ[attr] = lang + '.UTF-8'
  try:
    locale.setlocale(locale.LC_ALL, '')
  except locale.Error:
    pass
  return True


###############################
# Windows specific


def fix_win_sys_argv(encoding):
  """Converts sys.argv to 'encoding' encoded string.

  utf-8 is recommended.

  Works around <http://bugs.python.org/issue2128>.
  """
  global _SYS_ARGV_PROCESSED
  if _SYS_ARGV_PROCESSED:
    return False

  # These types are available on linux but not Mac.
  # pylint: disable=E0611,F0401
  from ctypes import byref, c_int, POINTER, windll, WINFUNCTYPE
  from ctypes.wintypes import LPCWSTR, LPWSTR

  # <http://msdn.microsoft.com/en-us/library/ms683156.aspx>
  GetCommandLineW = WINFUNCTYPE(LPWSTR)(('GetCommandLineW', windll.kernel32))
  # <http://msdn.microsoft.com/en-us/library/bb776391.aspx>
  CommandLineToArgvW = WINFUNCTYPE(POINTER(LPWSTR), LPCWSTR, POINTER(c_int))(
      ('CommandLineToArgvW', windll.shell32))

  argc = c_int(0)
  argv_unicode = CommandLineToArgvW(GetCommandLineW(), byref(argc))
  argv = [
      argv_unicode[i].encode(encoding, 'replace')
      for i in xrange(0, argc.value)]

  if not hasattr(sys, 'frozen'):
    # If this is an executable produced by py2exe or bbfreeze, then it
    # will have been invoked directly. Otherwise, unicode_argv[0] is the
    # Python interpreter, so skip that.
    argv = argv[1:]

    # Also skip option arguments to the Python interpreter.
    while len(argv) > 0:
      arg = argv[0]
      if not arg.startswith(u'-') or arg == u'-':
        break
      argv = argv[1:]
      if arg == u'-m':
        # sys.argv[0] should really be the absolute path of the
        # module source, but never mind.
        break
      if arg == u'-c':
        argv[0] = u'-c'
        break
  sys.argv = argv
  _SYS_ARGV_PROCESSED = True
  return True


def fix_win_codec():
  """Works around <http://bugs.python.org/issue6058>."""
  # <http://msdn.microsoft.com/en-us/library/dd317756.aspx>
  try:
    codecs.lookup('cp65001')
    return False
  except LookupError:
    codecs.register(
        lambda name: name == 'cp65001' and codecs.lookup('utf-8') or None)
    return True


class WinUnicodeOutputBase(object):
  """Base class to adapt sys.stdout or sys.stderr to behave correctly on
  Windows.

  Setting encoding to utf-8 is recommended.
  """
  def __init__(self, fileno, name, encoding):
    # Corresponding file handle.
    self._fileno = fileno
    self.encoding = encoding
    self.name = name

    self.closed = False
    self.softspace = False
    self.mode = 'w'

  @staticmethod
  def isatty():
    return False

  def close(self):
    # Don't really close the handle, that would only cause problems.
    self.closed = True

  def fileno(self):
    return self._fileno

  def flush(self):
    raise NotImplementedError()

  def write(self, text):
    raise NotImplementedError()

  def writelines(self, lines):
    try:
      for line in lines:
        self.write(line)
    except Exception, e:
      complain('%s.writelines: %r' % (self.name, e))
      raise


class WinUnicodeConsoleOutput(WinUnicodeOutputBase):
  """Output adapter to a Windows Console.

  Understands how to use the win32 console API.
  """
  def __init__(self, console_handle, fileno, stream_name, encoding):
    super(WinUnicodeConsoleOutput, self).__init__(
        fileno, '<Unicode console %s>' % stream_name, encoding)
    # Handle to use for WriteConsoleW
    self._console_handle = console_handle

    # Loads the necessary function.
    # These types are available on linux but not Mac.
    # pylint: disable=E0611,F0401
    from ctypes import byref, GetLastError, POINTER, windll, WINFUNCTYPE
    from ctypes.wintypes import BOOL, DWORD, HANDLE, LPWSTR
    from ctypes.wintypes import LPVOID  # pylint: disable=E0611

    self._DWORD = DWORD
    self._byref = byref

    # <http://msdn.microsoft.com/en-us/library/ms687401.aspx>
    self._WriteConsoleW = WINFUNCTYPE(
        BOOL, HANDLE, LPWSTR, DWORD, POINTER(DWORD), LPVOID)(
            ('WriteConsoleW', windll.kernel32))
    self._GetLastError = GetLastError

  def flush(self):
    # No need to flush the console since it's immediate.
    pass

  def write(self, text):
    try:
      if not isinstance(text, unicode):
        # Convert to unicode.
        text = str(text).decode(self.encoding, 'replace')
      remaining = len(text)
      while remaining > 0:
        n = self._DWORD(0)
        # There is a shorter-than-documented limitation on the length of the
        # string passed to WriteConsoleW. See
        # <http://tahoe-lafs.org/trac/tahoe-lafs/ticket/1232>.
        retval = self._WriteConsoleW(
            self._console_handle, text,
            min(remaining, 10000),
            self._byref(n), None)
        if retval == 0 or n.value == 0:
          raise IOError(
              'WriteConsoleW returned %r, n.value = %r, last error = %r' % (
                retval, n.value, self._GetLastError()))
        remaining -= n.value
        if not remaining:
          break
        text = text[n.value:]
    except Exception, e:
      complain('%s.write: %r' % (self.name, e))
      raise


class WinUnicodeOutput(WinUnicodeOutputBase):
  """Output adaptor to a file output on Windows.

  If the standard FileWrite function is used, it will be encoded in the current
  code page. WriteConsoleW() permits writting any character.
  """
  def __init__(self, stream, fileno, encoding):
    super(WinUnicodeOutput, self).__init__(
        fileno, '<Unicode redirected %s>' % stream.name, encoding)
    # Output stream
    self._stream = stream

    # Flush right now.
    self.flush()

  def flush(self):
    try:
      self._stream.flush()
    except Exception, e:
      complain('%s.flush: %r from %r' % (self.name, e, self._stream))
      raise

  def write(self, text):
    try:
      if isinstance(text, unicode):
        # Replace characters that cannot be printed instead of failing.
        text = text.encode(self.encoding, 'replace')
      self._stream.write(text)
    except Exception, e:
      complain('%s.write: %r' % (self.name, e))
      raise


def win_handle_is_a_console(handle):
  """Returns True if a Windows file handle is a handle to a console."""
  # These types are available on linux but not Mac.
  # pylint: disable=E0611,F0401
  from ctypes import byref, POINTER, windll, WINFUNCTYPE
  from ctypes.wintypes import BOOL, DWORD, HANDLE

  FILE_TYPE_CHAR   = 0x0002
  FILE_TYPE_REMOTE = 0x8000
  INVALID_HANDLE_VALUE = DWORD(-1).value

  # <http://msdn.microsoft.com/en-us/library/ms683167.aspx>
  GetConsoleMode = WINFUNCTYPE(BOOL, HANDLE, POINTER(DWORD))(
      ('GetConsoleMode', windll.kernel32))
  # <http://msdn.microsoft.com/en-us/library/aa364960.aspx>
  GetFileType = WINFUNCTYPE(DWORD, DWORD)(('GetFileType', windll.kernel32))

  # GetStdHandle returns INVALID_HANDLE_VALUE, NULL, or a valid handle.
  if handle == INVALID_HANDLE_VALUE or handle is None:
    return False
  return (
      (GetFileType(handle) & ~FILE_TYPE_REMOTE) == FILE_TYPE_CHAR and
       GetConsoleMode(handle, byref(DWORD())))


def win_get_unicode_stream(stream, excepted_fileno, output_handle, encoding):
  """Returns a unicode-compatible stream.

  This function will return a direct-Console writing object only if:
  - the file number is the expected console file number
  - the handle the expected file handle
  - the 'real' handle is in fact a handle to a console.
  """
  old_fileno = getattr(stream, 'fileno', lambda: None)()
  if old_fileno == excepted_fileno:
    # These types are available on linux but not Mac.
    # pylint: disable=E0611,F0401
    from ctypes import windll, WINFUNCTYPE
    from ctypes.wintypes import DWORD, HANDLE

    # <http://msdn.microsoft.com/en-us/library/ms683231.aspx>
    GetStdHandle = WINFUNCTYPE(HANDLE, DWORD)(('GetStdHandle', windll.kernel32))

    real_output_handle = GetStdHandle(DWORD(output_handle))
    if win_handle_is_a_console(real_output_handle):
      # It's a console.
      return WinUnicodeConsoleOutput(
          real_output_handle, old_fileno, stream.name, encoding)

  # It's something else. Create an auto-encoding stream.
  return WinUnicodeOutput(stream, old_fileno, encoding)


def fix_win_console(encoding):
  """Makes Unicode console output work independently of the current code page.

  This also fixes <http://bugs.python.org/issue1602>.
  Credit to Michael Kaplan
  <http://blogs.msdn.com/b/michkap/archive/2010/04/07/9989346.aspx> and
  TZOmegaTZIOY
  <http://stackoverflow.com/questions/878972/windows-cmd-encoding-change-causes-python-crash/1432462#1432462>.
  """
  if (isinstance(sys.stdout, WinUnicodeOutputBase) or
      isinstance(sys.stderr, WinUnicodeOutputBase)):
    return False

  try:
    # SetConsoleCP and SetConsoleOutputCP could be used to change the code page
    # but it's not really useful since the code here is using WriteConsoleW().
    # Also, changing the code page is 'permanent' to the console and needs to be
    # reverted manually.
    # In practice one needs to set the console font to a TTF font to be able to
    # see all the characters but it failed for me in practice. In any case, it
    # won't throw any exception when printing, which is the important part.
    # -11 and -12 are defined in stdio.h
    sys.stdout = win_get_unicode_stream(sys.stdout, 1, -11, encoding)
    sys.stderr = win_get_unicode_stream(sys.stderr, 2, -12, encoding)
    # TODO(maruel): Do sys.stdin with ReadConsoleW(). Albeit the limitation is
    # "It doesn't appear to be possible to read Unicode characters in UTF-8
    # mode" and this appears to be a limitation of cmd.exe.
  except Exception, e:
    complain('exception %r while fixing up sys.stdout and sys.stderr' % e)
  return True


def fix_encoding():
  """Fixes various encoding problems on all platforms.

  Should be called at the very begining of the process.
  """
  ret = True
  if sys.platform == 'win32':
    ret &= fix_win_codec()

  ret &= fix_default_encoding()

  if sys.platform == 'win32':
    encoding = sys.getdefaultencoding()
    ret &= fix_win_sys_argv(encoding)
    ret &= fix_win_console(encoding)
  return ret
