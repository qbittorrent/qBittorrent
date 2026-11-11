import json
import os
import plistlib
import shutil
import subprocess  # nosec B404
import tempfile
import unittest
from pathlib import Path
from typing import Optional
from unittest.mock import Mock, patch

from . import package


def commands(dependency: str = '/usr/lib/libSystem.B.dylib',
             rpath: Optional[str] = None, minimum: str = '13.0') -> str:
    text = (f'Load command 0\n cmd LC_LOAD_DYLIB\n name {dependency} (offset 24)\n'
            f'Load command 1\n cmd LC_BUILD_VERSION\n platform 1\n minos {minimum}\n')
    if rpath is not None:
        text += f'Load command 2\n cmd LC_RPATH\n path {rpath} (offset 12)\n'
    return text


class PackageTest(unittest.TestCase):
    def setUp(self) -> None:
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name).resolve()
        self.app = self.root / 'qbittorrent.app'
        self.binary = self.app / 'Contents/MacOS/qbittorrent'
        self.binary.parent.mkdir(parents=True)
        self.binary.write_bytes(bytes.fromhex('cffaedfe'))
        with (self.app / 'Contents/Info.plist').open('wb') as file:
            plistlib.dump({'CFBundleExecutable': 'qbittorrent',
                          'LSMinimumSystemVersion': '.0'}, file)

    def tool(self, *args: str) -> str:
        return 'arm64' if args[0].endswith('/lipo') else commands()

    def test_system_libraries_do_not_need_files_outside_dyld_cache(self) -> None:
        with patch.object(package, 'run', side_effect=self.tool):
            self.assertEqual(package.audit(self.app)['minimum_macos'], '13.0')

    def test_external_dependencies_are_rejected(self) -> None:
        for dependency in ('/opt/homebrew/lib/libbad.dylib',
                           '/usr/local/lib/libbad.dylib', '/Users/runner/libbad.dylib',
                           '/usr/lib/../../opt/homebrew/lib/libbad.dylib'):
            with self.subTest(dependency=dependency):
                def tool(*args: str, dependency: str = dependency) -> str:
                    return 'arm64' if args[0].endswith('/lipo') else commands(dependency)

                with patch.object(package, 'run', side_effect=tool), \
                        self.assertRaisesRegex(ValueError, 'Unresolved bundled dependency'):
                    package.audit(self.app)

    def test_external_rpath_is_rejected_even_if_unused(self) -> None:
        def tool(*args: str) -> str:
            return 'arm64' if args[0].endswith('/lipo') else commands(rpath='/Users/runner/Qt/lib')

        with patch.object(package, 'run', side_effect=tool), \
                self.assertRaisesRegex(ValueError, 'External runtime search path'):
            package.audit(self.app)

    def test_missing_rpath_dependency_is_rejected(self) -> None:
        def tool(*args: str) -> str:
            return ('arm64' if args[0].endswith('/lipo')
                    else commands('@rpath/libmissing.dylib', '@executable_path/../Frameworks'))

        with patch.object(package, 'run', side_effect=tool), \
                self.assertRaisesRegex(ValueError, 'Unresolved bundled dependency'):
            package.audit(self.app)

    def test_bundled_rpath_dependency_and_highest_minimum_are_detected(self) -> None:
        library = self.app / 'Contents/Frameworks/libbundled.dylib'
        library.parent.mkdir()
        library.write_bytes(bytes.fromhex('cffaedfe'))

        def tool(*args: str) -> str:
            if args[0].endswith('/lipo'):
                return 'arm64'
            if args[-1] == str(self.binary):
                return commands('@rpath/libbundled.dylib', '@executable_path/../Frameworks')
            return commands(minimum='14.5')

        with patch.object(package, 'run', side_effect=tool):
            self.assertEqual(package.audit(self.app)['minimum_macos'], '14.5')

    def test_symlink_escape_is_rejected(self) -> None:
        (self.app / 'Contents/escape').symlink_to(self.root)
        with self.assertRaisesRegex(ValueError, 'Path escapes app bundle'):
            package.bundle_files(self.app)

    def test_missing_architecture_is_rejected(self) -> None:
        library = self.app / 'Contents/library.dylib'
        library.write_bytes(bytes.fromhex('cffaedfe'))

        def tool(*args: str) -> str:
            if args[0].endswith('/lipo'):
                return 'arm64 x86_64' if args[-1] == str(self.binary) else 'arm64'
            return commands()

        with patch.object(package, 'run', side_effect=tool), \
                self.assertRaisesRegex(ValueError, 'Missing required architecture'):
            package.audit(self.app)

    def test_executable_cannot_escape_bundle(self) -> None:
        with (self.app / 'Contents/Info.plist').open('wb') as file:
            plistlib.dump({'CFBundleExecutable': '../../outside'}, file)
        with self.assertRaisesRegex(ValueError, 'Invalid bundle executable'):
            package.executable_path(self.app)

    def test_non_macos_binary_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, 'Non-macOS'):
            package.load_commands(commands().replace('platform 1', 'platform 2'))

    def test_prepare_signs_nested_code_before_app_and_repairs_minimum(self) -> None:
        framework = self.app / 'Contents/Frameworks/QtCore.framework'
        framework.mkdir(parents=True)
        library = framework / 'QtCore'
        library.write_bytes(bytes.fromhex('cffaedfe'))
        manifest = self.root / 'manifest.json'
        env = {'GITHUB_SHA': 'a' * 40, 'QBT_QT_VERSION': '6.10.3',
               'QBT_LIBTORRENT_VERSION': '2.1.2', 'QBT_BOOST_VERSION': '1.91.0'}

        def tool(*args: str) -> str:
            if args[0].endswith('/lipo'):
                return 'arm64'
            if args[-1] == '-v':
                return 'qBittorrent v5.3.0rc1'
            return commands()

        with patch.dict(os.environ, env, clear=True), \
                patch.object(package, 'run', side_effect=tool) as run:
            package.prepare(self.app, manifest)
        signing = [call.args[-1] for call in run.call_args_list
                   if call.args[0].endswith('/codesign') and '--sign' in call.args]
        self.assertLess(signing.index(str(library)), signing.index(str(framework)))
        self.assertEqual(signing[-1], str(self.app))
        metadata = json.loads(manifest.read_text())
        self.assertEqual(metadata['minimum_macos'], '13.0')
        self.assertIn('arm64-min-13.0', metadata['artifact_name'])
        self.assertIn('a' * 40, metadata['artifact_name'])

    def test_smoke_timeout_terminates_then_kills_only_its_process(self) -> None:
        process = Mock()
        process.poll.return_value = None
        process.wait.side_effect = [subprocess.TimeoutExpired('qbittorrent', 10), 0]
        with patch.object(subprocess, 'Popen', return_value=process), \
                patch.object(package, 'run', return_value='qBittorrent v5.3.0rc1'), \
                patch('time.monotonic', side_effect=[0, 31]), self.assertRaises(TimeoutError):
            package.smoke(self.app, self.root, '5.3.0rc1')
        process.terminate.assert_called_once()
        process.kill.assert_called_once()

    def test_smoke_uses_isolated_settings_and_shuts_down_through_api(self) -> None:
        process = Mock()
        process.poll.side_effect = [None, 0]
        process.wait.return_value = 0
        preferences = {'save_path': str(self.root / 'profile/qBittorrent/downloads'),
                       'dht': False, 'pex': False, 'lsd': False, 'upnp': False,
                       'rss_auto_downloading_enabled': False,
                       'current_network_interface': 'lo0',
                       'current_interface_address': '127.0.0.1',
                       'web_ui_address': '127.0.0.1'}
        versions = {'qt': '6.10.3', 'libtorrent': '2.1.2.0', 'boost': '1.91.0'}
        responses = ['v5.3.0rc1', json.dumps(preferences), '[]', json.dumps(versions), '']
        with patch.object(subprocess, 'Popen', return_value=process), \
                patch.object(package, 'run', return_value='qBittorrent v5.3.0rc1'), \
                patch.object(package, 'api_request', side_effect=responses) as api:
            self.assertEqual(package.smoke(self.app, self.root, '5.3.0rc1'), versions)
        self.assertEqual(api.call_args.args[1:], ('app/shutdown', 'POST'))
        process.terminate.assert_not_called()
        settings = (self.root / 'profile/qBittorrent/config/qBittorrent.ini').read_text()
        self.assertIn('Session\\DHTEnabled=false', settings)
        self.assertNotIn('Session\\\\DHTEnabled', settings)
        self.assertIn('Password_PBKDF2="@ByteArray(', settings)

    def test_dmg_is_detached_when_copy_fails(self) -> None:
        manifest = self.root / 'manifest.json'
        manifest.write_text('{}')

        def tool(*args: str) -> str:
            if 'attach' in args:
                (Path(args[args.index('-mountpoint') + 1]) / 'qbittorrent.app').mkdir(parents=True)
            if args[0].endswith('/ditto'):
                raise subprocess.CalledProcessError(1, args)
            return ''

        with patch.object(package, 'run', side_effect=tool) as run, \
                self.assertRaises(subprocess.CalledProcessError):
            package.verify_dmg(self.root / 'app.dmg', manifest)
        self.assertTrue(any('detach' in call.args for call in run.call_args_list))

    def test_verified_dmg_gets_checksum_and_runtime_dependency_versions(self) -> None:
        dmg = self.root / 'app.dmg'
        dmg.write_bytes(b'abc')
        manifest = self.root / 'manifest.json'
        metadata = {'architectures': ['arm64'], 'minimum_macos': '13.0', 'version': '5.3.0rc1',
                    'dependency_versions': {'qt': '6.10.3', 'libtorrent': '2.1.2', 'boost': '1.91.0'}}
        manifest.write_text(json.dumps(metadata))
        with (self.app / 'Contents/Info.plist').open('wb') as file:
            plistlib.dump({'LSMinimumSystemVersion': '13.0'}, file)

        def tool(*args: str) -> str:
            if 'attach' in args:
                mount = Path(args[args.index('-mountpoint') + 1])
                shutil.copytree(self.app, mount / self.app.name)
            elif args[0].endswith('/ditto'):
                shutil.copytree(args[1], args[2])
            return ''

        versions = {'qt': '6.10.3', 'libtorrent': '2.1.2.0', 'boost': '1.91.0'}
        with patch.object(package, 'run', side_effect=tool), \
                patch.object(package, 'audit', return_value=metadata), \
                patch.object(package, 'smoke', return_value=versions):
            package.verify_dmg(dmg, manifest)
        checksum = 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'
        self.assertEqual(dmg.with_suffix('.dmg.sha256').read_text(), f'{checksum}  app.dmg\n')
        saved = json.loads(manifest.read_text())
        self.assertEqual(saved['dmg_sha256'], checksum)
        self.assertEqual(saved['build_versions'], versions)

    def test_native_tool_failure_reports_stderr(self) -> None:
        error = subprocess.CalledProcessError(1, '/usr/bin/codesign', stderr='invalid signature\n')
        with patch.object(subprocess, 'run', side_effect=error), \
                self.assertRaisesRegex(RuntimeError, 'codesign failed: invalid signature'):
            package.run('/usr/bin/codesign', '--verify', str(self.app))


if __name__ == '__main__':
    unittest.main()
