"""Exercise only a fresh diagnostic process; never sends cleanup/UAC commands."""
from __future__ import annotations

import argparse
import ctypes as c
import json
import subprocess
import time
import uuid
from ctypes import wintypes as w
from pathlib import Path

from observe_native import EX2, RECT, k, startup, u, windows

WM_CLOSE = 0x10
WM_COMMAND = 0x111
WM_REQUEST = 0x8001
WM_SETTINGS_CHANGED = 0x8014
MAIN_CLASS = "OrionMemory.MainWindow"
SETTINGS_CLASS = "OrionMemory.NativeSettings"
HOST_CLASS = "OrionMemory.NativeHost"

u.SendMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
u.SendMessageW.restype = c.c_ssize_t
u.IsWindow.argtypes = [w.HWND]
u.IsWindow.restype = w.BOOL
u.GetWindow.argtypes = [w.HWND, w.UINT]
u.GetWindow.restype = w.HWND
u.GetDpiForWindow.argtypes = [w.HWND]
u.GetDpiForWindow.restype = w.UINT


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    try:
        u.SetProcessDpiAwarenessContext.argtypes = [c.c_void_p]
        u.SetProcessDpiAwarenessContext(c.c_void_p(-4))
    except AttributeError:
        pass

    executable = Path(args.exe).resolve(strict=True)
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    fixture = output / ("main-lifecycle-" + uuid.uuid4().hex)
    fixture.mkdir()
    settings_path = fixture / "settings.json"
    prefs = {
        "Language": "pt-BR", "SkipUac": False, "StartWithWindows": False,
        "EnableHotkey": False, "AutoThresholdEnabled": False,
        "AutoIntervalEnabled": False, "ShowNotifications": False,
        "NativeAppearanceRevision": 20, "AppearanceRevision": 20,
        "TrayFontSize": 8, "CloseToTray": True, "DarkTheme": True,
        "ShowGraph": False, "RefreshSeconds": 1,
        "Unknown": {"preserve": [7, True]},
    }
    settings_path.write_text(json.dumps(prefs), encoding="utf-8")
    autorun_before = startup()
    checks: list[dict] = []
    gdi_samples: list[dict] = []
    details: dict = {}
    handle = None

    def check(value, description):
        checks.append({"Passed": bool(value), "Description": description})
        print("PASS" if value else "FAIL", description, flush=True)
        if not value:
            raise AssertionError(description)

    proc = subprocess.Popen(
        [str(executable), "--native-test-dir", str(fixture), "--test-show", "main"],
        creationflags=subprocess.CREATE_NO_WINDOW,
    )

    def owned(hwnd):
        pid = w.DWORD()
        u.GetWindowThreadProcessId(hwnd, c.byref(pid))
        if not hwnd or pid.value != proc.pid:
            raise RuntimeError("Diagnostic HWND no longer belongs to the launched PID")
        return hwnd

    def send(hwnd, message, wp=0, lp=0):
        return u.SendMessageW(owned(hwnd), message, wp, lp)

    def find(class_name, visible=None):
        return next((item["Handle"] for item in windows(proc.pid)
                     if item["Class"] == class_name and
                     (visible is None or item["Visible"] == visible)), None)

    def wait_for(predicate, description, timeout=4.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            value = predicate()
            if value:
                return value
            if proc.poll() is not None:
                raise RuntimeError(f"Diagnostic process exited early ({proc.returncode})")
            time.sleep(.025)
        raise AssertionError(description)

    def bounds(hwnd):
        rectangle = RECT()
        if not u.GetWindowRect(owned(hwnd), c.byref(rectangle)):
            raise c.WinError(c.get_last_error())
        return (rectangle.left, rectangle.top,
                rectangle.right - rectangle.left, rectangle.bottom - rectangle.top)

    def close_reopen(host, main_window):
        started = time.perf_counter()
        send(main_window, WM_CLOSE)
        send(host, WM_REQUEST, 1)
        reopened = wait_for(lambda: find(MAIN_CLASS, True), "Main did not reopen immediately")
        return reopened, time.perf_counter() - started

    def gdi_count():
        # Paint owns temporary DC/bitmap objects for a few milliseconds. Compare
        # the resting floor, while retaining every raw sample for inspection.
        samples = []
        for _ in range(5):
            samples.append(int(u.GetGuiResources(handle, 0)))
            time.sleep(.01)
        return min(samples), samples

    def set_graph(host, enabled):
        document = json.loads(settings_path.read_text(encoding="utf-8-sig"))
        document["ShowGraph"] = enabled
        temporary = fixture / "settings.exercise.tmp"
        temporary.write_text(json.dumps(document), encoding="utf-8")
        temporary.replace(settings_path)
        send(host, WM_SETTINGS_CHANGED)

    try:
        host = wait_for(lambda: find(HOST_CLASS), "Diagnostic host did not start")
        main_window = wait_for(lambda: find(MAIN_CLASS, True), "Main did not open")
        check(proc.poll() is None, "isolated native main opens successfully")
        handle = k.OpenProcess(0x410, False, proc.pid)
        check(bool(handle), "read-only process metrics handle opens")

        rectangle = bounds(main_window)
        owned(main_window)
        if not u.SetWindowPos(main_window, None, 140, 110,
                              rectangle[2] + 48, rectangle[3] + 32, 0x14):
            raise c.WinError(c.get_last_error())
        custom = bounds(main_window)
        main_window, elapsed = close_reopen(host, main_window)
        details["ImmediateReopenSeconds"] = elapsed
        check(elapsed < .95, "main closes and reopens before the one-second sample timer")
        check(bounds(main_window) == custom, "custom position and size survive immediate recreation")

        dpi = u.GetDpiForWindow(main_window) or 96
        set_graph(host, True)
        graphed = bounds(main_window)
        graph_delta = (66 * dpi + 48) // 96
        check(graphed[:3] == custom[:3] and graphed[3] == custom[3] + graph_delta,
              "enabling the graph adds exactly 66 logical pixels without changing width or position")
        main_window, _ = close_reopen(host, main_window)
        check(bounds(main_window) == graphed, "graph dimensions survive main recreation")
        set_graph(host, False)
        check(bounds(main_window) == custom, "disabling the graph restores the previous custom size")

        send(main_window, WM_COMMAND, 1002)
        settings_window = wait_for(lambda: find(SETTINGS_CLASS, True), "Owned settings did not open")
        check(u.GetWindow(settings_window, 4) == main_window, "settings retains main as its owner")
        send(main_window, WM_CLOSE)
        check(u.IsWindow(main_window) and not u.IsWindowVisible(main_window),
              "main is hidden, rather than destroyed, while settings is owned")
        check(u.IsWindow(settings_window) and u.IsWindowVisible(settings_window),
              "hiding main preserves the open settings window")
        send(settings_window, WM_CLOSE)
        wait_for(lambda: not u.IsWindow(main_window),
                 "Hidden main was not released after settings closed", 3.5)
        check(not u.IsWindow(main_window), "the next sample releases hidden main after settings closes")
        send(host, WM_REQUEST, 1)
        main_window = wait_for(lambda: find(MAIN_CLASS, True), "Main did not reopen after owned settings")
        check(bounds(main_window) == custom, "position and custom size survive the owned-settings lifecycle")

        # Warm lazy Win32/font caches before comparing like-for-like states.
        for _ in range(2):
            main_window, _ = close_reopen(host, main_window)
        for cycle in range(10):
            open_gdi, open_samples = gdi_count()
            send(main_window, WM_CLOSE)
            check(not u.IsWindow(main_window), f"cycle {cycle + 1} releases main immediately")
            closed_gdi, closed_samples = gdi_count()
            send(host, WM_REQUEST, 1)
            main_window = wait_for(lambda: find(MAIN_CLASS, True), "Main did not reopen during repeated cycles")
            check(bounds(main_window) == custom, f"cycle {cycle + 1} preserves custom geometry")
            gdi_samples.append({"Cycle": cycle + 1, "Open": open_gdi, "Closed": closed_gdi,
                                "OpenRaw": open_samples, "ClosedRaw": closed_samples})
        for state in ("Open", "Closed"):
            values = [sample[state] for sample in gdi_samples]
            check(max(values) - min(values) <= 2,
                  f"GDI resources stay within two objects over ten {state.lower()} cycles")
        check(all(sample["Closed"] < sample["Open"] for sample in gdi_samples),
              "closing main releases its font/pen resources")

        metrics = EX2()
        metrics.cb = c.sizeof(metrics)
        if k.K32GetProcessMemoryInfo(handle, c.byref(metrics), metrics.cb):
            details["PrivateWorkingSetMiB"] = metrics.PrivateWorkingSetSize / 1048576
            details["PrivateCommitMiB"] = metrics.PrivateUsage / 1048576
        check(json.loads(settings_path.read_text(encoding="utf-8-sig"))["Unknown"] == prefs["Unknown"],
              "isolated graph preference updates preserve unrelated settings")
        send(host, WM_REQUEST, 6)
        check(proc.wait(timeout=5) == 0, "Sair closes the native resident with exit code zero")
    except Exception as error:
        checks.append({"Passed": False, "Description": repr(error)})
        details["WindowsAtFailure"] = [dict(item, Owner=u.GetWindow(item["Handle"], 4)) for item in windows(proc.pid)]
        print("FAIL", repr(error), flush=True)
    finally:
        if handle:
            k.CloseHandle(handle)
        if proc.poll() is None:
            host = find(HOST_CLASS)
            if host:
                send(host, WM_CLOSE)
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        unchanged = startup() == autorun_before
        checks.append({"Passed": unchanged, "Description": "Windows startup entry remains unchanged"})

    report = {
        "Executable": str(executable), "ProcessId": proc.pid,
        "Safety": "Owned diagnostic PID and isolated settings; no cleanup/UAC/hotkey/startup actions; no working-set trimming",
        "Fixture": str(fixture), "ExitCode": proc.returncode,
        "Checks": checks, "GdiSamples": gdi_samples, "Details": details,
    }
    (output / "main-lifecycle-results.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return 0 if all(check["Passed"] for check in checks) else 1


if __name__ == "__main__":
    raise SystemExit(main())
