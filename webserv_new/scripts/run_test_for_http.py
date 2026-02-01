#!/usr/bin/env python3
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_FILE = os.path.join(ROOT, "test_for_http.txt")

COMMAND_PREFIXES = (
    "./webserv",
    "./test_http",
    "curl ",
    "printf ",
    "python3 ",
    "nc ",
    "lsof ",
    "test ",
    "cp ",
    "sleep ",
    "HOST=",
    "PORT=",
    "BASE=",
)

IGNORE_PREFIXES = (
    "//",
    "#",
    "********",
    "success",
    "failed",
    "ATTENT",
)


def is_command_start(line):
    s = line.lstrip()
    if not s:
        return False
    for p in IGNORE_PREFIXES:
        if s.startswith(p):
            return False
    for p in COMMAND_PREFIXES:
        if s.startswith(p):
            return True
    if s.startswith("|"):
        return True
    return False


def main():
    path = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else DEFAULT_FILE
    dry_run = "--dry-run" in sys.argv

    if not os.path.exists(path):
        print("[ERR] file not found:", path)
        return 2

    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()

    cmds = []
    cur = []
    heredoc = None

    def flush_cur():
        nonlocal cur
        if cur:
            cmds.append("\n".join(cur).rstrip())
            cur = []

    i = 0
    while i < len(lines):
        line = lines[i].rstrip("\n")
        s = line.lstrip()

        if heredoc is not None:
            cur.append(line)
            if s == heredoc:
                heredoc = None
                flush_cur()
            i += 1
            continue

        if not is_command_start(line):
            flush_cur()
            i += 1
            continue

        m = re.search(r"<<\s*'?([A-Za-z0-9_]+)'?", line)
        if m:
            heredoc = m.group(1)
            cur.append(line)
            i += 1
            continue

        if cur and (cur[-1].rstrip().endswith("\\") or s.startswith("|")):
            cur.append(line)
        else:
            flush_cur()
            cur.append(line)
        i += 1

    flush_cur()

    print("[INFO] extracted commands:", len(cmds))
    if dry_run:
        for c in cmds:
            print("\n---\n" + c)
        return 0

    env = os.environ.copy()
    for c in cmds:
        cmd = c
        # Skip ./test_http if missing
        if cmd.strip().startswith("./test_http"):
            exe = os.path.join(ROOT, 'test_http')
            if not os.path.exists(exe):
                print("\n$ " + cmd)
                print("[WARN] ./test_http not found, skipping")
                continue
        # Add nc timeout to avoid hangs
        if ' nc ' in cmd or cmd.lstrip().startswith('nc ' ) or '| nc' in cmd:
            if '| nc' in cmd:
                cmd = cmd.replace('| nc', '| nc -w 5')
            else:
                cmd = cmd.replace('nc -v ', 'nc -w 5 -v ')
                if cmd.lstrip().startswith('nc '):
                    cmd = cmd.replace('nc ', 'nc -w 5 ', 1)
        # Add max time to curl if not present
        if cmd.lstrip().startswith('curl ') and '--max-time' not in cmd:
            parts = cmd.split(' ', 1)
            cmd = parts[0] + ' --max-time 5 ' + (parts[1] if len(parts) > 1 else '')
        print("\n$ " + cmd)
        try:
            p = subprocess.run(cmd, shell=True, cwd=ROOT, env=env, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            print(p.stdout)
            if p.returncode != 0:
                print("[WARN] non-zero exit:", p.returncode)
        except Exception as e:
            print("[ERR] failed to run command:", e)

    return 0


if __name__ == "__main__":
    sys.exit(main())
