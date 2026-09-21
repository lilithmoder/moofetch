#!/usr/bin/env python3
"""PTY smoke tests for moofetch animated ASCII logos.

Runs the built binary under a pseudo-terminal that emulates the two terminal queries
moofetch may send (DSR cursor position and terminal size), then asserts on the captured
byte stream.

Usage: python3 tests/pty_smoke.py [path/to/moofetch]
"""

import os
import pty
import re
import select
import signal
import struct
import subprocess
import sys
import termios
import time
import fcntl

MOOFETCH = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "build", "moofetch")
MOOFETCH = os.path.abspath(MOOFETCH)

STRUCTURE = "title:os:kernel:uptime:shell:terminal"
ARGS = [
    "--logo-type", "animation",
    "--logo", "spinner",
    "--structure", STRUCTURE,
    "--logo-animation-fps", "60",
]

failures = []


def check(condition, message):
    if condition:
        print(f"  ok   {message}")
    else:
        print(f"  FAIL {message}")
        failures.append(message)


def respond_to_queries(output, master):
    """Answer terminal queries like a real terminal would."""
    if b"\x1b[6n" in output:
        os.write(master, b"\x1b[24;1R")  # cursor position response
    if b"\x1b[18t" in output:
        os.write(master, b"\x1b[8;30;80t")  # text area size response


def make_controlling_terminal(slave):
    os.setsid()
    fcntl.ioctl(slave, termios.TIOCSCTTY, 0)


def run_pty(args, timeout=20, sigint_after_animation=False):
    master, slave = pty.openpty()
    # 30 rows x 80 columns
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 80, 0, 0))

    env = dict(os.environ)
    env["TERM"] = "xterm-256color"
    env.pop("NO_COLOR", None)

    proc = subprocess.Popen(
        [MOOFETCH] + args,
        stdin=slave, stdout=slave, stderr=slave,
        close_fds=True, env=env,
        pass_fds=[slave],
        preexec_fn=lambda: make_controlling_terminal(slave),
    )
    os.close(slave)

    output = b""
    answered = set()
    sent_sigint = False
    deadline = time.time() + timeout

    while time.time() < deadline:
        ready, _, _ = select.select([master], [], [], 0.1)
        if ready:
            try:
                chunk = os.read(master, 65536)
            except OSError:
                break
            if not chunk:
                break
            output += chunk
            for query, response in ((b"\x1b[6n", b"\x1b[24;1R"), (b"\x1b[18t", b"\x1b[8;30;80t")):
                if query in chunk and (query, output.count(query)) not in answered:
                    answered.add((query, output.count(query)))
                    os.write(master, response)
            if sigint_after_animation and not sent_sigint and b"\x1b[?25l" in output:
                time.sleep(0.1)
                os.killpg(proc.pid, signal.SIGINT)
                sent_sigint = True
        if proc.poll() is not None:
            # drain remaining output
            while True:
                ready, _, _ = select.select([master], [], [], 0.2)
                if not ready:
                    break
                try:
                    chunk = os.read(master, 65536)
                except OSError:
                    break
                if not chunk:
                    break
                output += chunk
            break

    if proc.poll() is None:
        proc.kill()
        proc.wait()
    os.close(master)
    return output, proc.returncode


def test_animation_plays():
    print("test: animation plays and freezes on the last frame")
    output, code = run_pty(ARGS + ["--logo-animation-loop", "2"])
    text = output.decode("utf-8", "replace")
    check(code == 0, f"exit code 0 (got {code})")
    check("\x1b[?25l" in text, "cursor hidden during animation")
    check("\x1b[?25h" in text, "cursor shown again after animation")
    frame_writes = text.count("\x1b[?2026h")
    check(frame_writes >= 16, f"at least 16 frame redraws (2 loops x 8 frames), got {frame_writes}")
    check("\x1b[K" not in text, "no clear-to-end-of-line (info columns never touched)")
    check("⠋" in text, "spinner frame content was drawn")


def test_infinite_timeout():
    print("test: infinite loop stops by itself on timeout")
    output, code = run_pty(ARGS + ["--logo-animation-loop", "0", "--logo-animation-timeout", "300"])
    text = output.decode("utf-8", "replace")
    check(code == 0, f"exit code 0 (got {code})")
    check("\x1b[?25h" in text, "cursor restored")
    frame_writes = text.count("\x1b[?2026h")
    check(frame_writes >= 1, f"animation ran ({frame_writes} frames)")
    check(frame_writes < 40, f"animation stopped on timeout, not running forever ({frame_writes} frames)")


def test_sigint_restores_cursor():
    print("test: SIGINT during animation restores the cursor")
    output, code = run_pty(ARGS + ["--logo-animation-loop", "0", "--logo-animation-timeout", "60000"], sigint_after_animation=True)
    text = output.decode("utf-8", "replace")
    check("\x1b[?25h" in text, "cursor shown after SIGINT")
    check(code in (130, 0), f"exit code 130 or 0 after SIGINT (got {code})")


def test_pipe_mode_is_static():
    print("test: piped output is static (no animation escapes)")
    proc = subprocess.run([MOOFETCH] + ARGS, capture_output=True, timeout=20)
    text = proc.stdout.decode("utf-8", "replace")
    check(proc.returncode == 0, "exit code 0")
    check("\x1b[?25" not in text, "no cursor hide/show sequences")
    check("\x1b[?2026h" not in text, "no synchronized-output sequences")
    check("⠏" in text, "hold (last) frame printed statically")
    check("OS:" in text, "info printed")


def test_config_file():
    print("test: animation options from a JSONC config file")
    import json, tempfile
    cfg = os.path.join(tempfile.gettempdir(), "moofetch-test-config.jsonc")
    with open(cfg, "w") as f:
        json.dump({
            "modules": ["title", "os"],
            "logo": {
                "type": "animation",
                "source": "spinner",
                "animation": {"fps": 60, "loop": 1},
            },
        }, f)
    output, code = run_pty(["-c", cfg])
    text = output.decode("utf-8", "replace")
    check(code == 0, f"exit code 0 (got {code})")
    frame_writes = text.count("\x1b[?2026h")
    check(frame_writes == 10, f"exactly 10 frames (1 loop x 10 frames from config), got {frame_writes}")
    os.remove(cfg)


def test_tall_logo():
    print("test: tall logo (logo taller than the info block) animates correctly")
    output, code = run_pty([
        "--logo-type", "animation", "--logo", "arch",
        "--structure", "title:os",
        "--logo-animation-fps", "60", "--logo-animation-loop", "2",
    ])
    text = output.decode("utf-8", "replace")
    check(code == 0, f"exit code 0 (got {code})")
    frame_writes = text.count("\x1b[?2026h")
    check(frame_writes >= 16, f"at least 16 frame redraws, got {frame_writes}")
    check("\x1b[?25h" in text, "cursor restored")
    check("\x1b[K" not in text, "info columns never touched")


def test_hold_first_static():
    print("test: --logo-animation-hold first freezes on the first frame when piped")
    proc = subprocess.run(
        [MOOFETCH, "--logo-type", "animation", "--logo", "spinner", "--logo-animation-hold", "first", "--structure", "title"],
        capture_output=True, timeout=20,
    )
    text = proc.stdout.decode("utf-8", "replace")
    check(proc.returncode == 0, "exit code 0")
    check("⠋" in text, "first frame character printed")
    check("⠏" not in text, "last frame character not printed")


def main():
    print(f"using binary: {MOOFETCH}")
    if not os.path.exists(MOOFETCH):
        print("binary not found; build it first")
        return 2
    test_animation_plays()
    test_infinite_timeout()
    test_sigint_restores_cursor()
    test_pipe_mode_is_static()
    test_config_file()
    test_tall_logo()
    test_hold_first_static()
    if failures:
        print(f"\n{len(failures)} test(s) failed")
        return 1
    print("\nall PTY smoke tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
