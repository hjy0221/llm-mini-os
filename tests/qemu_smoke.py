#!/usr/bin/env python3
"""Boot llm-mini-os in QEMU and exercise its UART shell."""

import argparse
import os
import re
import select
import signal
import subprocess
import sys
import time
from pathlib import Path


PROMPT = b"mini-os> "
ANSI_CSI = re.compile(rb"\x1b\[[0-?]*[ -/]*[@-~]")


class TestFailure(RuntimeError):
    pass


def normalize(raw):
    raw = ANSI_CSI.sub(b"", raw)
    text = raw.decode("utf-8", errors="replace")
    text = re.sub(r"\r+\n", "\n", text)
    return text.replace("\r", "\n")


def require(condition, message):
    if not condition:
        raise TestFailure(message)


def require_text(raw, expected):
    text = normalize(raw)
    require(expected in text, "expected {!r} in output:\n{}".format(expected, text))


def require_output_line(raw, expected):
    text = "\n" + normalize(raw)
    needle = "\n{}\n".format(expected)
    require(needle in text, "expected output line {!r}:\n{}".format(expected, text))


def require_no_output_line(raw, unexpected):
    text = "\n" + normalize(raw)
    needle = "\n{}\n".format(unexpected)
    require(needle not in text,
            "unexpected output line {!r}:\n{}".format(unexpected, text))


def report_pass(name):
    print("[PASS] {}".format(name), flush=True)


class QemuConsole:
    def __init__(self, qemu, kernel):
        self.command_line = [
            qemu,
            "-M", "virt,gic-version=2,secure=off",
            "-cpu", "cortex-a72",
            "-accel", "tcg",
            "-smp", "1",
            "-m", "128M",
            "-display", "none",
            "-monitor", "none",
            "-serial", "stdio",
            "-no-reboot",
            "-kernel", kernel,
        ]
        self.process = None
        self.pending = bytearray()
        self.transcript = bytearray()

    def start(self):
        environment = os.environ.copy()
        environment["LC_ALL"] = "C"
        self.process = subprocess.Popen(
            self.command_line,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=0,
            start_new_session=True,
            env=environment,
        )

    def write(self, data):
        require(self.process is not None, "QEMU has not been started")
        require(self.process.poll() is None, "QEMU exited before input was sent")

        remaining = memoryview(data)
        try:
            input_fd = self.process.stdin.fileno()
            while remaining:
                try:
                    written = os.write(input_fd, remaining)
                except InterruptedError:
                    continue
                require(written != 0, "QEMU input pipe accepted zero bytes")
                remaining = remaining[written:]
        except (BrokenPipeError, OSError) as error:
            raise TestFailure("failed to write to QEMU: {}".format(error))

    def expect(self, marker, timeout):
        require(self.process is not None, "QEMU has not been started")
        deadline = time.monotonic() + timeout
        output_fd = self.process.stdout.fileno()

        while True:
            index = self.pending.find(marker)
            if index >= 0:
                end = index + len(marker)
                result = bytes(self.pending[:end])
                del self.pending[:end]
                return result

            remaining = deadline - time.monotonic()
            if remaining <= 0.0:
                raise TestFailure(
                    "timed out after {:.1f}s waiting for {!r}".format(
                        timeout, marker.decode("ascii", errors="replace")
                    )
                )

            try:
                readable, _, _ = select.select([output_fd], [], [], remaining)
            except InterruptedError:
                continue

            if not readable:
                continue

            try:
                chunk = os.read(output_fd, 4096)
            except InterruptedError:
                continue
            if chunk:
                self.pending.extend(chunk)
                self.transcript.extend(chunk)
                continue

            return_code = self.process.poll()
            raise TestFailure(
                "QEMU output closed before {!r}; exit code {}".format(
                    marker.decode("ascii", errors="replace"), return_code
                )
            )

    def shell_command(self, command, timeout=5.0):
        if isinstance(command, str):
            command = command.encode("ascii")
        self.write(command + b"\r")
        return self.expect(PROMPT, timeout)

    def wait_for_exit_and_drain(self, timeout):
        deadline = time.monotonic() + timeout
        output_fd = self.process.stdout.fileno()
        reached_eof = False

        while True:
            return_code = self.process.poll()
            if return_code is not None and reached_eof:
                return return_code

            remaining = deadline - time.monotonic()
            if remaining <= 0.0:
                raise TestFailure(
                    "QEMU did not exit within {:.1f}s".format(timeout)
                )

            if reached_eof:
                time.sleep(min(0.01, remaining))
                continue

            try:
                readable, _, _ = select.select(
                    [output_fd], [], [], min(0.1, remaining)
                )
            except InterruptedError:
                continue

            if not readable:
                continue

            try:
                chunk = os.read(output_fd, 4096)
            except InterruptedError:
                continue

            if chunk:
                self.pending.extend(chunk)
                self.transcript.extend(chunk)
            else:
                reached_eof = True

    def stop(self):
        if self.process is None:
            return

        if self.process.poll() is None:
            try:
                os.killpg(self.process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass

            try:
                self.process.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(self.process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                self.process.wait(timeout=2.0)

        # 프로세스 종료 뒤 pipe에 남은 진단 출력을 로그에 포함한다.
        try:
            while True:
                chunk = os.read(self.process.stdout.fileno(), 4096)
                if not chunk:
                    break
                self.pending.extend(chunk)
                self.transcript.extend(chunk)
        except (OSError, ValueError):
            pass

        for stream in (self.process.stdin, self.process.stdout):
            if stream is not None:
                try:
                    stream.close()
                except OSError:
                    pass


def read_ticks(console):
    output = console.shell_command("ticks")
    match = re.search(r"Timer ticks: ([0-9]+)", normalize(output))
    require(match is not None, "could not parse timer ticks")
    return int(match.group(1))


def run_tests(console):
    boot = console.expect(PROMPT, 15.0)
    require_text(boot, "Hello from ARM64 QEMU Mini OS!")
    require_text(boot, "Kernel is running on QEMU virt.")
    report_pass("kernel boot")

    help_output = console.shell_command("help")
    for command in ("help", "echo", "sleep", "shutdown", "reboot"):
        require_text(help_output, command)
    report_pass("help command list")

    hello = console.shell_command("hello")
    require_output_line(hello, "Hello!")
    report_pass("basic shell command")

    echo = console.shell_command("   echo    hello   world   ")
    require_output_line(echo, "hello world")
    blank = console.shell_command("     ")
    require_text(blank, "mini-os> ")
    require(b"Unknown command:" not in blank, "blank line became a command")
    report_pass("argument and whitespace parser")

    quoted = console.shell_command(
        "echo \"double quoted\" 'single quoted' pre\"joined words\"post"
    )
    require_output_line(
        quoted, "double quoted single quoted prejoined wordspost"
    )
    empty_quoted = console.shell_command("echo \"\" '' end")
    require_output_line(empty_quoted, "  end")
    report_pass("quoted arguments")

    escaped = console.shell_command(
        b'echo escaped\\ space "say \\"hello\\""'
    )
    require_output_line(escaped, 'escaped space say "hello"')
    escaped_single = console.shell_command(
        b"echo 'it\\'s fine' \\\\backslash"
    )
    require_output_line(escaped_single, "it's fine \\backslash")
    report_pass("backslash escapes")

    for command, error in (
        (b'echo "unterminated', "Parse error: unterminated double quote."),
        (b"echo 'unterminated", "Parse error: unterminated single quote."),
        (b"echo trailing" + b"\x5c", "Parse error: trailing backslash."),
    ):
        parse_error = console.shell_command(command)
        require_text(parse_error, error)
        require_no_output_line(parse_error, "unterminated")
        require_no_output_line(parse_error, "trailing")
    unsafe_parse_error = console.shell_command(b'"shutdown')
    require_text(unsafe_parse_error, "Parse error: unterminated double quote.")
    require(b"Shutting down..." not in unsafe_parse_error,
            "malformed shutdown command was executed")
    require(console.process.poll() is None,
            "QEMU exited after malformed shutdown command")
    require_output_line(
        console.shell_command("echo parser-recovered"), "parser-recovered"
    )
    report_pass("quote and escape parse errors")

    allowed = console.shell_command("echo a b c d e f g")
    require_output_line(allowed, "a b c d e f g")
    rejected = console.shell_command("echo a b c d e f g h")
    require_text(rejected, "Too many arguments (maximum 8 including command).")
    require_no_output_line(rejected, "a b c d e f g h")
    report_pass("argument count boundary")

    for command, usage, forbidden in (
        ("hello extra", "Usage: hello", "Hello!"),
        ("fault extra", "Usage: fault", "Triggering a BRK exception..."),
        ("shutdown now", "Usage: shutdown", "Shutting down..."),
        ("reboot now", "Usage: reboot", "Rebooting..."),
    ):
        output = console.shell_command(command)
        require_text(output, usage)
        require_no_output_line(output, forbidden)
        require(console.process.poll() is None, "QEMU exited after {!r}".format(command))
    report_pass("unsafe extra arguments rejected")

    for command, error in (
        ("sleep", "Usage: sleep <seconds>"),
        ("sleep abc", "sleep: seconds must be an integer from 0 to 86400"),
        ("sleep 1 2", "Usage: sleep <seconds>"),
        ("sleep 86401", "sleep: seconds must be an integer from 0 to 86400"),
        (
            "sleep 18446744073709551616",
            "sleep: seconds must be an integer from 0 to 86400",
        ),
    ):
        output = console.shell_command(command)
        require_text(output, error)
        require("Sleeping for" not in normalize(output),
                "invalid sleep command started waiting")
        require_no_output_line(output, "Done.")
    report_pass("sleep argument validation")

    backspace = console.shell_command(b"echo hellp\x7fo")
    require_output_line(backspace, "hello")
    report_pass("Backspace editing")

    console.shell_command('echo   "history quoted value"')
    replayed = console.shell_command(b"\x1b[A")
    require_output_line(replayed, "history quoted value")
    console.shell_command("     ")
    replayed_after_blank = console.shell_command(b"\x1b[A")
    require_output_line(replayed_after_blank, "history quoted value")
    restored_draft = console.shell_command(
        b"echo draft\x1b[A\x1b[B restored"
    )
    require_output_line(restored_draft, "draft restored")
    report_pass("command history and draft restoration")

    sleep_zero = console.shell_command("sleep 0")
    require_text(sleep_zero, "Sleeping for 0 seconds...")
    require_text(sleep_zero, "Done.")
    report_pass("zero-second sleep")

    mmu = console.shell_command("mmu")
    require_text(mmu, "MMU: on")
    require_text(mmu, "D-cache: off")
    require_text(mmu, "I-cache: off")
    require_text(mmu, "MAIR_EL1:  0x000000000000ff00")
    require_text(mmu, "Mapping:   identity, 4KiB granule, 2MiB blocks")
    ttbr_match = re.search(r"TTBR0_EL1: (0x[0-9a-f]+)", normalize(mmu))
    require(ttbr_match is not None, "could not parse TTBR0_EL1")
    ttbr = int(ttbr_match.group(1), 16)
    require(ttbr & 0xFFF == 0,
            "TTBR0_EL1 was not 4KiB-aligned")
    require(0x40000000 <= ttbr < 0x48000000,
            "TTBR0_EL1 was outside QEMU RAM")
    tcr_match = re.search(r"TCR_EL1:\s+(0x[0-9a-f]+)", normalize(mmu))
    require(tcr_match is not None, "could not parse TCR_EL1")
    tcr = int(tcr_match.group(1), 16)
    require(tcr & 0x3F == 32, "TCR_EL1.T0SZ was not 32")
    require((tcr >> 14) & 0x3 == 0, "TCR_EL1.TG0 was not 4KiB")
    require((tcr >> 12) & 0x3 == 3, "TCR_EL1.SH0 was not inner-shareable")
    require((tcr >> 10) & 0x3 == 1, "TCR_EL1.ORGN0 was not WBWA")
    require((tcr >> 8) & 0x3 == 1, "TCR_EL1.IRGN0 was not WBWA")
    require(tcr & (1 << 23), "TCR_EL1.EPD1 was not set")
    require((tcr >> 32) & 0x7 <= 5, "TCR_EL1.IPS was unsupported")
    report_pass("MMU identity mapping")

    memory_before = console.shell_command("mem")
    memory_match = re.search(
        r"Total pages:\s+([0-9]+).*Used pages:\s+([0-9]+).*"
        r"Free pages:\s+([0-9]+)",
        normalize(memory_before),
        re.DOTALL,
    )
    require(memory_match is not None, "could not parse page allocator statistics")
    total_pages, used_pages, free_pages = map(int, memory_match.groups())
    require(total_pages > 0 and used_pages == 0 and free_pages == total_pages,
            "unexpected initial page allocator statistics")
    for _ in range(2):
        memory_test = console.shell_command("memtest")
        require_output_line(memory_test, "memtest: PASS")
        require_no_output_line(memory_test, "memtest: FAIL")
    memory_after = console.shell_command("mem")
    require_text(memory_after, "Used pages:  0")
    require_text(memory_after, "Free pages:  {}".format(free_pages))
    report_pass("physical page allocator")

    for command, error in (
        ("taskdemo 0", "taskdemo: rounds must be an integer from 1 to 20"),
        ("taskdemo 21", "taskdemo: rounds must be an integer from 1 to 20"),
        ("taskdemo 1 extra", "Usage: taskdemo [rounds]"),
    ):
        invalid_demo = console.shell_command(command)
        require_text(invalid_demo, error)
        require("Cooperative task demo:" not in normalize(invalid_demo),
                "invalid taskdemo started tasks")
        require("Task A:" not in normalize(invalid_demo),
                "invalid taskdemo ran task A")
        require("Task B:" not in normalize(invalid_demo),
                "invalid taskdemo ran task B")
        require_no_output_line(invalid_demo, "taskdemo: PASS")
    report_pass("task demo argument validation")

    task_demo = console.shell_command("taskdemo")
    task_demo_text = normalize(task_demo)
    actual_task_lines = re.findall(
        r"(?m)^Task [AB]: [0-9]+$", task_demo_text
    )
    expected_task_lines = [
        "Task A: 1", "Task B: 1",
        "Task A: 2", "Task B: 2",
        "Task A: 3", "Task B: 3",
    ]
    require(actual_task_lines == expected_task_lines,
            "task demo output order differed:\n{}".format(task_demo_text))
    require_output_line(task_demo, "taskdemo: PASS")
    require("taskdemo: FAIL" not in task_demo_text, "task demo reported failure")

    task_stats = console.shell_command("tasks")
    for label, expected in (
        ("Active tasks", 1),
        ("Ready tasks", 0),
        ("Finished tasks", 0),
        ("Created tasks", 2),
        ("Context switches", 9),
        ("Yield calls", 7),
        ("Exited tasks", 2),
        ("Reaped tasks", 2),
        ("Stack errors", 0),
    ):
        match = re.search(r"{}:\s+([0-9]+)".format(label), normalize(task_stats))
        require(match is not None and int(match.group(1)) == expected,
                "unexpected {} scheduler statistic".format(label))
    require_text(console.shell_command("mem"), "Used pages:  0")
    report_pass("cooperative task switching")

    ticks_before = read_ticks(console)
    started = time.monotonic()
    console.write(b"sleep 3\r")
    console.expect(b"Sleeping for 3 seconds...", 5.0)
    require(b"Done." not in console.pending,
            "sleep ended before buffered UART input was injected")
    console.write(b"echo buffered-7f31\r")

    sleep_finished = console.expect(PROMPT, 12.0)
    elapsed = time.monotonic() - started
    require_text(sleep_finished, "Done.")
    require(elapsed >= 2.5, "sleep 3 returned too early ({:.3f}s)".format(elapsed))

    buffered = console.expect(PROMPT, 5.0)
    require_output_line(buffered, "buffered-7f31")
    ticks_after = read_ticks(console)
    tick_delta = (ticks_after - ticks_before) & ((1 << 64) - 1)
    require(tick_delta >= 300, "sleep advanced only {} timer ticks".format(tick_delta))
    report_pass("timer sleep and buffered UART input")

    info = console.shell_command("info")
    text = normalize(info)
    for label in ("UART RX IRQs", "UART RX bytes", "UART highwater"):
        match = re.search(r"{}:\s+([0-9]+)".format(re.escape(label)), text)
        require(match is not None and int(match.group(1)) > 0,
                "{} was not positive".format(label))
    require("UART buffered: 0" in text, "UART buffer was not drained")
    require("UART drops:   0" in text, "UART input was dropped")
    require("UART errors:  0" in text, "UART reported an error")
    report_pass("UART interrupt statistics")

    console.write(b"   shutdown   \r")
    shutdown = console.expect(b"Shutting down...", 3.0)
    require_text(shutdown, "Shutting down...")
    return_code = console.wait_for_exit_and_drain(5.0)
    require(return_code == 0, "QEMU shutdown exit code was {}".format(return_code))
    require(b"Shutdown failed." not in console.transcript, "PSCI shutdown failed")
    report_pass("PSCI shutdown")


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", required=True, help="QEMU executable")
    parser.add_argument("--kernel", required=True, help="kernel ELF path")
    parser.add_argument(
        "--log", default="build/qemu-test.log", help="UART transcript path"
    )
    return parser.parse_args()


def write_log(path, command_line, transcript):
    log_path = Path(path)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    header = "$ {}\n\n".format(" ".join(command_line)).encode("utf-8")
    log_path.write_bytes(header + transcript)


def handle_timeout(_signum, _frame):
    raise TestFailure("test exceeded the 75-second global timeout")


def main():
    arguments = parse_arguments()
    if not Path(arguments.kernel).is_file():
        print(
            "[FAIL] kernel does not exist: {}".format(arguments.kernel),
            file=sys.stderr,
        )
        return 2

    console = QemuConsole(arguments.qemu, arguments.kernel)
    signal.signal(signal.SIGALRM, handle_timeout)
    signal.signal(signal.SIGTERM, handle_timeout)
    signal.alarm(75)
    exit_code = 0

    try:
        console.start()
        run_tests(console)
    except (TestFailure, OSError, subprocess.SubprocessError) as error:
        print("[FAIL] {}".format(error), file=sys.stderr, flush=True)
        if console.transcript:
            print("\n--- QEMU UART transcript ---", file=sys.stderr)
            print(normalize(console.transcript)[-12000:], file=sys.stderr)
        exit_code = 1
    except KeyboardInterrupt:
        print("[FAIL] test interrupted", file=sys.stderr, flush=True)
        exit_code = 130
    finally:
        signal.alarm(0)
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        signal.signal(signal.SIGINT, signal.SIG_IGN)

        try:
            console.stop()
        except (OSError, subprocess.SubprocessError) as error:
            print("[FAIL] QEMU cleanup failed: {}".format(error),
                  file=sys.stderr, flush=True)
            exit_code = 1

        try:
            write_log(arguments.log, console.command_line,
                      bytes(console.transcript))
        except OSError as error:
            print("[WARN] could not write QEMU log: {}".format(error),
                  file=sys.stderr, flush=True)

    if exit_code == 0:
        print("All QEMU regression tests passed.", flush=True)
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
