#!/usr/bin/env python3

import json
import os
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
from urllib.error import URLError
from urllib.parse import urlencode
from urllib.request import Request, urlopen


TIMEOUT = 15


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def wait_until(predicate, message):
    deadline = time.monotonic() + TIMEOUT
    last_error = None
    while time.monotonic() < deadline:
        try:
            result = predicate()
            if result:
                return result
        except (OSError, URLError, ValueError) as error:
            last_error = error
        time.sleep(0.1)
    detail = f": {last_error}" if last_error else ""
    raise AssertionError(f"{message}{detail}")


class QBittorrent:
    def __init__(self, executable, profile):
        self.executable = executable
        self.profile = profile
        self.webui_port = free_port()
        self.torrenting_port = free_port()
        self.process = None

    @property
    def base_url(self):
        return f"http://127.0.0.1:{self.webui_port}/api/v2"

    def start(self):
        environment = os.environ.copy()
        environment.setdefault("QT_QPA_PLATFORM", "offscreen")
        if "QT_PLUGIN_PATH" not in environment:
            qmake = shutil.which("qmake")
            if qmake:
                plugin_path = subprocess.check_output(
                    [qmake, "-query", "QT_INSTALL_PLUGINS"], text=True).strip()
                if plugin_path:
                    environment["QT_PLUGIN_PATH"] = plugin_path

        self.process = subprocess.Popen(
            [
                self.executable,
                f"--profile={self.profile}",
                f"--webui-port={self.webui_port}",
                f"--torrenting-port={self.torrenting_port}",
                "--confirm-legal-notice",
                "--no-splash",
            ],
            env=environment,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        def is_ready():
            if self.process.poll() is not None:
                raise AssertionError(
                    f"qBittorrent exited early with code {self.process.returncode}")
            return self.get_text("/app/version")

        wait_until(is_ready, "WebUI did not become ready")

    def stop(self):
        if not self.process:
            return
        if self.process.poll() is None:
            self.process.send_signal(signal.SIGINT)
            try:
                self.process.wait(timeout=TIMEOUT)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=5)
                raise AssertionError("qBittorrent did not stop after SIGINT")
        if self.process.returncode != 0:
            raise AssertionError(
                f"qBittorrent exited with code {self.process.returncode}")
        self.process = None

    def request(self, path, data=None):
        encoded = None
        if data is not None:
            encoded = urlencode(data).encode()
        request = Request(
            self.base_url + path,
            data=encoded,
            headers={"Content-Type": "application/x-www-form-urlencoded"},
        )
        with urlopen(request, timeout=5) as response:
            if response.status != 200:
                raise AssertionError(
                    f"{path} returned HTTP {response.status}")
            return response.read()

    def get_text(self, path):
        return self.request(path).decode()

    def get_json(self, path):
        return json.loads(self.request(path))

    def post(self, path, data):
        self.request(path, data)


def assert_equal(actual, expected, message):
    if actual != expected:
        raise AssertionError(f"{message}: expected {expected!r}, got {actual!r}")


def set_preferences(app, **values):
    app.post("/app/setPreferences", {"json": json.dumps(values)})


def create_category(app, name, save_path, download_path):
    app.post(
        "/torrents/createCategory",
        {
            "category": name,
            "savePath": save_path,
            "downloadPathEnabled": "true",
            "downloadPath": download_path,
        },
    )


def add_magnet(app, info_hash, category):
    app.post(
        "/torrents/add",
        {
            "urls": f"magnet:?xt=urn:btih:{info_hash}&dn={category}",
            "category": category,
            "stopped": "true",
            "autoTMM": "true",
        },
    )


def torrent_info(app, info_hash):
    def find_torrent():
        torrents = app.get_json(
            f"/torrents/info?hashes={info_hash}")
        return torrents[0] if torrents else None

    return wait_until(find_torrent, f"torrent {info_hash} was not added")


def wait_for_auto_tmm(app, info_hash, expected):
    def has_expected_state():
        return torrent_info(app, info_hash)["auto_tmm"] is expected

    wait_until(
        has_expected_state,
        f"torrent {info_hash} auto_tmm did not become {expected}")


def main():
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} /path/to/qbittorrent")

    executable = os.path.abspath(sys.argv[1])
    home = Path.home()

    with tempfile.TemporaryDirectory(prefix="qbt-session-paths-") as temp_dir:
        profile = Path(temp_dir)
        config_dir = profile / "qBittorrent" / "config"
        config_dir.mkdir(parents=True)
        config_file = config_dir / "qBittorrent.ini"
        config_file.write_text(
            "[LegalNotice]\n"
            "Accepted=true\n\n"
            "[Preferences]\n"
            "WebUI\\Enabled=true\n"
            "WebUI\\LocalHostAuth=false\n"
            "WebUI\\Password_PBKDF2=test\n\n"
            "[BitTorrent]\n"
            "Session\\DefaultSavePath=~/read-save\n"
            "Session\\TempPath=~/read-temp\n"
            "Session\\TempPathEnabled=true\n",
            encoding="utf-8",
        )

        app = QBittorrent(executable, profile)
        try:
            app.start()

            preferences = app.get_json("/app/preferences")
            assert_equal(
                preferences["save_path"],
                str(home / "read-save"),
                "default save path was not expanded while loading settings",
            )
            assert_equal(
                preferences["temp_path"],
                str(home / "read-temp"),
                "temporary path was not expanded while loading settings",
            )

            set_preferences(
                app,
                save_path="~/set-save",
                temp_path="~/set-temp",
                temp_path_enabled=True,
                save_path_changed_tmm_enabled=False,
            )
            preferences = app.get_json("/app/preferences")
            assert_equal(
                preferences["save_path"],
                str(home / "set-save"),
                "save-path setter did not expand tilde",
            )
            assert_equal(
                preferences["temp_path"],
                str(home / "set-temp"),
                "temporary-path setter did not expand tilde",
            )

            categories = {
                "tilde": ("~/category-save", "~/category-temp"),
                "relative-save": ("relative-save", "~/relative-save-temp"),
                "relative-temp": ("~/relative-temp-save", "relative-temp"),
            }
            for name, (save_path, download_path) in categories.items():
                create_category(app, name, save_path, download_path)

            hashes = {
                "tilde": "1111111111111111111111111111111111111111",
                "relative-save": "2222222222222222222222222222222222222222",
                "relative-temp": "3333333333333333333333333333333333333333",
            }
            for category, info_hash in hashes.items():
                add_magnet(app, info_hash, category)

            tilde_torrent = torrent_info(app, hashes["tilde"])
            assert_equal(
                tilde_torrent["save_path"],
                str(home / "category-save"),
                "category save path was not expanded",
            )
            assert_equal(
                tilde_torrent["download_path"],
                str(home / "category-temp"),
                "category download path was not expanded",
            )

            for info_hash in hashes.values():
                wait_for_auto_tmm(app, info_hash, True)

            set_preferences(app, save_path="~/changed-save")
            wait_for_auto_tmm(app, hashes["relative-save"], False)
            wait_for_auto_tmm(app, hashes["tilde"], True)
            wait_for_auto_tmm(app, hashes["relative-temp"], True)

            set_preferences(app, temp_path="~/changed-temp")
            wait_for_auto_tmm(app, hashes["relative-temp"], False)
            wait_for_auto_tmm(app, hashes["tilde"], True)

            set_preferences(app, refresh_interval=1601)
            app.stop()

            categories_file = config_dir / "categories.json"
            persisted_categories = json.loads(
                categories_file.read_text(encoding="utf-8"))
            assert_equal(
                persisted_categories["tilde"]["save_path"],
                "~/category-save",
                "category save path was rewritten while persisting",
            )
            assert_equal(
                persisted_categories["tilde"]["download_path"],
                "~/category-temp",
                "category download path was rewritten while persisting",
            )

            app = QBittorrent(executable, profile)
            app.start()
            categories_after_restart = app.get_json("/torrents/categories")
            assert_equal(
                categories_after_restart["tilde"]["savePath"],
                "~/category-save",
                "category save path changed after restart",
            )
            assert_equal(
                categories_after_restart["tilde"]["download_path"],
                "~/category-temp",
                "category download path changed after restart",
            )

            tilde_after_restart = torrent_info(app, hashes["tilde"])
            assert_equal(
                tilde_after_restart["save_path"],
                str(home / "category-save"),
                "category save path was not expanded after restart",
            )
            assert_equal(
                tilde_after_restart["download_path"],
                str(home / "category-temp"),
                "category download path was not expanded after restart",
            )
            app.stop()
        finally:
            if app.process and app.process.poll() is None:
                app.process.kill()
                app.process.wait(timeout=5)


if __name__ == "__main__":
    main()
