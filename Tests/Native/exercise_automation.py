"""Exercise automation preferences in a newly owned, isolated native process.

The executable's diagnostic guard prevents all cleanup, UAC, autorun and hotkey
operations even while the fixture's automatic-cleanup checkboxes are enabled.
"""
from __future__ import annotations

import argparse
import ctypes as c
import json
import subprocess
import time
import uuid
from ctypes import wintypes as w
from pathlib import Path

from observe_native import k, post, startup, u, windows

u.GetDlgItem.argtypes = [w.HWND, c.c_int]
u.GetDlgItem.restype = w.HWND
u.SendMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
u.SendMessageW.restype = c.c_ssize_t


def child_processes(pid: int) -> list[int]:
    class Process(c.Structure):
        _fields_ = [
            ("dwSize", w.DWORD), ("cntUsage", w.DWORD),
            ("th32ProcessID", w.DWORD), ("th32DefaultHeapID", c.c_size_t),
            ("th32ModuleID", w.DWORD), ("cntThreads", w.DWORD),
            ("th32ParentProcessID", w.DWORD), ("pcPriClassBase", w.LONG),
            ("dwFlags", w.DWORD), ("szExeFile", w.WCHAR * 260),
        ]

    k.CreateToolhelp32Snapshot.argtypes = [w.DWORD, w.DWORD]
    k.CreateToolhelp32Snapshot.restype = w.HANDLE
    k.Process32FirstW.argtypes = [w.HANDLE, c.POINTER(Process)]
    k.Process32NextW.argtypes = [w.HANDLE, c.POINTER(Process)]
    snapshot = k.CreateToolhelp32Snapshot(2, 0)
    if snapshot == c.c_void_p(-1).value:
        raise c.WinError(c.get_last_error())
    result = []
    try:
        item = Process()
        item.dwSize = c.sizeof(item)
        present = k.Process32FirstW(snapshot, c.byref(item))
        while present:
            if item.th32ParentProcessID == pid:
                result.append(item.th32ProcessID)
            present = k.Process32NextW(snapshot, c.byref(item))
    finally:
        k.CloseHandle(snapshot)
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    exe = Path(args.exe).resolve(strict=True)
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    fixture = output / ("automation-" + uuid.uuid4().hex)
    fixture.mkdir()
    path = fixture / "settings.json"
    preferences = {
        "NativeAppearanceRevision": 20, "Language": "pt-BR",
        "SkipUac": False, "StartWithWindows": False, "EnableHotkey": False,
        "ShowNotifications": False, "WindowBlur": False,
        "AutoThresholdEnabled": False, "AutoThresholdPercent": 88,
        "AutoIntervalEnabled": False, "AutoIntervalMinutes": 30,
        "AllowAggressiveAuto": False, "MinimumCooldownMinutes": 15,
        "Unknown": {"preserve": [7, True]},
    }
    path.write_text(json.dumps(preferences), encoding="utf-8")
    original_startup = startup()
    checks: list[dict] = []
    proc = subprocess.Popen(
        [str(exe), "--native-test-dir", str(fixture), "--test-show", "settings"],
        creationflags=subprocess.CREATE_NO_WINDOW,
    )
    owned_pids = [proc.pid]
    fixtures = [str(fixture)]

    def check(value, description):
        checks.append({"Passed": bool(value), "Description": description})
        print("PASS" if value else "FAIL", description, flush=True)
        if not value:
            raise AssertionError(description)

    def read():
        return json.loads(path.read_text(encoding="utf-8-sig"))

    try:
        settings = host = None
        for _ in range(100):
            if proc.poll() is not None:
                raise RuntimeError(f"Diagnostic process exited early: {proc.returncode}")
            own = windows(proc.pid)
            settings = next((x["Handle"] for x in own if x["Class"] == "OrionMemory.NativeSettings"), None)
            host = next((x["Handle"] for x in own if x["Class"] == "OrionMemory.NativeHost"), None)
            if settings and host:
                break
            time.sleep(.05)
        check(settings and host, "isolated native settings and resident windows are owned by the launched PID")
        u.SetWindowPos(settings, None, -10000, -10000, 0, 0, 0x15)

        def owned(handle):
            owner = w.DWORD()
            u.GetWindowThreadProcessId(handle, c.byref(owner))
            if owner.value != proc.pid:
                raise RuntimeError("Refusing a message to a window outside the diagnostic process")

        def command(number, notification=0, handle=0):
            owned(settings)
            return u.SendMessageW(settings, 0x111, number | (notification << 16), handle)

        def field(number):
            handle = u.GetDlgItem(settings, number)
            check(bool(handle), f"automation control {number} exists")
            owned(handle)
            return handle

        def text(number):
            value = c.create_unicode_buffer(64)
            # GetWindowText cannot read another process's EDIT contents.
            u.SendMessageW(field(number), 0xD, 64, c.addressof(value))
            return value.value

        def set_text(number, value):
            handle = field(number)
            wide = c.create_unicode_buffer(value)
            u.SendMessageW(handle, 0xC, 0, c.addressof(wide))
            return handle

        def commit(number, value, enter=False):
            handle = set_text(number, value)
            if enter:
                u.SendMessageW(handle, 0x100, 13, 0)
            else:
                command(number, 0x200, handle)  # EN_KILLFOCUS
            time.sleep(.12)

        command(5001)
        check(text(5181) == "88" and text(5183) == "30", "memory-management fields show the 88-percent and 30-minute defaults")
        check(not read()["AutoThresholdEnabled"] and not read()["AutoIntervalEnabled"], "automatic cleaning starts disabled")
        command(5180, 0, field(5180))
        check(read()["AutoThresholdEnabled"] is True, "threshold checkbox applies immediately without closing settings")
        commit(5181, "90", enter=True)
        check(read()["AutoThresholdPercent"] == 90, "Enter commits a threshold edit immediately")
        edit = set_text(5181, "42")
        u.SendMessageW(edit, 0x100, 27, 0)
        check(read()["AutoThresholdPercent"] == 90 and text(5181) == "90", "Escape discards a partial edit and restores its saved effective value")
        commit(5181, "0")
        check(read()["AutoThresholdPercent"] == 1 and text(5181) == "1", "threshold clamps below-range input and updates the visible field")
        commit(5181, "999")
        check(read()["AutoThresholdPercent"] == 99 and text(5181) == "99", "threshold clamps above-range input and updates the visible field")
        set_text(5181, "")
        time.sleep(.1)
        check(read()["AutoThresholdPercent"] == 99, "an incomplete threshold edit does not overwrite the saved value")
        commit(5181, "88")
        command(5182, 0, field(5182))
        check(read()["AutoIntervalEnabled"] is True, "interval checkbox applies immediately")
        commit(5183, "0")
        check(read()["AutoIntervalMinutes"] == 1 and text(5183) == "1", "interval clamps its minimum to one minute")
        commit(5183, "999")
        check(read()["AutoIntervalMinutes"] == 240 and text(5183) == "240", "interval clamps its maximum to 240 minutes")
        commit(5183, "2", enter=True)
        check(read()["AutoIntervalMinutes"] == 2 and read()["MinimumCooldownMinutes"] == 15, "two-minute intervals persist independently of the fifteen-minute threshold cooldown")
        command(5004)
        command(5174, 0, field(5174))
        check(read()["AllowAggressiveAuto"] is True, "advanced automatic-region preference applies immediately")
        command(5174, 0, field(5174))
        check(read()["AllowAggressiveAuto"] is False, "conservative automatic cleaning can be restored immediately")
        command(5001)
        check(text(5181) == "88" and text(5183) == "2", "memory-management selections survive navigation between settings pages")
        post(proc.pid, settings, 0x10)
        time.sleep(2.2)  # Includes two real resident timer cycles in diagnostic mode.
        check(proc.poll() is None, "closing settings keeps the native resident alive")
        saved = read()
        check(saved["AutoThresholdEnabled"] and saved["AutoIntervalEnabled"] and saved["AutoIntervalMinutes"] == 2, "automatic selections remain saved after settings closes")
        check(saved["Unknown"] == preferences["Unknown"], "automation edits preserve unrelated settings")
        check(not (fixture / "history.json").exists() and not child_processes(proc.pid), "diagnostic timer never performs cleanup or launches a privileged helper")
        post(proc.pid, host, 0x10)
        check(proc.wait(timeout=5) == 0, "isolated native resident exits normally")

        legacy_fixture = output / ("automation-legacy-" + uuid.uuid4().hex)
        legacy_fixture.mkdir()
        fixtures.append(str(legacy_fixture))
        legacy = dict(preferences)
        legacy.pop("AutoThresholdPercent")
        legacy["AutoThreshold"] = 65
        path = legacy_fixture / "settings.json"
        path.write_text(json.dumps(legacy), encoding="utf-8")
        proc = subprocess.Popen(
            [str(exe), "--native-test-dir", str(legacy_fixture), "--test-show", "settings"],
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
        owned_pids.append(proc.pid)
        settings = host = None
        for _ in range(100):
            own = windows(proc.pid)
            settings = next((x["Handle"] for x in own if x["Class"] == "OrionMemory.NativeSettings"), None)
            host = next((x["Handle"] for x in own if x["Class"] == "OrionMemory.NativeHost"), None)
            if settings and host:
                break
            if proc.poll() is not None:
                raise RuntimeError("Legacy diagnostic process exited before its settings opened")
            time.sleep(.05)
        check(settings and host, "legacy threshold preferences open in a second independently owned diagnostic process")
        u.SetWindowPos(settings, None, -10000, -10000, 0, 0, 0x15)
        command(5001)
        check(text(5181) == "65", "legacy AutoThreshold is used when the modern key is absent")
        commit(5181, "88", enter=True)
        check(read().get("AutoThresholdPercent") == 88 and text(5181) == "88", "editing a legacy threshold to the modern default writes the modern key immediately")
        post(proc.pid, settings, 0x10)
        post(proc.pid, host, 0x10)
        check(proc.wait(timeout=5) == 0, "legacy diagnostic process exits normally")
        check(startup() == original_startup, "automation UI checks do not change Windows startup")
    except Exception as error:
        checks.append({"Passed": False, "Description": repr(error)})
    finally:
        if proc.poll() is None:
            for item in windows(proc.pid):
                if item["Class"] == "OrionMemory.NativeHost":
                    post(proc.pid, item["Handle"], 0x10)
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
    report = {
        "Executable": str(exe), "OwnedProcessIds": owned_pids, "Fixtures": fixtures,
        "Safety": "Owned diagnostic PID; isolated preferences; no cleanup, UAC, startup registration or hotkeys",
        "Checks": checks,
    }
    (output / "automation-results.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return 0 if all(item["Passed"] for item in checks) else 1


if __name__ == "__main__":
    raise SystemExit(main())
