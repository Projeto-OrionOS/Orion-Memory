/* Native authorization installer. Never called by monitoring or tests.
   Its only privileged action is installing/removing this user's fixed launcher. */
#include <aclapi.h>
static int g_native_authorizing;
static int native_authorization_busy(void){return g_native_authorizing;}

static int auth_paths(const wchar_t *sid,wchar_t *parent,wchar_t *directory,wchar_t *exe) {
    wchar_t program_files[MAX_PATH];
    if(FAILED(SHGetFolderPathW(NULL,CSIDL_PROGRAM_FILES,NULL,SHGFP_TYPE_CURRENT,program_files)))return 0;
    return path_join(parent,MAX_PATH,program_files,L"OrionMemoryAuthorized")&&
        path_join(directory,MAX_PATH,parent,sid)&&path_join(exe,MAX_PATH,directory,L"OrionMemory.exe");
}
static int auth_escape(const wchar_t *input,wchar_t *output,size_t cap) {
    size_t at=0;
    for(;*input;++input){const wchar_t *value=NULL;wchar_t single[2]={*input,0};
        switch(*input){case L'&':value=L"&amp;";break;case L'<':value=L"&lt;";break;case L'>':value=L"&gt;";break;case L'\"':value=L"&quot;";break;case L'\'':value=L"&apos;";break;default:value=single;}
        size_t n=wcslen(value);if(at+n>=cap)return 0;memcpy(output+at,value,n*sizeof(wchar_t));at+=n;
    }output[at]=0;return 1;
}
static int auth_make_xml(const wchar_t *sid,const wchar_t *exe,const wchar_t *directory,wchar_t *xml,size_t cap) {
    wchar_t escaped_exe[MAX_PATH*6],escaped_dir[MAX_PATH*6];
    if(!auth_escape(exe,escaped_exe,MAX_PATH*6)||!auth_escape(directory,escaped_dir,MAX_PATH*6))return 0;
    int length=swprintf(xml,cap,L"<?xml version=\"1.0\" encoding=\"UTF-16\"?>"
        L"<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">"
        L"<RegistrationInfo><Description>Orion Memory: abertura autorizada pelo usuario.</Description>"
        L"<SecurityDescriptor>D:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;GRGX;;;%ls)</SecurityDescriptor></RegistrationInfo>"
        L"<Principals><Principal id=\"Author\"><UserId>%ls</UserId><LogonType>InteractiveToken</LogonType><RunLevel>HighestAvailable</RunLevel></Principal></Principals>"
        L"<Settings><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy><DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>"
        L"<StopIfGoingOnBatteries>false</StopIfGoingOnBatteries><AllowHardTerminate>false</AllowHardTerminate><StartWhenAvailable>false</StartWhenAvailable>"
        L"<RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable><AllowStartOnDemand>true</AllowStartOnDemand><Enabled>true</Enabled><Hidden>false</Hidden>"
        L"<RunOnlyIfIdle>false</RunOnlyIfIdle><WakeToRun>false</WakeToRun><ExecutionTimeLimit>PT0S</ExecutionTimeLimit><Priority>7</Priority></Settings>"
        L"<Actions Context=\"Author\"><Exec><Command>%ls</Command><Arguments>--authorized-start</Arguments><WorkingDirectory>%ls</WorkingDirectory></Exec></Actions></Task>",sid,sid,escaped_exe,escaped_dir);
    return length>0&&(size_t)length<cap;
}
static int auth_secure_directory(const wchar_t *path) {
    if(!CreateDirectoryW(path,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return 0;
    if(!real_path(path,1))return 0;
    PSECURITY_DESCRIPTOR descriptor=NULL;PACL acl=NULL;BOOL present=FALSE,defaulted=FALSE;
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;GRGX;;;BU)",SDDL_REVISION_1,&descriptor,NULL))return 0;
    int ok=GetSecurityDescriptorDacl(descriptor,&present,&acl,&defaulted)&&present&&
        SetNamedSecurityInfoW((LPWSTR)path,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,NULL,NULL,acl,NULL)==ERROR_SUCCESS;
    LocalFree(descriptor);return ok;
}
static int auth_copy(const wchar_t *source,const wchar_t *target) {
    if(!_wcsicmp(source,target))return real_path(source,0);
    DWORD attr=GetFileAttributesW(target);if(attr!=INVALID_FILE_ATTRIBUTES&&(attr&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)))return 0;
    wchar_t temporary[MAX_PATH];if(swprintf(temporary,MAX_PATH,L"%ls.%lu.tmp",target,GetCurrentProcessId())<0)return 0;
    if(!CopyFileW(source,temporary,TRUE))return 0;
    int ok=MoveFileExW(temporary,target,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);if(!ok)DeleteFileW(temporary);return ok;
}
static int auth_valid_task(const wchar_t *sid,const wchar_t *exe) {
    wchar_t arguments[512],task[256];swprintf(task,256,L"OrionMemory.Elevated.%ls",sid);
    swprintf(arguments,512,L"/Query /TN \"%ls\" /XML",task);char *raw=NULL;
    if(!run_scheduler(arguments,&raw)){free(raw);return 0;}
    wchar_t *xml=scheduler_xml(raw);free(raw);if(!xml)return 0;
    wchar_t action[MAX_PATH],args[128],principal[192],level[64],enabled[16];
    int valid=xml_value(xml,L"Command",action,MAX_PATH)&&!_wcsicmp(action,exe)&&
        xml_value(xml,L"Arguments",args,128)&&!wcscmp(args,L"--authorized-start")&&
        xml_value(xml,L"UserId",principal,192)&&!wcscmp(principal,sid)&&
        xml_value(xml,L"RunLevel",level,64)&&!wcscmp(level,L"HighestAvailable")&&
        (!xml_value(xml,L"Enabled",enabled,16)||!wcscmp(enabled,L"true"));
    const wchar_t *first=wcsstr(xml,L"<Exec>");if(!first||wcsstr(first+6,L"<Exec>")||wcsstr(xml,L"<ComHandler>"))valid=0;
    free(xml);return valid;
}
static int native_authorization_ready(void) {
    if(g_test_mode)return 0;
    wchar_t sid[192],parent[MAX_PATH],directory[MAX_PATH],exe[MAX_PATH],current[MAX_PATH];
    if(!current_sid(sid,192)||!auth_paths(sid,parent,directory,exe))return 0;
    GetModuleFileNameW(NULL,current,MAX_PATH);
    return real_path(parent,1)&&real_path(directory,1)&&real_path(exe,0)&&same_product(current,exe)&&auth_valid_task(sid,exe);
}
static int native_authorization_worker(const wchar_t *mode,const wchar_t *requested_sid) {
    if(g_test_mode||!process_elevated()||(wcscmp(mode,L"enable")&&wcscmp(mode,L"disable")))return 2;
    wchar_t sid[192],parent[MAX_PATH],directory[MAX_PATH],exe[MAX_PATH],arguments[1024];
    if(!current_sid(sid,192)||wcscmp(sid,requested_sid)||!auth_paths(sid,parent,directory,exe))return 2;
    if(!wcscmp(mode,L"disable")){
        swprintf(arguments,1024,L"/Delete /TN \"OrionMemory.Elevated.%ls\" /F",sid);
        return run_scheduler(arguments,NULL)?0:1;
    }
    if(!auth_secure_directory(parent)||!auth_secure_directory(directory))return 3;
    wchar_t source[MAX_PATH],source_dir[MAX_PATH];GetModuleFileNameW(NULL,source,MAX_PATH);copyw(source_dir,MAX_PATH,source);
    wchar_t *slash=wcsrchr(source_dir,L'\\');if(!slash)return 3;*slash=0;
    if(!auth_copy(source,exe))return 4;
    wchar_t locales[MAX_PATH],source_locales[MAX_PATH],pattern[MAX_PATH];
    path_join(locales,MAX_PATH,directory,L"Languages");path_join(source_locales,MAX_PATH,source_dir,L"Languages");
    if(!auth_secure_directory(locales))return 3;
    path_join(pattern,MAX_PATH,source_locales,L"*.json");WIN32_FIND_DATAW found;HANDLE search=FindFirstFileW(pattern,&found);
    if(search!=INVALID_HANDLE_VALUE){int ok=1;do{
        if(found.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))continue;
        wchar_t from[MAX_PATH],to[MAX_PATH];path_join(from,MAX_PATH,source_locales,found.cFileName);path_join(to,MAX_PATH,locales,found.cFileName);
        if(!auth_copy(from,to)){ok=0;break;}
    }while(FindNextFileW(search,&found));FindClose(search);if(!ok)return 4;}
    wchar_t from_metadata[MAX_PATH],to_metadata[MAX_PATH];path_join(from_metadata,MAX_PATH,source_dir,L"Languages.json");path_join(to_metadata,MAX_PATH,directory,L"Languages.json");
    if(real_path(from_metadata,0)&&!auth_copy(from_metadata,to_metadata))return 4;
    wchar_t xml[8192],xml_file[MAX_PATH];if(!auth_make_xml(sid,exe,directory,xml,8192))return 5;
    swprintf(xml_file,MAX_PATH,L"%ls\\task.%lu.xml",directory,GetCurrentProcessId());
    HANDLE file=CreateFileW(xml_file,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(file==INVALID_HANDLE_VALUE)return 5;
    const WORD bom=0xFEFF;DWORD written;int ok=WriteFile(file,&bom,2,&written,NULL)&&written==2;
    DWORD bytes=(DWORD)(wcslen(xml)*sizeof(wchar_t));ok=ok&&WriteFile(file,xml,bytes,&written,NULL)&&written==bytes&&FlushFileBuffers(file);CloseHandle(file);
    if(ok){swprintf(arguments,1024,L"/Create /TN \"OrionMemory.Elevated.%ls\" /XML \"%ls\" /F",sid,xml_file);ok=run_scheduler(arguments,NULL);}
    DeleteFileW(xml_file);return ok&&auth_valid_task(sid,exe)?0:6;
}
static int native_configure_authorization(HWND owner,int desired) {
    if(g_test_mode)return 0;
    wchar_t sid[192],source[MAX_PATH],arguments[384];if(!current_sid(sid,192))return 0;
    if(!desired){
        wchar_t parent[MAX_PATH],directory[MAX_PATH],exe[MAX_PATH];
        if(!auth_paths(sid,parent,directory,exe))return 0;
        /* An authorization from an older app version must be removable too. */
        if(!auth_valid_task(sid,exe))return 1;
    }
    GetModuleFileNameW(NULL,source,MAX_PATH);swprintf(arguments,384,L"--native-configure-authorization %ls %ls",desired?L"enable":L"disable",sid);
    SHELLEXECUTEINFOW launch;memset(&launch,0,sizeof(launch));launch.cbSize=sizeof(launch);launch.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;
    launch.hwnd=owner;launch.lpVerb=L"runas";launch.lpFile=source;launch.lpParameters=arguments;launch.nShow=SW_HIDE;
    g_native_authorizing=1;
    if(!ShellExecuteExW(&launch)||!launch.hProcess){g_native_authorizing=0;return 0;}
    EnableWindow(owner,FALSE);DWORD result=STILL_ACTIVE;
    while(GetExitCodeProcess(launch.hProcess,&result)&&result==STILL_ACTIVE){
        MsgWaitForMultipleObjects(1,&launch.hProcess,FALSE,100,QS_ALLINPUT);
        MSG message;while(PeekMessageW(&message,NULL,0,0,PM_REMOVE)){if(message.message==WM_QUIT){PostQuitMessage((int)message.wParam);break;}TranslateMessage(&message);DispatchMessageW(&message);}
    }
    CloseHandle(launch.hProcess);g_native_authorizing=0;
    if(IsWindow(owner)){EnableWindow(owner,TRUE);SetActiveWindow(owner);}
    if(g_exit_pending&&!g_busy)PostMessageW(g_hwnd,WM_CLOSE,0,0);
    return result==0;
}
static int native_authorization_self_test(void) {
    wchar_t escaped[128],xml[8192];
    int ok=auth_escape(L"C:\\A&B\\<Orion>\\\"x\"",escaped,128)&&!wcscmp(escaped,L"C:\\A&amp;B\\&lt;Orion&gt;\\&quot;x&quot;");
    int failures=check(ok,"authorization XML escapes paths without command interpolation");
    ok=auth_make_xml(L"S-1-5-21-100",L"C:\\Program Files\\OrionMemory.exe",L"C:\\Program Files",xml,8192)&&
        wcsstr(xml,L"<Arguments>--authorized-start</Arguments>")&&wcsstr(xml,L"<SecurityDescriptor>D:P")&&
        !wcsstr(xml,L"<Triggers>")&&wcsstr(xml,L"<LogonType>InteractiveToken</LogonType>");
    failures+=check(ok,"authorization creates only a fixed user-scoped on-demand launcher");
    failures+=check(g_test_mode&&!native_configure_authorization(NULL,1)&&native_authorization_worker(L"enable",L"S-1-5-21-100")==2,
        "diagnostic mode cannot authorize elevation or install tasks");
    return failures;
}
