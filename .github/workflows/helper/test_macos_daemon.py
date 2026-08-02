#!/usr/bin/env python3

import os
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
from pathlib import Path


TIMEOUT = 30


def wait_until(predicate, description, timeout=TIMEOUT):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.1)
    raise AssertionError(f"Timed out waiting for {description}")


def reserve_ports(count):
    sockets = []
    try:
        for _ in range(count):
            sock = socket.socket()
            sock.bind(("127.0.0.1", 0))
            sockets.append(sock)
        return [sock.getsockname()[1] for sock in sockets]
    finally:
        for sock in sockets:
            sock.close()


def profile_processes(binary, profile):
    result = subprocess.run(
        ["ps", "-axo", "pid=,command="],
        check=True,
        capture_output=True,
        text=True,
    )
    profile_arg = f"--profile={profile}"
    processes = []
    for line in result.stdout.splitlines():
        fields = line.strip().split(maxsplit=1)
        if len(fields) == 2 and str(binary) in fields[1] and profile_arg in fields[1]:
            processes.append(int(fields[0]))
    return processes


def process_exists(pid):
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False


def webui_status(port):
    try:
        with urllib.request.urlopen(f"http://127.0.0.1:{port}/", timeout=0.5) as response:
            return response.status
    except urllib.error.HTTPError as error:
        return error.code
    except (OSError, urllib.error.URLError):
        return None


def file_descriptors(pid):
    result = subprocess.run(
        ["lsof", "-a", "-p", str(pid), "-d", "0,1,2", "-Ffn"],
        check=True,
        capture_output=True,
        text=True,
    )
    descriptors = {}
    current_fd = None
    for line in result.stdout.splitlines():
        if line.startswith("f"):
            current_fd = int(line[1:])
        elif line.startswith("n") and current_fd is not None:
            descriptors[current_fd] = line[1:]
    return descriptors


def stop_processes(pids):
    for pid in pids:
        try:
            os.kill(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass

    deadline = time.monotonic() + 5
    while any(process_exists(pid) for pid in pids) and time.monotonic() < deadline:
        time.sleep(0.1)

    for pid in pids:
        if process_exists(pid):
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass


def run_mode(binary, mode):
    launcher = None
    with tempfile.TemporaryDirectory(prefix=f"qbt-daemon-{mode}-") as temp_dir:
        profile = Path(temp_dir)
        webui_port, torrent_port = reserve_ports(2)
        args = [
            str(binary),
            "--confirm-legal-notice",
            f"--profile={profile}",
            f"--webui-port={webui_port}",
            f"--torrenting-port={torrent_port}",
        ]
        env = os.environ.copy()
        env.pop("QBT_DAEMON", None)
        env.pop("QBT_DAEMONIZED", None)
        env["LANG"] = "C"
        env["LC_ALL"] = "C"
        if mode == "cli":
            args.append("-d")
        else:
            env["QBT_DAEMON"] = "1"

        try:
            launcher = subprocess.Popen(
                args,
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            stdout, stderr = launcher.communicate(timeout=5)
            assert launcher.returncode == 0, (
                f"{mode}: launcher exited with {launcher.returncode}\n"
                f"stdout:\n{stdout}\nstderr:\n{stderr}"
            )

            wait_until(
                lambda: len(profile_processes(binary, profile)) == 1,
                f"{mode} daemon child",
                timeout=5,
            )
            processes = profile_processes(binary, profile)
            assert len(processes) == 1, f"{mode}: expected one child, found {processes}"
            child_pid = processes[0]
            assert child_pid != launcher.pid, f"{mode}: daemon did not start a child process"
            assert os.getsid(child_pid) == child_pid, f"{mode}: child did not create a new session"

            wait_until(
                lambda: webui_status(webui_port) in (200, 401),
                f"{mode} WebUI",
            )
            descriptors = file_descriptors(child_pid)
            assert descriptors == {0: "/dev/null", 1: "/dev/null", 2: "/dev/null"}, (
                f"{mode}: unexpected standard descriptors: {descriptors}"
            )

            duplicate = subprocess.run(
                args,
                env=env,
                capture_output=True,
                text=True,
                timeout=5,
            )
            assert duplicate.returncode != 0, f"{mode}: duplicate daemon unexpectedly succeeded"
            assert "already running" in duplicate.stderr, (
                f"{mode}: duplicate did not hit the single-instance check:\n{duplicate.stderr}"
            )
            assert profile_processes(binary, profile) == [child_pid], (
                f"{mode}: duplicate daemon created another process"
            )

            log_path = profile / "qBittorrent" / "data" / "logs" / "qbittorrent.log"
            wait_until(log_path.exists, f"{mode} log file")
            log_size = log_path.stat().st_size

            os.kill(child_pid, signal.SIGTERM)
            wait_until(lambda: not process_exists(child_pid), f"{mode} child termination")
            wait_until(
                lambda: not profile_processes(binary, profile),
                f"{mode} profile process cleanup",
            )
            wait_until(lambda: webui_status(webui_port) is None, f"{mode} WebUI shutdown")

            log_tail = log_path.read_text(errors="replace")[log_size:]
            assert "qBittorrent termination initiated" in log_tail, (
                f"{mode}: clean termination missing from log tail:\n{log_tail}"
            )
            print(f"PASS: {mode} daemon mode")
        finally:
            if launcher is not None and launcher.poll() is None:
                launcher.kill()
                launcher.wait()
            stop_processes(profile_processes(binary, profile))


def main():
    if len(sys.argv) != 2:
        raise SystemExit(f"Usage: {sys.argv[0]} /path/to/qbittorrent-nox")

    binary = Path(sys.argv[1]).resolve()
    if not binary.is_file():
        raise SystemExit(f"Executable not found: {binary}")

    run_mode(binary, "cli")
    run_mode(binary, "env")


if __name__ == "__main__":
    main()
