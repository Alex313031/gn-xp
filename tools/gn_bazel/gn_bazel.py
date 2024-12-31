#!/usr/bin/env python3

import os
import sys

# This should, unlike bash launchers (in theory) allow it to work on non-unix environments.
WANT_INTERPRETER = os.path.join(
    os.path.dirname(os.path.realpath(__file__)),
    "venv/bin/python3"
)
if sys.executable != WANT_INTERPRETER:
    # Relaunch in the venv
    os.execvp(WANT_INTERPRETER, [WANT_INTERPRETER, *sys.argv])

import datetime
import pathlib
import shutil
import subprocess
import tempfile
import time

import daemon
from lockfile.pidlockfile import PIDLockFile
from watchdog.events import FileSystemEventHandler
from watchdog.observers import Observer

def fail(*args: str):
    print(*args, file=sys.stderr)
    exit(1)

# Use environment variables instead of args so we don't interfere with bazel's args.
PORT = int(os.environ.get("GN_BAZEL_DAEMON_PORT", 36729))
DAEMON_ONLY = bool(int(os.environ.get("GN_BAZEL_DAEMON_ONLY", "0")))
GN_BINARY = os.environ.get("GN_BINARY", shutil.which("gn"))
GN_OUT = os.environ.get("GN_OUT", "out/bazel")
ARGS_GN = pathlib.Path(GN_OUT, "args.gn")

BAZEL_REAL = os.environ.get("BAZEL_REAL", None)
# We expect BAZEL_REAL to be set by bazel.
if not BAZEL_REAL:
    fail("BAZEL_REAL must be set")

def get_bazel_dir(d: pathlib.Path) -> pathlib.Path:
    while not (d / "MODULE.bazel").exists():
        if d.parent == d:
            fail("Unable to find MODULE.bazel - are you in a bazel directory?")
        d = d.parent
    return d

def get_mtime(p: pathlib.Path) -> int | None:
    try:
        return p.stat().st_mtime
    except FileNotFoundError:
        return None


class InvalidateEventHandler(FileSystemEventHandler):
    def __init__(self, bazel_dir: pathlib.Path, invalidate_path: pathlib.Path):
        self.bazel_dir = bazel_dir
        self.invalidate_path = invalidate_path
        self.gn_binary = pathlib.Path(GN_BINARY).resolve()
        self.args_gn = pathlib.Path(bazel_dir, ARGS_GN).resolve()

    def handle(self, event):
        path = pathlib.Path(event.src_path)
        if path == self.gn_binary or path.name == "BUILD.gn" or path.name == "/BUILD.gni" or path == self.args_gn:
            print(f"Invalidating due to {event}")
            self.invalidate_path.touch()

    on_created = handle
    on_moved = handle
    on_modified = handle
    # TODO: we don't handle deletions very well in the gn gen itself
    on_deleted = handle

    def watch(self):
        observer = Observer()
        observer.schedule(self, self.bazel_dir, recursive=True)
        observer.schedule(self, self.gn_binary)
        observer.schedule(self, self.args_gn)
        observer.start()

        try:
            time.sleep(99999999)
        finally:
            observer.stop()
            observer.join()


# Do this before daemonizing, so the daemon has the invariant of starting in the correct directory.
bazel_dir = get_bazel_dir(pathlib.Path()).resolve()
modified_path = bazel_dir / ".gn_modified"
updated_path = bazel_dir / ".bazel_updated"
# A string that, when changed, ensures we must rerun.
config = str(pathlib.Path(bazel_dir, GN_OUT)).encode('utf-8')

if DAEMON_ONLY:
    InvalidateEventHandler(bazel_dir, modified_path).watch() 
elif os.fork() == 0:
    # One daemon per workspace.
    with daemon.DaemonContext(pidfile = PIDLockFile(bazel_dir / ".gn_bazel_daemon_pid")):
        InvalidateEventHandler(bazel_dir, modified_path).watch() 
else:
    modified_time = get_mtime(modified_path) or 0
    updated_time = get_mtime(updated_path) or 0
    invalid = modified_time > updated_time
    if invalid:
        print("Files affecting the build (eg. BUILD.gn, BUILD.gni) have been updated.")
    else:
        try:
            last_config = updated_path.read_bytes()
            if last_config != config:
                print("Configuration has changed since last bazel invocation")
                invalid = True
        except FileNotFoundError:
            invalid = True

    if invalid:
        # Make sure the timestamps fit.
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(config)
            f.close()
            args = [GN_BINARY, "gen", GN_OUT]
            print("Running", " ".join(args))
            ps = subprocess.run(args)
            if ps.returncode != 0:
                fail("gn gen failed with exit status", ps.returncode)
            shutil.move(f.name, updated_path)
            print("BUILD.bazel files regenerated")
    os.execvp(BAZEL_REAL, [BAZEL_REAL, *sys.argv[1:]])


