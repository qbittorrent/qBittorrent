import argparse
import base64
import hashlib
import http.client
import json
import os
import plistlib
import re
import secrets
import socket
import subprocess  # nosec B404
import tempfile
import time
from pathlib import Path
from typing import Any

MACH_O_MAGIC = {bytes.fromhex(value) for value in (
    'feedface', 'cefaedfe', 'feedfacf', 'cffaedfe', 'cafebabe', 'bebafeca',
    'cafebabf', 'bfbafeca')}
SYSTEM_PATHS = ('/System/Library/', '/usr/lib/')
DYLIB_COMMANDS = {'LC_LOAD_DYLIB', 'LC_LOAD_WEAK_DYLIB', 'LC_REEXPORT_DYLIB',
                  'LC_LOAD_UPWARD_DYLIB', 'LC_LAZY_LOAD_DYLIB'}


def run(*args: str) -> str:
    # Only native tools and the freshly built app are invoked, without a shell.
    try:
        return subprocess.run(args, check=True, capture_output=True, text=True,  # nosec B603
                              timeout=120).stdout.strip()
    except subprocess.CalledProcessError as error:
        raise RuntimeError(f'{args[0]} failed: {error.stderr.strip()}') from error


def load_commands(text: str) -> tuple[list[str], list[str], list[str]]:
    dependencies: list[str] = []
    rpaths: list[str] = []
    minimums: list[str] = []
    for block in re.split(r'Load command \d+\n', text)[1:]:
        command = re.findall(r'^\s*cmd (\S+)', block, re.MULTILINE)[0]
        if command in DYLIB_COMMANDS:
            dependencies.append(re.findall(r'\sname (.*?) \(offset ', block)[0])
        elif command == 'LC_RPATH':
            rpaths.append(re.findall(r'\spath (.*?) \(offset ', block)[0])
        elif command == 'LC_BUILD_VERSION':
            if re.search(r'^\s*platform 1\s*$', block, re.MULTILINE) is None:
                raise ValueError('Non-macOS Mach-O binary')
            minimums.append(re.findall(r'\sminos ([\d.]+)', block)[0])
        elif command == 'LC_VERSION_MIN_MACOSX':
            minimums.append(re.findall(r'\sversion ([\d.]+)', block)[0])
    if not minimums:
        raise ValueError('Missing Mach-O minimum macOS version')
    return dependencies, rpaths, minimums


def version_tuple(value: str) -> tuple[int, ...]:
    if re.fullmatch(r'\d+(?:\.\d+){0,2}', value) is None:
        raise ValueError(f'Invalid macOS version: {value}')
    return tuple((list(map(int, value.split('.'))) + [0, 0])[:3])


def bundle_files(app: Path) -> list[Path]:
    binaries: list[Path] = []
    for path in app.rglob('*'):
        if not path.resolve().is_relative_to(app):
            raise ValueError(f'Path escapes app bundle: {path}')
        if path.is_file() and not path.is_symlink():
            with path.open('rb') as file:
                if file.read(4) in MACH_O_MAGIC:
                    binaries.append(path)
    return binaries


def executable_path(app: Path) -> Path:
    with (app / 'Contents/Info.plist').open('rb') as file:
        name: str = plistlib.load(file)['CFBundleExecutable']
    if Path(name).name != name:
        raise ValueError(f'Invalid bundle executable: {name}')
    return app / 'Contents/MacOS' / name


def expand_path(value: str, binary: Path, executable: Path) -> Path:
    for token, root in (('@loader_path', binary.parent),
                        ('@executable_path', executable.parent)):
        if value == token or value.startswith(token + '/'):
            return (root / value[len(token):].lstrip('/')).resolve()
    if value.startswith('/'):
        return Path(value).resolve()
    raise ValueError(f'Unsupported runtime path: {value}')


def audit(app: Path) -> dict[str, Any]:
    app = app.resolve()
    executable = executable_path(app)
    binaries = bundle_files(app)
    info = {path: {arch: load_commands(run('/usr/bin/otool', '-arch', arch, '-l', str(path)))
                   for arch in run('/usr/bin/lipo', '-archs', str(path)).split()}
            for path in binaries}
    if executable not in info:
        raise ValueError('Bundle executable is not Mach-O')
    architectures = set(info[executable])
    minimums: list[str] = []
    for binary, slices in info.items():
        if not architectures.issubset(slices):
            raise ValueError(f'Missing required architecture in {binary}')
        for arch in architectures:
            dependencies, rpaths, versions = slices[arch]
            minimums.extend(versions)
            roots: list[Path] = []
            # shortcut: binary and executable rpaths suffice here; extend for other loader chains.
            for owner, values in ((binary, rpaths), (executable, info[executable][arch][1])):
                for value in values:
                    root = expand_path(value, owner, executable)
                    if not root.is_relative_to(app):
                        raise ValueError(f'External runtime search path in {binary}: {value}')
                    roots.append(root)
            for dependency in dependencies:
                if os.path.normpath(dependency).startswith(SYSTEM_PATHS):
                    continue
                if dependency.startswith('@rpath/'):
                    candidates = [(root / dependency[len('@rpath/'):]).resolve() for root in roots]
                else:
                    candidates = [expand_path(dependency, binary, executable)]
                if not any(path.is_relative_to(app) and path in info and arch in info[path]
                           for path in candidates):
                    raise ValueError(f'Unresolved bundled dependency in {binary}: {dependency}')
    minimum = max(minimums, key=version_tuple)
    return {'architectures': sorted(architectures), 'minimum_macos': minimum,
            'mach_o_files': len(binaries)}


def prepare(app: Path, manifest: Path) -> None:
    app = app.resolve()
    binaries = bundle_files(app)
    for binary in binaries:
        rpaths: set[str] = set()
        for arch in run('/usr/bin/lipo', '-archs', str(binary)).split():
            rpaths.update(load_commands(run('/usr/bin/otool', '-arch', arch, '-l', str(binary)))[1])
        for value in sorted(rpaths):
            if not expand_path(value, binary, executable_path(app)).is_relative_to(app):
                run('/usr/bin/install_name_tool', '-delete_rpath', value, str(binary))
    metadata = audit(app)
    plist_path = app / 'Contents/Info.plist'
    with plist_path.open('rb') as file:
        plist = plistlib.load(file)
    declared = plist.get('LSMinimumSystemVersion', '')
    if re.fullmatch(r'\d+(?:\.\d+){0,2}', declared):
        metadata['minimum_macos'] = max(declared, metadata['minimum_macos'], key=version_tuple)
    plist['LSMinimumSystemVersion'] = metadata['minimum_macos']
    with plist_path.open('wb') as file:
        plistlib.dump(plist, file, sort_keys=False)
    run('/usr/bin/xattr', '-cr', str(app))
    nested = binaries + [path for path in app.rglob('*') if path.is_dir()
                         and not path.is_symlink() and path.suffix in ('.framework', '.app', '.xpc')]
    for path in sorted(nested, key=lambda value: len(value.parts), reverse=True) + [app]:
        run('/usr/bin/codesign', '--force', '--sign', '-', str(path))
    run('/usr/bin/codesign', '--verify', '--deep', '--strict', str(app))
    with tempfile.TemporaryDirectory(prefix='qbt-version-') as profile:
        metadata['version'] = run(str(executable_path(app)), f'--profile={profile}',
                                  '-v').removeprefix('qBittorrent v')
    metadata['commit'] = os.environ['GITHUB_SHA']
    metadata['dependency_versions'] = {key: os.environ[f'QBT_{key.upper()}_VERSION']
                                       for key in ('qt', 'libtorrent', 'boost')}
    name = (f"{app.stem}-{metadata['version']}-macOS-{'_'.join(metadata['architectures'])}"
            f"-min-{metadata['minimum_macos']}-Qt-{metadata['dependency_versions']['qt']}"
            f"-libtorrent-{metadata['dependency_versions']['libtorrent']}"
            f"-boost-{metadata['dependency_versions']['boost']}-{metadata['commit']}")
    if re.fullmatch(r'[A-Za-z0-9_.-]+', name) is None:
        raise ValueError(f'Invalid artifact name: {name}')
    metadata['artifact_name'] = name
    manifest.write_text(json.dumps(metadata, indent=2) + '\n')
    if 'GITHUB_OUTPUT' in os.environ:
        with open(os.environ['GITHUB_OUTPUT'], 'a') as file:
            file.write(f'artifact_name={name}\n')
    print(name)


def api_request(port: int, path: str, method: str = 'GET') -> str:
    connection = http.client.HTTPConnection('127.0.0.1', port, timeout=2)
    try:
        connection.request(method, '/api/v2/' + path,
                           headers={'Referer': f'http://127.0.0.1:{port}/'})
        response = connection.getresponse()
        body = response.read().decode()
        if response.status != 200:
            raise ValueError(f'Smoke API returned HTTP {response.status}: {path}')
        return body
    finally:
        connection.close()


def smoke(app: Path, root: Path, expected_version: str) -> dict[str, str]:
    profile = root / 'profile'
    config = profile / 'qBittorrent/config'
    config.mkdir(parents=True)
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        port = listener.getsockname()[1]
    downloads = profile / 'qBittorrent/downloads'
    run(str(executable_path(app)), f'--profile={profile}', '-v')
    salt = secrets.token_bytes(16)
    password_hash = hashlib.pbkdf2_hmac('sha512', secrets.token_bytes(32), salt, 100000)
    secret = base64.b64encode(salt).decode() + ':' + base64.b64encode(password_hash).decode()
    settings = (
        '[BitTorrent]\nSession\\DHTEnabled=false\nSession\\PeXEnabled=false\n'
        'Session\\LSDEnabled=false\nSession\\Interface=lo0\n'
        'Session\\InterfaceAddress=127.0.0.1\nSession\\Port=0\n'
        f'Session\\DefaultSavePath={downloads}\nSession\\ShutdownTimeout=5\n'
        '[Network]\nPortForwardingEnabled=false\n'
        '[RSS]\nAutoDownloader\\EnableProcessing=false\nSession\\EnableProcessing=false\n'
        '[WebUI]\nEnabled=true\nAddress=127.0.0.1\nLocalHostAuth=false\n'
        f'Username=ci-smoke\nPassword_PBKDF2="@ByteArray({secret})"\n'
        f'Port={port}\n')
    with (config / 'qBittorrent.ini').open('a') as file:
        file.write('\n' + settings)
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('DYLD_', 'QT_', 'QBT_'))}
    with (root / 'smoke.log').open('w+') as log:
        process = subprocess.Popen([str(executable_path(app)), f'--profile={profile}',  # nosec B603
                                    '--confirm-legal-notice'], env=env, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise ValueError(f'Packaged app exited before startup: {process.returncode}')
                try:
                    version = api_request(port, 'app/version')
                    break
                except OSError:
                    time.sleep(0.2)
            else:
                raise TimeoutError('Packaged app did not start its WebUI within 30 seconds')
            preferences = json.loads(api_request(port, 'app/preferences'))
            if Path(preferences['save_path']) != downloads:
                raise ValueError('Smoke API does not belong to the disposable profile')
            if any(preferences[key] for key in ('dht', 'pex', 'lsd', 'upnp',
                                                'rss_auto_downloading_enabled')):
                raise ValueError('Smoke profile enabled peer discovery or port forwarding')
            if (preferences['current_network_interface'] != 'lo0'
                    or preferences['current_interface_address'] != '127.0.0.1'
                    or preferences['web_ui_address'] != '127.0.0.1'):
                raise ValueError('Smoke profile is not bound to loopback')
            if version.removeprefix('v') != expected_version:
                raise ValueError(f'Packaged app version mismatch: {version}')
            if json.loads(api_request(port, 'torrents/info')):
                raise ValueError('Disposable profile contains torrents')
            versions: dict[str, str] = json.loads(api_request(port, 'app/buildInfo'))
            api_request(port, 'app/shutdown', 'POST')
            if process.wait(timeout=20) != 0:
                raise ValueError(f'Packaged app shutdown failed: {process.returncode}')
            return versions
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=10)


def verify_dmg(dmg: Path, manifest: Path) -> None:
    metadata = json.loads(manifest.read_text())
    run('/usr/bin/hdiutil', 'verify', str(dmg))
    with tempfile.TemporaryDirectory(prefix='qbt-dmg-') as directory:
        root = Path(directory)
        mount = root / 'mount'
        run('/usr/bin/hdiutil', 'attach', '-readonly', '-nobrowse', '-mountpoint',
            str(mount), str(dmg))
        try:
            apps = list(mount.glob('*.app'))
            if len(apps) != 1 or apps[0].is_symlink():
                raise ValueError('DMG must contain exactly one app bundle')
            app = root / apps[0].name
            run('/usr/bin/ditto', str(apps[0]), str(app))
        finally:
            run('/usr/bin/hdiutil', 'detach', str(mount))
        run('/usr/bin/codesign', '--verify', '--deep', '--strict', str(app))
        actual = audit(app)
        if actual['architectures'] != metadata['architectures']:
            raise ValueError('DMG architecture differs from prepared app')
        with (app / 'Contents/Info.plist').open('rb') as file:
            minimum = plistlib.load(file)['LSMinimumSystemVersion']
        if (minimum != metadata['minimum_macos']
                or version_tuple(minimum) < version_tuple(actual['minimum_macos'])):
            raise ValueError('DMG minimum macOS version differs from prepared app')
        metadata['build_versions'] = smoke(app, root, metadata['version'])
        for key, expected in metadata['dependency_versions'].items():
            built_version = metadata['build_versions'][key]
            if tuple(map(int, built_version.split('.')[:3])) != tuple(map(int, expected.split('.'))):
                raise ValueError(f'DMG {key} version mismatch: {built_version} != {expected}')
    sha256 = hashlib.sha256()
    with dmg.open('rb') as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b''):
            sha256.update(chunk)
    digest = sha256.hexdigest()
    metadata['dmg_sha256'] = digest
    manifest.write_text(json.dumps(metadata, indent=2) + '\n')
    dmg.with_suffix('.dmg.sha256').write_text(f'{digest}  {dmg.name}\n')
    print(f'Verified {dmg.name}: {digest}')


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('command', choices=('prepare', 'verify'))
    parser.add_argument('path', type=Path)
    parser.add_argument('manifest', type=Path)
    args = parser.parse_args()
    if args.command == 'prepare':
        prepare(args.path, args.manifest)
    else:
        verify_dmg(args.path, args.manifest)


if __name__ == '__main__':
    main()
