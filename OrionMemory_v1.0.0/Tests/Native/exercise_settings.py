"""Drive only controls in a freshly launched isolated native test process."""
import argparse,ctypes as c,json,subprocess,time,uuid
from pathlib import Path
from ctypes import wintypes as w
from observe_native import u,k,windows,post,startup,RECT
u.GetDlgItem.argtypes=[w.HWND,c.c_int];u.GetDlgItem.restype=w.HWND
u.SetWindowTextW.argtypes=[w.HWND,w.LPCWSTR]
u.SendMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM];u.SendMessageW.restype=c.c_ssize_t
u.GetWindowTextW.argtypes=[w.HWND,w.LPWSTR,c.c_int]

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--exe',required=True);ap.add_argument('--output',required=True)
    ap.add_argument('--capture',action='store_true');a=ap.parse_args()
    folder=Path(a.output).resolve();folder.mkdir(parents=True,exist_ok=True)
    fixture=folder/('settings-'+uuid.uuid4().hex);fixture.mkdir()
    prefs={'NativeAppearanceRevision':20,'TrayFontSize':8,'Language':'pt-BR','SkipUac':False,
        'StartWithWindows':False,'EnableHotkey':False,'DarkTheme':True,'Unknown':{'preserve':[7,True]}}
    path=fixture/'settings.json';path.write_text(json.dumps(prefs),encoding='utf-8')
    before=startup();checks=[];shots={}
    def check(value,label):
        checks.append({'Passed':bool(value),'Description':label});print('PASS' if value else 'FAIL',label,flush=True)
        if not value:raise AssertionError(label)
    def read():return json.loads(path.read_text(encoding='utf-8-sig'))
    proc=subprocess.Popen([str(Path(a.exe).resolve()),'--native-test-dir',str(fixture),'--test-show','settings'],creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        time.sleep(1.2);own=windows(proc.pid);settings=next(x['Handle'] for x in own if x['Class']=='OrionMemory.NativeSettings')
        host=next(x['Handle'] for x in own if x['Class']=='OrionMemory.NativeHost')
        def field(number):
            handle=u.GetDlgItem(settings,number);check(bool(handle),f'control {number} exists');return handle
        check(all(u.IsWindowVisible(u.GetDlgItem(settings,i)) for i in range(5000,5005)),'all five settings navigation buttons are visible')
        check(u.IsWindowVisible(field(5090)) and u.IsWindowVisible(field(5091)),'Close and Restore buttons are visible')
        def command(number,notification=0,handle=0):
            owner=w.DWORD();u.GetWindowThreadProcessId(settings,c.byref(owner))
            if owner.value!=proc.pid:raise RuntimeError('Target is no longer the owned diagnostic window')
            return u.SendMessageW(settings,0x111,number|(notification<<16),handle)
        def nav(page):command(5000+page);time.sleep(.15)
        def set_text(handle,text):
            value=c.create_unicode_buffer(text)
            # SetWindowText does not set an edit control in another process.
            return u.SendMessageW(handle,0xC,0,c.addressof(value))
        def commit(number,text):
            handle=field(number);set_text(handle,text);command(number,0x200,handle);time.sleep(.2)
        nav(2);commit(5141,'1');check(read()['TrayFontSize']==1,'font can be reduced to 1 pt')
        commit(5141,'18');check(read()['TrayFontSize']==18,'font upper bound is 18 pt')
        commit(5141,'99');check(read()['TrayFontSize']==18,'font above the maximum is clamped')
        commit(5141,'0');check(read()['TrayFontSize']==1,'font below the minimum is clamped')
        commit(5141,'8');check(read()['TrayFontSize']==8,'requested 8 pt preference is retained')
        color=field(5150);saved=read().get('TrayTextColor','#6FB5FF')
        set_text(color,'#12');time.sleep(.2);check(read().get('TrayTextColor','#6FB5FF')==saved,'incomplete hex input does not overwrite saved color')
        set_text(color,'#112233');time.sleep(.3);check(read()['TrayTextColor']=='#112233','complete hex input applies immediately')
        command(5326,0,u.GetDlgItem(settings,5326));time.sleep(.2)
        check(read()['TrayTextColor']=='#4CD964','palette selection applies immediately')
        check(read()['Unknown']==prefs['Unknown'],'settings edits preserve unknown preferences')
        language=field(5110);count=u.SendMessageW(language,0x146,0,0)
        check(count==30,'all 30 languages are available')
        english=-1;portuguese=-1
        for i in range(count):
            text=c.create_unicode_buffer(256);u.SendMessageW(language,0x148,i,c.addressof(text))
            if text.value.startswith('Estados Unidos'):english=i
            if text.value.startswith('Brasil'):portuguese=i
        check(english>=0 and portuguese>=0,'language list includes Brazil and United States')
        u.SendMessageW(language,0x14E,english,0);command(5110,1,language);time.sleep(.2)
        check(read()['Language']=='en-US','language changes are saved without closing settings')
        title=c.create_unicode_buffer(256);u.GetWindowTextW(settings,title,256)
        check('Settings' in title.value,'window title updates immediately after language selection')
        u.SendMessageW(language,0x14E,portuguese,0);command(5110,1,language);time.sleep(.2)
        check(read()['Language']=='pt-BR','Brazilian Portuguese can be restored immediately')
        nav(1);check(any(x['Class']=='OrionMemory.NativeSettings' for x in windows(proc.pid)),'memory page opens without replacing or restarting the process')
        for page in range(5):
            nav(page)
            check(all(u.IsWindowVisible(u.GetDlgItem(settings,number)) for number in (5090,5091)),f'Close and Restore remain visible on settings page {page}')
        if a.capture:
            r=RECT();u.GetWindowRect(settings,c.byref(r));u.SetWindowPos(settings,w.HWND(-1),40,60,r.right-r.left,r.bottom-r.top,0x10)
            helper=Path(__file__).with_name('Capturar-Janela.ps1')
            for page in range(5):
                nav(page);time.sleep(.15)
                shot=subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(helper),'-WindowHandle',str(settings),'-Output',str(folder/f'settings-{page}.png')],capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW,check=True)
                shots[str(page)]=shot.stdout.strip().splitlines()[-1]
        post(proc.pid,settings,0x10);time.sleep(.2);check(proc.poll() is None,'closing settings keeps the native tray monitor running')
        post(proc.pid,host,0x10);check(proc.wait(timeout=5)==0,'native process exits normally')
        check(startup()==before,'isolated UI checks did not change Windows startup')
    except Exception as error:checks.append({'Passed':False,'Description':repr(error)})
    finally:
        if proc.poll() is None:
            for x in windows(proc.pid):
                if x['Class']=='OrionMemory.NativeHost':post(proc.pid,x['Handle'],0x10)
            try:proc.wait(timeout=3)
            except subprocess.TimeoutExpired:proc.kill();proc.wait()
    report={'ProcessId':proc.pid,'Safety':'Isolated native test process; no startup/UAC/cleanup operations','Checks':checks,'Screenshots':shots}
    (folder/'settings-results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    return 0 if all(x['Passed'] for x in checks) else 1
if __name__=='__main__':raise SystemExit(main())
