import fcntl
import os
import pty
import shutil
import select
import struct
import subprocess
import sys
import termios
import time


def run(args, **kwargs):
    return subprocess.run(args, text=True, capture_output=True, check=True, **kwargs)


def capture_raw_startup(modt, input_path, rows=12, cols=90):
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
    env = dict(os.environ)
    env["TERM"] = "xterm"

    proc = subprocess.Popen(
        [modt, "inspect", input_path],
        stdin=slave,
        stdout=slave,
        stderr=slave,
        close_fds=True,
        env=env,
    )
    os.close(slave)

    output = bytearray()
    deadline = time.time() + 5
    sent_quit = False
    while time.time() < deadline:
        ready, _, _ = select.select([master], [], [], 0.05)
        if ready:
            try:
                chunk = os.read(master, 4096)
            except OSError:
                break
            if chunk:
                output.extend(chunk)
                if b"MODT Inspector" in output and not sent_quit:
                    os.write(master, b"q")
                    sent_quit = True
        if proc.poll() is not None:
            break

    if proc.poll() is None:
        proc.terminate()
        proc.wait(timeout=2)
    os.close(master)

    if proc.returncode != 0:
        raise AssertionError(f"inspector exited with {proc.returncode}")

    raw = bytes(output)
    if b"\x1b[?1049h" in raw or b"\x1b[?1049l" in raw:
        raise AssertionError("inspector used terminal alternate-screen mode")
    if b"MODT Inspector" not in raw:
        raise AssertionError("inspector did not write an initial frame")


def tmux_capture(modt, input_path, keys, rows=12, cols=90):
    tmux = shutil.which("tmux")
    if tmux is None:
        raise AssertionError("tmux is required for inspector TUI tests")

    session = f"modt-inspector-{os.getpid()}-{time.time_ns()}"
    run([
        tmux,
        "new-session",
        "-d",
        "-x",
        str(cols),
        "-y",
        str(rows),
        "-s",
        session,
        modt,
        "inspect",
        input_path,
    ])

    try:
        deadline = time.time() + 5
        pane = ""
        while time.time() < deadline:
            pane = run([tmux, "capture-pane", "-t", session, "-p"]).stdout
            if "MODT Inspector" in pane:
                break
            time.sleep(0.05)
        else:
            raise AssertionError("inspector never rendered in tmux pane")

        time.sleep(0.15)
        pane = run([tmux, "capture-pane", "-t", session, "-p"]).stdout

        if keys:
            run([tmux, "send-keys", "-t", session, *keys])
            time.sleep(0.15)
            pane = run([tmux, "capture-pane", "-t", session, "-p"]).stdout

        run([tmux, "send-keys", "-t", session, "q"])
        return "\n".join(line.rstrip() for line in pane.splitlines())
    finally:
        subprocess.run([tmux, "kill-session", "-t", session], text=True, capture_output=True)


def assert_contains(frame, *needles):
    missing = [needle for needle in needles if needle not in frame]
    if missing:
        raise AssertionError("missing expected text: " + ", ".join(missing) + "\n\nFrame:\n" + frame)


def assert_not_contains(frame, *needles):
    present = [needle for needle in needles if needle in frame]
    if present:
        raise AssertionError("unexpected text: " + ", ".join(present) + "\n\nFrame:\n" + frame)


def main():
    if len(sys.argv) != 3:
        print("usage: inspector_tui_test.py <modt> <input-path>", file=sys.stderr)
        return 2

    modt = sys.argv[1]
    input_path = sys.argv[2]

    capture_raw_startup(modt, input_path)

    initial = tmux_capture(modt, input_path, [], rows=12, cols=90)
    assert_contains(initial, "MODT Inspector - Overview", "> Overview", "Objects", "* Project", "Source: test.modt")
    assert_not_contains(initial, "No entries")

    inspect_object = tmux_capture(
        modt,
        input_path,
        ["Down", "Right", "Right"],
        rows=12,
        cols=90,
    )
    assert_contains(
        inspect_object,
        "MODT Inspector - Objects",
        "* Objects",
        "* User",
        "> User",
        "Attribute: string username",
        "Method: void login(string",
        "username)",
    )
    assert_not_contains(inspect_object, "No details available.")

    scrolled = tmux_capture(
        modt,
        input_path,
        ["Down"] * 10,
        rows=7,
        cols=90,
    )
    assert_contains(scrolled, "MODT Inspector - Objects", "> Objects")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
