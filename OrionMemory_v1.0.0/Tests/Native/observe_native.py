"""Isolated Win32 lifecycle/RAM check. Never controls an existing user process."""
from __future__ import annotations
import argparse, ctypes as c, json, statistics, subprocess, time, uuid, winreg
from ctypes import wintypes as w
from pathlib import Path

u=c.WinDLL('user32',use_last_error=True);k=c.WinDLL('kernel32',use_last_error=True)
SIZE=c.c_size_t;HANDLE=w.HANDLE
class EX2(c.Structure):
    _fields_=[('cb',w.DWORD),('PageFaultCount',w.DWORD)]+[(n,SIZE) for n in (
        'PeakWorkingSetSize','WorkingSetSize','QuotaPeakPagedPoolUsage','QuotaPagedPoolUsage',
        'QuotaPeakNonPagedPoolUsage','QuotaNonPagedPoolUsage','PagefileUsage','PeakPagefileUsage',
        'PrivateUsage','PrivateWorkingSetSize','SharedCommitUsage')]
class RECT(c.Structure):_fields_=[('left',c.c_long),('top',c.c_long),('right',c.c_long),('bottom',c.c_long)]
EnumProc=c.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
u.EnumWindows.argtypes=[EnumProc,w.LPARAM];u.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)]
u.GetClassNameW.argtypes=[w.HWND,w.LPWSTR,c.c_int];u.GetWindowTextW.argtypes=[w.HWND,w.LPWSTR,c.c_int]
u.GetWindowRect.argtypes=[w.HWND,c.POINTER(RECT)];u.IsWindowVisible.argtypes=[w.HWND]
u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
u.SetWindowPos.argtypes=[w.HWND,w.HWND,c.c_int,c.c_int,c.c_int,c.c_int,w.UINT]
u.GetGuiResources.argtypes=[HANDLE,w.DWORD];u.GetGuiResources.restype=w.DWORD
k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];k.OpenProcess.restype=HANDLE
k.CloseHandle.argtypes=[HANDLE]
k.K32GetProcessMemoryInfo.argtypes=[HANDLE,c.POINTER(EX2),w.DWORD]
k.K32GetProcessMemoryInfo.restype=w.BOOL

def windows(pid:int):
    result=[]
    @EnumProc
    def each(hwnd,unused):
        owner=w.DWORD();u.GetWindowThreadProcessId(hwnd,c.byref(owner))
        if owner.value==pid:
            name=c.create_unicode_buffer(256);title=c.create_unicode_buffer(512)
            u.GetClassNameW(hwnd,name,256);u.GetWindowTextW(hwnd,title,512)
            result.append({'Handle':hwnd,'Class':name.value,'Title':title.value,'Visible':bool(u.IsWindowVisible(hwnd))})
        return True
    u.EnumWindows(each,0);return result

def post(pid,hwnd,message,wp=0,lp=0):
    owner=w.DWORD();u.GetWindowThreadProcessId(hwnd,c.byref(owner))
    if owner.value!=pid:raise RuntimeError('Window does not belong to this diagnostic process')
    if not u.PostMessageW(hwnd,message,wp,lp):raise c.WinError(c.get_last_error())

def startup():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER,r'Software\Microsoft\Windows\CurrentVersion\Run') as key:
            return winreg.QueryValueEx(key,'OrionMemory')
    except FileNotFoundError:return None

def modules(pid):
    # Toolhelp snapshot is read-only; inspect the target's module names, not the observer.
    class MOD(c.Structure):
        _fields_=[('dwSize',w.DWORD),('th32ModuleID',w.DWORD),('th32ProcessID',w.DWORD),
            ('GlblcntUsage',w.DWORD),('ProccntUsage',w.DWORD),('modBaseAddr',c.c_void_p),
            ('modBaseSize',w.DWORD),('hModule',HANDLE),('szModule',w.WCHAR*256),('szExePath',w.WCHAR*260)]
    k.CreateToolhelp32Snapshot.argtypes=[w.DWORD,w.DWORD];k.CreateToolhelp32Snapshot.restype=HANDLE
    k.Module32FirstW.argtypes=[HANDLE,c.POINTER(MOD)];k.Module32NextW.argtypes=[HANDLE,c.POINTER(MOD)]
    h=k.CreateToolhelp32Snapshot(0x18,pid)
    if h==c.c_void_p(-1).value:raise c.WinError(c.get_last_error())
    names=[]
    try:
        item=MOD();item.dwSize=c.sizeof(item);ok=k.Module32FirstW(h,c.byref(item))
        while ok:names.append(item.szModule);ok=k.Module32NextW(h,c.byref(item))
    finally:k.CloseHandle(h)
    return names

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--exe',required=True);ap.add_argument('--output',required=True)
    ap.add_argument('--samples',type=int,default=10);ap.add_argument('--capture',action='store_true')
    ap.add_argument('--cases',default='tray,main,settings,history,about');args=ap.parse_args()
    exe=Path(args.exe).resolve(strict=True);out=Path(args.output).resolve();out.mkdir(parents=True,exist_ok=True)
    before=startup();results=[]
    for case in args.cases.split(','):
        fixture=out/(case+'-'+uuid.uuid4().hex);fixture.mkdir()
        prefs={'Language':'pt-BR','SkipUac':False,'StartWithWindows':False,'StartMinimized':False,
            'EnableHotkey':False,'ShowNotifications':False,'AppearanceRevision':20,'NativeAppearanceRevision':20,
            'TrayFontSize':8,'CloseToTray':True,'DarkTheme':True,'WindowBlur':False}
        (fixture/'settings.json').write_text(json.dumps(prefs),encoding='utf-8')
        command=[str(exe),'--native-test-dir',str(fixture)]
        if case!='tray':command+=['--test-show',case]
        proc=subprocess.Popen(command,creationflags=subprocess.CREATE_NO_WINDOW)
        handle=None;row={'Case':case,'ProcessId':proc.pid,'Samples':[]}
        try:
            time.sleep(2)
            if proc.poll() is not None:raise RuntimeError(f'App exited before test: {proc.returncode}')
            own=windows(proc.pid);host=next(x for x in own if x['Class']=='OrionMemory.NativeHost')
            visible=[x for x in own if x['Visible'] and x['Class'].startswith('OrionMemory.')]
            if case!='tray' and not visible:raise RuntimeError('Requested native window did not open')
            row['Windows']=own
            if args.capture and visible:
                target=visible[-1];r=RECT();u.GetWindowRect(target['Handle'],c.byref(r))
                u.SetWindowPos(target['Handle'],w.HWND(-1),40,60,r.right-r.left,r.bottom-r.top,0x10)
                time.sleep(.4)
                helper=Path(__file__).with_name('Capturar-Janela.ps1')
                capture=subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(helper),
                    '-WindowHandle',str(target['Handle']),'-Output',str(out/(case+'-captura.png'))],capture_output=True,text=True,
                    creationflags=subprocess.CREATE_NO_WINDOW,check=True)
                row['Screenshot']=capture.stdout.strip().splitlines()[-1]
            else:
                for window in visible:u.SetWindowPos(window['Handle'],None,-10000,-10000,0,0,0x15)
            handle=k.OpenProcess(0x410,False,proc.pid)
            if not handle:raise c.WinError(c.get_last_error())
            loaded=modules(proc.pid);row['ManagedModules']=[x for x in loaded if any(t in x.lower() for t in ('coreclr','wpfgfx','presentationframework','windowsbase','clr.dll'))]
            if row['ManagedModules']:raise RuntimeError('A managed GUI runtime was loaded')
            gdi_before=u.GetGuiResources(handle,0)
            for i in range(args.samples):
                metrics=EX2();metrics.cb=c.sizeof(metrics)
                if not k.K32GetProcessMemoryInfo(handle,c.byref(metrics),metrics.cb):raise c.WinError(c.get_last_error())
                row['Samples'].append({'PrivateWorkingSetMiB':metrics.PrivateWorkingSetSize/1048576,
                    'WorkingSetMiB':metrics.WorkingSetSize/1048576,'PrivateCommitMiB':metrics.PrivateUsage/1048576})
                post(proc.pid,host['Handle'],0x8014);time.sleep(.5)
            row['PrivateWorkingSetMiB']=statistics.median(x['PrivateWorkingSetMiB'] for x in row['Samples'])
            row['GdiBefore']=gdi_before;row['GdiAfter']=u.GetGuiResources(handle,0)
            if row['GdiAfter']>gdi_before+2:raise RuntimeError('GDI resources grew during idle redraws')
            if case=='main':
                main_window=next(x for x in own if x['Class']=='OrionMemory.MainWindow')
                post(proc.pid,main_window['Handle'],0x10);time.sleep(.2)
                row['CloseKeepsNativeResident']=proc.poll() is None and not bool(u.IsWindowVisible(main_window['Handle']))
                if not row['CloseKeepsNativeResident']:raise RuntimeError('Closing the main window did not return to tray')
            post(proc.pid,host['Handle'],0x10);row['ExitCode']=proc.wait(timeout=5)
            if row['ExitCode']!=0:raise RuntimeError(f'Exit code {row["ExitCode"]}')
            row['Passed']=True
        except Exception as error:row['Passed']=False;row['Error']=repr(error)
        finally:
            if handle:k.CloseHandle(handle)
            if proc.poll() is None:
                for item in windows(proc.pid):
                    if item['Class']=='OrionMemory.NativeHost':post(proc.pid,item['Handle'],0x10)
                try:proc.wait(timeout=3)
                except subprocess.TimeoutExpired:proc.kill();proc.wait()
        row['AutorunUnchanged']=startup()==before
        results.append(row);print(case, 'PASS' if row.get('Passed') else 'FAIL',row.get('PrivateWorkingSetMiB'),row.get('Error',''),flush=True)
    report={'Executable':str(exe),'Counter':'PROCESS_MEMORY_COUNTERS_EX2.PrivateWorkingSetSize',
        'Safety':'Owned PIDs, isolated settings, no real cleanup/UAC/startup/hotkeys; no working-set trimming',
        'AutorunUnchanged':startup()==before,'Results':results}
    (out/'native-ui-results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    return 0 if report['AutorunUnchanged'] and all(x.get('Passed') for x in results) else 1
if __name__=='__main__':raise SystemExit(main())
