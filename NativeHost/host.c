#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <sddl.h>
#include <winver.h>
#include <windowsx.h>
#include <dwmapi.h>
#define PSAPI_VERSION 2
#include <psapi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <math.h>

/* Orion Memory: native Win32 monitor, windows, settings and cleanup. No CLR/WPF. */
#define HOST_CLASS L"OrionMemory.NativeHost"
#define HOST_MUTEX L"Local\\OrionOS.OrionMemory.Native"
#define WM_TRAY (WM_APP + 71)
#define WM_REQUEST (WM_APP + 1)
#define WM_CLEAN_BUSY (WM_APP + 30)
#define WM_BALLOON (WM_APP + 31)
#define POLL_TIMER 1
#define CLICK_TIMER 2
#define HOTKEY_ID 0xA403
#define JSON_MAX (2u * 1024u * 1024u)
#define TOKEN_MAX 65536
#define MENU_MAX 32
#define CMD_SHOW 1
#define CMD_CLEAN 2
#define CMD_SETTINGS 3
#define CMD_HISTORY 4
#define CMD_ABOUT 5
#define CMD_EXIT 6
#define CMD_TOGGLE 7
#define CMD_REGION 100

static const char *region_keys[] = {"GlobalWorkingSets", "SystemFileCache", "ModifiedPageList", "FullStandby", "LowPriorityStandby", "ModifiedFileCache", "RegistryCache", "CombineMemory", "BackgroundWorkingSets"};
static const char *region_labels[] = {"Conjunto de trabalho (todos os processos)", "Cache de arquivos do sistema", "Lista de páginas modificadas *", "Lista de espera completa *", "Lista de espera de baixa prioridade", "Cache de arquivos modificados", "Cache do Registro (Windows 8.1+)", "Combinação de páginas de memória (Windows 10+)", "Conjuntos de trabalho em segundo plano"};
/* New installations start conservatively: no automatic cache purges or wide process trims. */
static const int default_regions[] = {0,0,0,0,1,0,0,0,0};

/* Small validating JSON reader.  Tokens retain offsets into the original UTF-8
   document so an edit preserves unknown properties and their original values. */
typedef struct { int type, start, end, next; } Token;
typedef struct { const char *text; size_t length, pos; Token *tokens; int count, capacity, error; } Json;
enum { J_OBJECT=1, J_ARRAY, J_STRING, J_NUMBER, J_TRUE, J_FALSE, J_NULL };
static void whitespace(Json *j) { while (j->pos < j->length && (j->text[j->pos]==' ' || j->text[j->pos]=='\r' || j->text[j->pos]=='\n' || j->text[j->pos]=='\t')) ++j->pos; }
static int hex4(const char *p) { int v=0; for(int i=0;i<4;++i) { int h=p[i]>='0'&&p[i]<='9'?p[i]-'0':p[i]>='a'&&p[i]<='f'?p[i]-'a'+10:p[i]>='A'&&p[i]<='F'?p[i]-'A'+10:-1; if(h<0)return -1; v=(v<<4)|h; } return v; }
static int json_token(Json *j){if(j->count>=TOKEN_MAX){j->error=1;return -1;}if(j->count==j->capacity){int capacity=j->capacity?j->capacity*2:128;if(capacity>TOKEN_MAX)capacity=TOKEN_MAX;Token *grown=(Token*)realloc(j->tokens,(size_t)capacity*sizeof(Token));if(!grown){j->error=1;return -1;}j->tokens=grown;j->capacity=capacity;}int n=j->count++;memset(&j->tokens[n],0,sizeof(Token));return n;}
static int parse_value(Json *j, int depth);
static int parse_string(Json *j) {
    if(j->count>=TOKEN_MAX || j->pos>=j->length || j->text[j->pos]!='"') {j->error=1;return -1;}
    int n=json_token(j);if(n<0)return -1;Token *t=&j->tokens[n]; t->type=J_STRING; t->start=(int)++j->pos;
    while(j->pos<j->length) {
        unsigned char c=(unsigned char)j->text[j->pos++];
        if(c=='"') {t->end=(int)j->pos-1;t->next=j->count;return n;}
        if(c<0x20)break;
        if(c=='\\') {if(j->pos>=j->length)break; char e=j->text[j->pos++]; if(e=='u') {if(j->pos+4>j->length || hex4(j->text+j->pos)<0)break; j->pos+=4;} else if(!strchr("\"\\/bfnrt",e))break;}
    }
    j->error=1;return -1;
}
static int parse_value(Json *j, int depth) {
    whitespace(j); if(j->error || depth>48 || j->pos>=j->length || j->count>=TOKEN_MAX) {j->error=1;return -1;}
    char c=j->text[j->pos]; if(c=='"')return parse_string(j);
    int n=json_token(j);if(n<0)return -1;Token *t=&j->tokens[n]; t->start=(int)j->pos;
    if(c=='{' || c=='[') {
        t->type=c=='{'?J_OBJECT:J_ARRAY; ++j->pos; whitespace(j); char closing=c=='{'?'}':']';
        if(j->pos<j->length && j->text[j->pos]==closing)++j->pos;
        else for(;;) {
            if(c=='{') {whitespace(j); if(parse_string(j)<0)break; whitespace(j); if(j->pos>=j->length || j->text[j->pos++]!=':') {j->error=1;break;}}
            if(parse_value(j,depth+1)<0)break; whitespace(j);
            if(j->pos>=j->length) {j->error=1;break;}
            char sep=j->text[j->pos++]; if(sep==closing)break; if(sep!=',') {j->error=1;break;}
        }
    } else {
        size_t start=j->pos;
        while(j->pos<j->length && !strchr(" \r\n\t,]}",j->text[j->pos]))++j->pos;
        size_t len=j->pos-start;
        if(len==4 && !memcmp(j->text+start,"true",4))t->type=J_TRUE;
        else if(len==5 && !memcmp(j->text+start,"false",5))t->type=J_FALSE;
        else if(len==4 && !memcmp(j->text+start,"null",4))t->type=J_NULL;
        else {
            size_t p=start; if(p<j->pos&&j->text[p]=='-')++p;
            if(p>=j->pos)j->error=1;
            else if(j->text[p]=='0')++p;
            else if(j->text[p]>='1'&&j->text[p]<='9') {while(p<j->pos&&j->text[p]>='0'&&j->text[p]<='9')++p;}
            else j->error=1;
            if(p<j->pos&&j->text[p]=='.') {size_t q=++p;while(p<j->pos&&j->text[p]>='0'&&j->text[p]<='9')++p;if(p==q)j->error=1;}
            if(p<j->pos&&(j->text[p]=='e'||j->text[p]=='E')) {++p;if(p<j->pos&&(j->text[p]=='+'||j->text[p]=='-'))++p;size_t q=p;while(p<j->pos&&j->text[p]>='0'&&j->text[p]<='9')++p;if(p==q)j->error=1;}
            if(p!=j->pos)j->error=1;t->type=J_NUMBER;
        }
    }
    t=&j->tokens[n];t->end=(int)j->pos;t->next=j->count;return j->error?-1:n;
}
static int json_init(Json *j, const char *text, size_t length) {
    memset(j,0,sizeof(*j));j->text=text;j->length=length;j->capacity=128;j->tokens=(Token*)calloc((size_t)j->capacity,sizeof(Token));if(!j->tokens)return 0;
    if(length>=3 && (unsigned char)text[0]==0xEF && (unsigned char)text[1]==0xBB && (unsigned char)text[2]==0xBF)j->pos=3;
    int root=parse_value(j,0);whitespace(j);return root==0&&!j->error&&j->pos==length&&j->tokens[0].type==J_OBJECT;
}
static void json_free(Json *j) {free(j->tokens);j->tokens=NULL;}
static int utf8_emit(unsigned v,char *out,int cap,int *at) {
    int n=v<0x80?1:v<0x800?2:v<0x10000?3:4;if(*at+n>=cap)return 0;
    if(n==1)out[(*at)++]=(char)v;
    else {if(n==2)out[(*at)++]=(char)(0xC0|(v>>6));else if(n==3)out[(*at)++]=(char)(0xE0|(v>>12));else out[(*at)++]=(char)(0xF0|(v>>18));if(n>=3)out[(*at)++]=(char)(0x80|((v>>(n==4?12:6))&63));if(n==4)out[(*at)++]=(char)(0x80|((v>>6)&63));out[(*at)++]=(char)(0x80|(v&63));}return 1;
}
static int json_text(Json *j,int n,char *out,int cap) {
    if(n<0||n>=j->count||j->tokens[n].type!=J_STRING||cap<1)return 0;Token t=j->tokens[n];int at=0;
    for(int p=t.start;p<t.end;++p) {
        unsigned char c=(unsigned char)j->text[p];
        if(c!='\\') {if(at+1>=cap)return 0;out[at++]=(char)c;continue;}
        c=(unsigned char)j->text[++p];
        if(c=='u') {unsigned v=(unsigned)hex4(j->text+p+1);p+=4;if(v>=0xD800&&v<=0xDBFF){if(p+6>=t.end||j->text[p+1]!='\\'||j->text[p+2]!='u')return 0;int low=hex4(j->text+p+3);if(low<0xDC00||low>0xDFFF)return 0;v=0x10000+((v-0xD800)<<10)+(low-0xDC00);p+=6;}else if(v>=0xDC00&&v<=0xDFFF)return 0;if(!utf8_emit(v,out,cap,&at))return 0;}
        else {if(c=='b')c='\b';else if(c=='f')c='\f';else if(c=='n')c='\n';else if(c=='r')c='\r';else if(c=='t')c='\t';if(at+1>=cap)return 0;out[at++]=(char)c;}
    }
    out[at]=0;return 1;
}
static int json_find(Json *j,const char *key) {
    char decoded[1024]; if(!j->tokens||!j->count)return -1;
    for(int n=1;n<j->tokens[0].next;) {int value=n+1;if(value>=j->count)return -1;if(json_text(j,n,decoded,sizeof(decoded))&&!strcmp(decoded,key))return value;n=j->tokens[value].next;}return -1;
}
static int json_bool(Json *j,const char *key,int fallback) {int n=json_find(j,key);return n>=0&&j->tokens[n].type==J_TRUE?1:n>=0&&j->tokens[n].type==J_FALSE?0:fallback;}
static double json_number(Json *j,const char *key,double fallback) {int n=json_find(j,key);if(n<0||j->tokens[n].type!=J_NUMBER)return fallback;double v=strtod(j->text+j->tokens[n].start,NULL);return isfinite(v)?v:fallback;}
static int json_wide(Json *j,const char *key,wchar_t *out,int cap) {char decoded[4096];int n=json_find(j,key);if(!json_text(j,n,decoded,sizeof(decoded)))return 0;return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,decoded,-1,out,cap)>0;}
static char *json_replace(const char *document,const char *key,const char *raw) {
    Json j;if(!json_init(&j,document,strlen(document))) {json_free(&j);return NULL;}int n=json_find(&j,key);size_t a,b;char insert[256];const char *replacement=raw;
    if(n>=0) {a=(size_t)j.tokens[n].start;b=(size_t)j.tokens[n].end;if(j.tokens[n].type==J_STRING){--a;++b;}}
    else {a=b=(size_t)j.tokens[0].end-1;snprintf(insert,sizeof(insert),"%s\n  \"%s\": %s\n",j.tokens[0].next>1?",":"",key,raw);replacement=insert;}
    size_t len=strlen(document),rlen=strlen(replacement);char *next=(char*)malloc(len-(b-a)+rlen+1);if(next){memcpy(next,document,a);memcpy(next+a,replacement,rlen);memcpy(next+a+rlen,document+b,len-b+1);}json_free(&j);return next;
}

typedef struct {
    int dark,window_blur,transparent,border,rounded,color_background,antialias,bold,warning,danger,regions[9],enable_hotkey,click,double_click,notifications,sound,skip_uac,show_graph,always_on_top,close_to_tray,start_minimized,refresh_seconds;
    int auto_threshold_enabled,auto_threshold_percent,auto_interval_enabled,auto_interval_minutes,allow_aggressive_auto,minimum_cooldown_minutes;
    UINT hotkey_modifiers,hotkey_key;
    float font_size;
    COLORREF text_color,background_color,warning_color,danger_color;
    wchar_t font[LF_FACESIZE],language[32];
} Settings;
typedef struct {uint64_t total,available,commit_limit,commit_used,cache;DWORD processes;double physical_percent,commit_percent,cache_percent;} NativeMemory;
static NativeMemory g_memory;
static double g_graph_values[90];static int g_graph_count;
static HWND g_main_hwnd;
static void native_main_show(void);static void native_main_toggle(void);static void native_main_refresh(void);static void native_main_sample_changed(void);
static void native_settings_show(HWND);static void native_settings_refresh(void);static void native_settings_close(void);static int native_settings_message(MSG*);
static void native_request_cleanup(HWND);static void native_open_history(HWND);static void native_open_about(HWND);static void native_actions_tick(void);static void native_actions_close(void);static int native_actions_command_line(int,wchar_t**,int*);static int native_actions_self_test(void);static void native_test_confirmation(HWND);
static void native_automation_tick(void);static void native_automation_preferences_changed(void);static int native_automation_self_test(void);static void native_automation_cleanup_completed(int);
static void native_popup_open(HWND,int,POINT);static void native_preferences_changed(void);static void native_cleanup_completed(int,const wchar_t*);static int process_elevated(void);static int native_authorization_busy(void);static int check(int,const char*);
static HWND g_hwnd;static HICON g_icon;static HANDLE g_mutex;static HINSTANCE g_instance;static UINT g_taskbar_created;static Settings g_settings;
static wchar_t g_folder[MAX_PATH],g_settings_path[MAX_PATH],g_data_dir[MAX_PATH];
static int g_test_mode,g_authorized,g_busy,g_exit_pending,g_registered,g_skip_up,g_percent=-1,g_icon_size=16,g_settings_loaded,g_hotkey_registered;
static ULONGLONG g_last_memory_tick;static int g_last_tooltip_milli=-1;static ULONGLONG g_last_tooltip_tick;
static FILETIME g_stamp;static DWORD g_stamp_size;static volatile LONG g_settings_write_sequence;static char *g_document;static char *g_language_doc;static Json g_language_json;static UINT g_dpi=96;
static int clampi(int value,int low,int high){return value<low?low:value>high?high:value;}
static void copyw(wchar_t *to,size_t cap,const wchar_t *from){if(!cap)return;wcsncpy(to,from,cap-1);to[cap-1]=0;}
static int path_join(wchar_t *to,size_t cap,const wchar_t *folder,const wchar_t *name){return swprintf(to,cap,L"%ls\\%ls",folder,name)>0;}
static COLORREF color_from(const wchar_t *text,COLORREF fallback){unsigned r,g,b;if(text[0]!='#'||wcslen(text)!=7||swscanf(text+1,L"%2x%2x%2x",&r,&g,&b)!=3)return fallback;return RGB(r,g,b);}
static COLORREF json_color(Json *j,const char *key,COLORREF fallback){wchar_t text[32];return json_wide(j,key,text,32)?color_from(text,fallback):fallback;}
/* Resolve Windows preferred display language to one of Orion Memory's
 * bundled locales. All results are ASCII BCP-47 codes supported by the app.
 * Manual selections in an existing settings.json are never replaced. */
static int locale_prefix(const wchar_t *tag,const wchar_t *prefix){
 unsigned long i=0;while(prefix[i]){
  wchar_t x=tag[i],y=prefix[i];
  if(!x)return 0;
  if(x>=L'A'&&x<=L'Z')x+=32;
  if(y>=L'A'&&y<=L'Z')y+=32;
  if(x!=y)return 0;
  ++i;
 }
 return tag[i]==0||tag[i]==L'-';
}
static const wchar_t *mapped_locale(const wchar_t *tag){
 static const wchar_t *codes[]={
  L"pt-BR",L"pt-PT",L"en-US",L"es-ES",L"ar-SA",L"bg-BG",L"ca-ES",
  L"zh-CN",L"zh-TW",L"cs-CZ",L"nl-NL",L"fr-FR",L"de-DE",
  L"he-IL",L"hu-HU",L"id-ID",L"it-IT",L"ja-JP",L"kk-KZ",
  L"ko-KR",L"fa-IR",L"pl-PL",L"ro-RO",L"ru-RU",
  L"sr-Cyrl-RS",L"sk-SK",L"sv-SE",L"tr-TR",L"uk-UA",L"vi-VN"
 };
 if(!tag||!tag[0])return NULL;
 for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i)
  if(_wcsicmp(tag,codes[i])==0)return codes[i];
 if(locale_prefix(tag,L"zh")){
  if(locale_prefix(tag,L"zh-Hant")||locale_prefix(tag,L"zh-HK")||locale_prefix(tag,L"zh-MO"))return L"zh-TW";
  return L"zh-CN";
 }
 if(locale_prefix(tag,L"sr-Cyrl"))return L"sr-Cyrl-RS";
 if(locale_prefix(tag,L"sr"))return NULL; /* Do not present Cyrillic as Latin Serbian. */
 if(locale_prefix(tag,L"pt-PT")||locale_prefix(tag,L"pt-AO")||locale_prefix(tag,L"pt-MZ"))return L"pt-PT";
 if(locale_prefix(tag,L"pt"))return L"pt-BR";
 static const struct {const wchar_t *language,*code;} fallback[]={
  {L"en",L"en-US"},{L"es",L"es-ES"},{L"ar",L"ar-SA"},{L"bg",L"bg-BG"},
  {L"ca",L"ca-ES"},{L"cs",L"cs-CZ"},{L"nl",L"nl-NL"},
  {L"fr",L"fr-FR"},{L"de",L"de-DE"},{L"he",L"he-IL"},
  {L"hu",L"hu-HU"},{L"id",L"id-ID"},{L"it",L"it-IT"},
  {L"ja",L"ja-JP"},{L"kk",L"kk-KZ"},{L"ko",L"ko-KR"},
  {L"fa",L"fa-IR"},{L"pl",L"pl-PL"},{L"ro",L"ro-RO"},
  {L"ru",L"ru-RU"},{L"sk",L"sk-SK"},{L"sv",L"sv-SE"},
  {L"tr",L"tr-TR"},{L"uk",L"uk-UA"},{L"vi",L"vi-VN"}
 };
 for(unsigned i=0;i<sizeof(fallback)/sizeof(fallback[0]);++i)
  if(locale_prefix(tag,fallback[i].language))return fallback[i].code;
 return NULL;
}
static const wchar_t *windows_ui_locale(void){
 wchar_t languages[1024];DWORD count=0,capacity=1024;
 if(GetUserPreferredUILanguages(0x8,&count,languages,&capacity)){
  unsigned long at=0;
  for(DWORD i=0;i<count && at<1024 && languages[at];++i){
   const wchar_t *result=mapped_locale(languages+at);
   if(result)return result;
   at+=wcslen(languages+at)+1;
  }
 }
 /* Last resort when the user preferred UI-language API is unavailable. */
 wchar_t locale[85];
 if(GetUserDefaultLocaleName(locale,85)>0){
  const wchar_t *result=mapped_locale(locale);
  if(result)return result;
 }
 return L"pt-BR"; /* Same safe fallback as the previous releases. */
}

static void default_settings(Settings *s){memset(s,0,sizeof(*s));s->dark=s->rounded=s->antialias=s->bold=s->enable_hotkey=s->notifications=s->sound=1;s->skip_uac=1;s->font_size=8;s->refresh_seconds=1;s->auto_threshold_percent=88;s->auto_interval_minutes=30;s->minimum_cooldown_minutes=15;s->warning=75;s->close_to_tray=1;s->danger=90;s->double_click=1;s->hotkey_modifiers=3;s->hotkey_key='M';s->text_color=RGB(111,181,255);s->background_color=RGB(15,28,50);s->warning_color=RGB(249,194,86);s->danger_color=RGB(255,127,123);memcpy(s->regions,default_regions,sizeof(default_regions));copyw(s->font,LF_FACESIZE,L"Segoe UI");copyw(s->language,32,windows_ui_locale());}
static void parse_settings(Settings *s,Json *j){default_settings(s);s->refresh_seconds=clampi((int)json_number(j,"RefreshSeconds",1),1,5);s->show_graph=json_bool(j,"ShowGraph",0);s->always_on_top=json_bool(j,"AlwaysOnTop",0);s->close_to_tray=json_bool(j,"CloseToTray",1);s->start_minimized=json_bool(j,"StartMinimized",0);s->auto_threshold_enabled=0;s->auto_threshold_percent=clampi((int)json_number(j,"AutoThresholdPercent",json_number(j,"AutoThreshold",88)),1,99);s->auto_interval_enabled=0;s->auto_interval_minutes=clampi((int)json_number(j,"AutoIntervalMinutes",30),1,240);s->allow_aggressive_auto=0;s->minimum_cooldown_minutes=clampi((int)json_number(j,"MinimumCooldownMinutes",15),1,120);
    s->skip_uac=json_bool(j,"SkipUac",1);s->dark=json_bool(j,"DarkTheme",s->dark);s->window_blur=json_bool(j,"WindowBlur",0);s->transparent=json_bool(j,"TrayTransparent",s->transparent);s->border=json_bool(j,"TrayBorder",s->border);s->rounded=json_bool(j,"TrayRounded",s->rounded);s->color_background=json_bool(j,"TrayColorBackground",s->color_background);s->antialias=json_bool(j,"TrayAntialias",s->antialias);s->bold=json_bool(j,"TrayFontBold",s->bold);
    s->warning=clampi((int)json_number(j,"WarningPercent",75),1,98);s->danger=clampi((int)json_number(j,"DangerPercent",90),s->warning+1,100);s->font_size=(float)fmax(1,fmin(18,json_number(j,"TrayFontSize",8)));if(json_number(j,"NativeAppearanceRevision",0)<20)s->font_size=8;
    s->text_color=json_color(j,"TrayTextColor",s->text_color);s->background_color=json_color(j,"TrayBackgroundColor",s->background_color);s->warning_color=json_color(j,"TrayWarningColor",s->warning_color);s->danger_color=json_color(j,"TrayDangerColor",s->danger_color);json_wide(j,"TrayFontFamily",s->font,LF_FACESIZE);json_wide(j,"Language",s->language,32);
    if(wcspbrk(s->language,L"\\/.: ")||!s->language[0])copyw(s->language,32,L"pt-BR");
    s->enable_hotkey=json_bool(j,"EnableHotkey",1);s->click=clampi((int)json_number(j,"TraySingleClick",0),0,2);s->double_click=clampi((int)json_number(j,"TrayDoubleClick",1),0,2);s->notifications=json_bool(j,"ShowNotifications",1);s->sound=json_bool(j,"NotificationSound",1);s->hotkey_modifiers=(UINT)json_number(j,"HotkeyModifiers",3)&15;s->hotkey_key=(UINT)json_number(j,"HotkeyKey",'M');if(!s->hotkey_modifiers||s->hotkey_key<0x20||s->hotkey_key>0xFE){s->hotkey_modifiers=3;s->hotkey_key='M';}
    for(int i=0;i<9;++i)s->regions[i]=json_bool(j,region_keys[i],default_regions[i]);
    int profile=json_find(j,"Profile");if(profile>=0&&j->tokens[profile].type==J_NUMBER&&(int)json_number(j,"Profile",2)!=2){memset(s->regions,0,sizeof(s->regions));if((int)json_number(j,"Profile",0)==1)s->regions[3]=1;else s->regions[4]=1;}
}
static char *read_utf8(const wchar_t *path){HANDLE h=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(h==INVALID_HANDLE_VALUE)return NULL;LARGE_INTEGER size;if(!GetFileSizeEx(h,&size)||size.QuadPart<2||size.QuadPart>JSON_MAX){CloseHandle(h);return NULL;}char *text=(char*)malloc((size_t)size.QuadPart+1);DWORD read=0;if(!text||!ReadFile(h,text,(DWORD)size.QuadPart,&read,NULL)||read!=(DWORD)size.QuadPart){free(text);text=NULL;}else text[read]=0;CloseHandle(h);return text;}
static void load_language(void){json_free(&g_language_json);free(g_language_doc);g_language_doc=NULL;/* The native source strings are Brazilian Portuguese; avoid retaining an identical dictionary and token tree. */if(!wcscmp(g_settings.language,L"pt-BR"))return;wchar_t sub[96],path[MAX_PATH];swprintf(sub,96,L"Languages\\%ls.json",g_settings.language);path_join(path,MAX_PATH,g_folder,sub);g_language_doc=read_utf8(path);if(g_language_doc&&!json_init(&g_language_json,g_language_doc,strlen(g_language_doc))){json_free(&g_language_json);free(g_language_doc);g_language_doc=NULL;}}
static void translate(const char *text,wchar_t *out,int cap){if(g_language_doc&&json_wide(&g_language_json,text,out,cap))return;MultiByteToWideChar(CP_UTF8,0,text,-1,out,cap);out[cap-1]=0;}
static void bind_hotkey(void){if(!g_hwnd||g_test_mode)return;if(g_hotkey_registered){UnregisterHotKey(g_hwnd,HOTKEY_ID);g_hotkey_registered=0;}if(g_settings.enable_hotkey)g_hotkey_registered=RegisterHotKey(g_hwnd,HOTKEY_ID,g_settings.hotkey_modifiers|MOD_NOREPEAT,g_settings.hotkey_key);}
static int atomic_settings_write(const char *doc);
static int load_settings(int force){WIN32_FILE_ATTRIBUTE_DATA attrs;int exists=GetFileAttributesExW(g_settings_path,GetFileExInfoStandard,&attrs);if(!force&&g_settings_loaded&&exists&&CompareFileTime(&attrs.ftLastWriteTime,&g_stamp)==0&&attrs.nFileSizeLow==g_stamp_size)return 0;if(!force&&g_settings_loaded&&!exists&&g_stamp_size==0)return 0;
    char *doc=read_utf8(g_settings_path);Json parsed;int valid=doc&&json_init(&parsed,doc,strlen(doc));if(!valid){if(doc)json_free(&parsed);free(doc);wchar_t backup[MAX_PATH];swprintf(backup,MAX_PATH,L"%ls.bak",g_settings_path);doc=read_utf8(backup);valid=doc&&json_init(&parsed,doc,strlen(doc));}
    if(!valid){if(doc)json_free(&parsed);free(doc);if(g_settings_loaded)return 0;doc=_strdup("{}");json_init(&parsed,doc,2);}
    if(json_number(&parsed,"NativeAppearanceRevision",0)<20){char *font=json_replace(doc,"TrayFontSize","8"),*revision=font?json_replace(font,"NativeAppearanceRevision","20"):NULL;free(font);if(revision&&atomic_settings_write(revision)){json_free(&parsed);free(doc);doc=revision;json_init(&parsed,doc,strlen(doc));exists=GetFileAttributesExW(g_settings_path,GetFileExInfoStandard,&attrs);}else free(revision);}
    Settings next;parse_settings(&next,&parsed);json_free(&parsed);int changed=!g_settings_loaded||memcmp(&next,&g_settings,sizeof(next));int language_changed=!g_settings_loaded||wcscmp(next.language,g_settings.language);g_settings=next;free(g_document);g_document=doc;g_settings_loaded=1;if(exists){g_stamp=attrs.ftLastWriteTime;g_stamp_size=attrs.nFileSizeLow;}else{memset(&g_stamp,0,sizeof(g_stamp));g_stamp_size=0;}if(language_changed)load_language();if(changed){g_percent=-1;bind_hotkey();}return changed;}
/* Keep preferences atomic without generating a permanent .bak on every click.
   The temporary file is flushed before ReplaceFileW; on failure the original
   remains readable. The temporary file is removed on every failure path. */
static int atomic_settings_write(const char *doc){
    if(!doc)return 0;
    size_t len=strlen(doc);
    if(len<2||len>JSON_MAX||len>UINT32_MAX)return 0;
    wchar_t temporary[MAX_PATH];
    int count=swprintf(temporary,MAX_PATH,L"%ls.%lu.%lu.%lu.tmp",
        g_settings_path,GetCurrentProcessId(),GetTickCount(),(DWORD)InterlockedIncrement(&g_settings_write_sequence));
    if(count<=0||count>=MAX_PATH)return 0;
    SHCreateDirectoryExW(NULL,g_data_dir,NULL);
    HANDLE h=CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE)return 0;
    DWORD written=0;
    int ok=WriteFile(h,doc,(DWORD)len,&written,NULL)&&written==(DWORD)len&&FlushFileBuffers(h);
    CloseHandle(h);
    if(ok){
        if(GetFileAttributesW(g_settings_path)!=INVALID_FILE_ATTRIBUTES)
            ok=ReplaceFileW(g_settings_path,temporary,NULL,REPLACEFILE_IGNORE_MERGE_ERRORS,NULL,NULL);
        else ok=MoveFileExW(temporary,g_settings_path,MOVEFILE_WRITE_THROUGH);
    }
    if(!ok)DeleteFileW(temporary);
    return ok;
}
static void signal_reload(void);
static int toggle_region(int index){if(index<0||index>=9)return 0;load_settings(1);int count=0;for(int i=0;i<9;++i)count+=g_settings.regions[i];if(g_settings.regions[index]&&count<=1)return 0;char *doc=_strdup(g_document?g_document:"{}");if(!doc)return 0;
    /* Materialize effective legacy-profile regions before retiring the old flag. */
    for(int i=0;i<9;++i){char *next=json_replace(doc,region_keys[i],(i==index?!g_settings.regions[i]:g_settings.regions[i])?"true":"false");free(doc);if(!(doc=next))return 0;}
    Json j;if(json_init(&j,doc,strlen(doc))&&json_find(&j,"Profile")>=0){char *next=json_replace(doc,"Profile","null");free(doc);doc=next;}json_free(&j);if(!doc)return 0;int ok=atomic_settings_write(doc);free(doc);if(ok){load_settings(1);signal_reload();}return ok;}

/* Native tray bitmap: derive real glyph ink bounds and fit those bounds instead
   of shrinking a padded 64px icon.  The DIB uses premultiplied alpha. */
static HICON render_icon(int value,int size){
    const int mask_w=128,mask_h=80;BITMAPINFO mask_info;memset(&mask_info,0,sizeof(mask_info));mask_info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);mask_info.bmiHeader.biWidth=mask_w;mask_info.bmiHeader.biHeight=-mask_h;mask_info.bmiHeader.biPlanes=1;mask_info.bmiHeader.biBitCount=32;mask_info.bmiHeader.biCompression=BI_RGB;
    HDC screen=GetDC(NULL),mask_dc=CreateCompatibleDC(screen);uint32_t *mask=NULL;HBITMAP mask_bitmap=CreateDIBSection(screen,&mask_info,DIB_RGB_COLORS,(void**)&mask,NULL,0);if(!mask_dc||!mask_bitmap||!mask){if(mask_bitmap)DeleteObject(mask_bitmap);if(mask_dc)DeleteDC(mask_dc);ReleaseDC(NULL,screen);return NULL;}memset(mask,0,mask_w*mask_h*sizeof(uint32_t));HGDIOBJ old_bitmap=SelectObject(mask_dc,mask_bitmap);HFONT font=CreateFontW(-56,0,0,0,g_settings.bold?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,g_settings.antialias?ANTIALIASED_QUALITY:NONANTIALIASED_QUALITY,DEFAULT_PITCH,g_settings.font);HGDIOBJ old_font=SelectObject(mask_dc,font);SetTextColor(mask_dc,RGB(255,255,255));SetBkColor(mask_dc,RGB(0,0,0));SetBkMode(mask_dc,OPAQUE);wchar_t digits[8];swprintf(digits,8,L"%d",clampi(value,0,100));TextOutW(mask_dc,3,0,digits,(int)wcslen(digits));GdiFlush();
    int left=mask_w,top=mask_h,right=-1,bottom=-1;for(int y=0;y<mask_h;++y)for(int x=0;x<mask_w;++x)if((mask[y*mask_w+x]&0xFFFFFF)!=0){if(x<left)left=x;if(x>right)right=x;if(y<top)top=y;if(y>bottom)bottom=y;}
    BITMAPINFO output_info=mask_info;output_info.bmiHeader.biWidth=size;output_info.bmiHeader.biHeight=-size;uint32_t *pixels=NULL;HBITMAP bitmap=CreateDIBSection(screen,&output_info,DIB_RGB_COLORS,(void**)&pixels,NULL,0);HICON icon=NULL;
    if(bitmap&&pixels&&right>=left){
        double inset=g_settings.border?fmax(1.5,size/16.0+0.5):0.5,available=size-2*inset;double scale=fmin(available/(right-left+1),fmin(available,g_settings.font_size*size/16.0)/(bottom-top+1));double width=(right-left+1)*scale,height=(bottom-top+1)*scale,x0=(size-width)/2.0,y0=(size-height)/2.0;COLORREF status=value>=g_settings.danger?g_settings.danger_color:value>=g_settings.warning?g_settings.warning_color:g_settings.text_color;int color_background=g_settings.color_background&&!g_settings.transparent;COLORREF text=color_background?g_settings.text_color:status,background=color_background&&value>=g_settings.warning?status:g_settings.background_color;
        double radius=g_settings.rounded?size*.1875:0;for(int y=0;y<size;++y)for(int x=0;x<size;++x){double coverage=0;for(int sy=0;sy<4;++sy)for(int sx=0;sx<4;++sx){double dx=x+(sx+.5)/4,dy=y+(sy+.5)/4;if(dx>=x0&&dx<x0+width&&dy>=y0&&dy<y0+height){int mx=clampi((int)(left+(dx-x0)/scale),left,right),my=clampi((int)(top+(dy-y0)/scale),top,bottom);coverage+=(mask[my*mask_w+mx]&255)/255.0/16;}}
            if(!g_settings.antialias)coverage=coverage>=.5?1:0;double shape=1;double px=x+.5,py=y+.5;if(radius>0){double cx=px<radius?radius:px>size-radius?size-radius:px,cy=py<radius?radius:py>size-radius?size-radius:py;shape=(px-cx)*(px-cx)+(py-cy)*(py-cy)<=radius*radius;}
            int edge=g_settings.border&&shape&&(x==0||y==0||x==size-1||y==size-1);if(edge)coverage=1;double alpha=g_settings.transparent?coverage:shape?1:coverage;int r=(int)lround(GetRValue(text)*coverage+(!g_settings.transparent&&shape?GetRValue(background)*(1-coverage):0)),g=(int)lround(GetGValue(text)*coverage+(!g_settings.transparent&&shape?GetGValue(background)*(1-coverage):0)),b=(int)lround(GetBValue(text)*coverage+(!g_settings.transparent&&shape?GetBValue(background)*(1-coverage):0));pixels[y*size+x]=((uint32_t)lround(alpha*255)<<24)|((uint32_t)clampi(r,0,255)<<16)|((uint32_t)clampi(g,0,255)<<8)|clampi(b,0,255);
        }
        size_t mask_bytes=(size_t)((size+15)/16)*2*size;void *zero=calloc(1,mask_bytes);HBITMAP mono=zero?CreateBitmap(size,size,1,1,zero):NULL;free(zero);if(mono){ICONINFO info;memset(&info,0,sizeof(info));info.fIcon=TRUE;info.hbmColor=bitmap;info.hbmMask=mono;icon=CreateIconIndirect(&info);DeleteObject(mono);}
    }
    if(bitmap)DeleteObject(bitmap);SelectObject(mask_dc,old_font);if(font)DeleteObject(font);SelectObject(mask_dc,old_bitmap);DeleteObject(mask_bitmap);DeleteDC(mask_dc);ReleaseDC(NULL,screen);return icon;
}
static void update_dpi(void){HWND taskbar=FindWindowW(L"Shell_TrayWnd",NULL);UINT dpi=taskbar?GetDpiForWindow(taskbar):96;if(!dpi)dpi=96;int size=clampi(GetSystemMetricsForDpi(SM_CXSMICON,dpi),16,128);if(size!=g_icon_size||dpi!=g_dpi){g_icon_size=size;g_dpi=dpi;g_percent=-1;}}
static NOTIFYICONDATAW tray_data(void){NOTIFYICONDATAW data;memset(&data,0,sizeof(data));data.cbSize=sizeof(data);data.hWnd=g_hwnd;data.uID=1;data.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;data.uCallbackMessage=WM_TRAY;data.hIcon=g_icon;swprintf(data.szTip,128,L"Orion Memory • RAM %.3f%%",g_memory.physical_percent);for(wchar_t *p=data.szTip;*p;++p)if(*p==L'.')*p=L',';return data;}
static void update_memory(void){g_last_memory_tick=GetTickCount64();update_dpi();MEMORYSTATUSEX memory;memset(&memory,0,sizeof(memory));memory.dwLength=sizeof(memory);if(!GlobalMemoryStatusEx(&memory))return;g_memory.total=memory.ullTotalPhys;g_memory.available=memory.ullAvailPhys;g_memory.physical_percent=memory.ullTotalPhys?100.0*(double)(memory.ullTotalPhys-memory.ullAvailPhys)/memory.ullTotalPhys:0;PERFORMANCE_INFORMATION performance;memset(&performance,0,sizeof(performance));performance.cb=sizeof(performance);if(g_main_hwnd&&GetPerformanceInfo(&performance,sizeof(performance))){g_memory.commit_limit=(uint64_t)performance.CommitLimit*performance.PageSize;g_memory.commit_used=(uint64_t)performance.CommitTotal*performance.PageSize;g_memory.cache=(uint64_t)performance.SystemCache*performance.PageSize;g_memory.processes=performance.ProcessCount;g_memory.commit_percent=g_memory.commit_limit?100.0*g_memory.commit_used/g_memory.commit_limit:0;g_memory.cache_percent=g_memory.total?100.0*g_memory.cache/g_memory.total:0;}if(g_graph_count==90){memmove(g_graph_values,g_graph_values+1,89*sizeof(double));g_graph_count=89;}g_graph_values[g_graph_count++]=g_memory.physical_percent;native_main_sample_changed();int percent=clampi((int)lround(g_memory.physical_percent),0,100);if(percent==g_percent&&g_icon&&g_registered){int milli=(int)lround(g_memory.physical_percent*1000);if(milli!=g_last_tooltip_milli&&GetTickCount64()-g_last_tooltip_tick>=5000ull){NOTIFYICONDATAW tip=tray_data();tip.uFlags=NIF_TIP;if(Shell_NotifyIconW(NIM_MODIFY,&tip)){g_last_tooltip_milli=milli;g_last_tooltip_tick=GetTickCount64();}else g_registered=0;}return;}HICON next=render_icon(percent,g_icon_size);if(!next)return;HICON old=g_icon;g_icon=next;g_percent=percent;NOTIFYICONDATAW data=tray_data();swprintf(data.szTip,128,L"Orion Memory • RAM %.3f%%",g_memory.physical_percent);for(wchar_t *p=data.szTip;*p;++p)if(*p==L'.')*p=L',';g_registered=Shell_NotifyIconW(NIM_MODIFY,&data)||Shell_NotifyIconW(NIM_ADD,&data);if(g_registered){g_last_tooltip_milli=(int)lround(g_memory.physical_percent*1000);g_last_tooltip_tick=GetTickCount64();}if(old)DestroyIcon(old);}
static void show_error(const char *message){if(g_test_mode)return;wchar_t text[768];translate(message,text,768);MessageBoxW(NULL,text,L"Orion Memory",MB_ICONERROR|MB_OK);}
static void signal_reload(void){if(g_test_mode)return;HANDLE event=OpenEventW(EVENT_MODIFY_STATE,FALSE,L"Local\\OrionMemory.Reload");if(event){SetEvent(event);CloseHandle(event);}}
static void signal_exit_ui(void){if(g_test_mode)return;HANDLE event=OpenEventW(EVENT_MODIFY_STATE,FALSE,L"Local\\OrionMemory.Exit");if(event){SetEvent(event);CloseHandle(event);}}
static void request_ui(int command){switch(command){case CMD_SHOW:native_main_show();break;case CMD_TOGGLE:native_main_toggle();break;case CMD_SETTINGS:native_settings_show(g_main_hwnd);break;case CMD_HISTORY:native_open_history(g_main_hwnd);break;case CMD_ABOUT:native_open_about(g_main_hwnd);break;case CMD_CLEAN:if(!g_test_mode&&!native_authorization_busy())native_request_cleanup(g_main_hwnd);break;case CMD_EXIT:if(g_busy||native_authorization_busy()){g_exit_pending=1;return;}PostMessageW(g_hwnd,WM_CLOSE,0,0);break;default:break;}}
static void tray_action(int action){if(action==2)request_ui(CMD_CLEAN);else if(action==1)request_ui(CMD_TOGGLE);}

typedef struct{UINT id;int separator,submenu,checked,disabled;wchar_t label[256];}MenuEntry;
static MenuEntry g_menu_entries[MENU_MAX];static int g_menu_count;static HFONT g_menu_font;static HBRUSH g_menu_brush;static COLORREF g_menu_bg,g_menu_fg,g_menu_selected;static int g_menu_row,g_menu_margin;
static int dpi_scale(int value){return MulDiv(value,(int)g_dpi,96);}
static void menu_palette(void){HIGHCONTRASTW hc;memset(&hc,0,sizeof(hc));hc.cbSize=sizeof(hc);SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(hc),&hc,0);if(hc.dwFlags&HCF_HIGHCONTRASTON){g_menu_bg=GetSysColor(COLOR_MENU);g_menu_fg=GetSysColor(COLOR_MENUTEXT);g_menu_selected=GetSysColor(COLOR_HIGHLIGHT);}else{g_menu_bg=g_settings.dark?RGB(43,43,43):RGB(249,249,249);g_menu_fg=g_settings.dark?RGB(245,245,245):RGB(26,26,26);g_menu_selected=g_settings.dark?RGB(62,62,62):RGB(232,232,232);}g_menu_row=dpi_scale(31);g_menu_margin=dpi_scale(8);g_menu_font=CreateFontW(-dpi_scale(12),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");g_menu_brush=CreateSolidBrush(g_menu_bg);}
static void style_menu(HMENU menu){MENUINFO info;memset(&info,0,sizeof(info));info.cbSize=sizeof(info);info.fMask=MIM_BACKGROUND|MIM_STYLE;info.hbrBack=g_menu_brush;info.dwStyle=MNS_CHECKORBMP;SetMenuInfo(menu,&info);}
static void append_menu(HMENU menu,UINT id,const char *label,int checked,int disabled,HMENU submenu){if(g_menu_count>=MENU_MAX)return;MenuEntry *entry=&g_menu_entries[g_menu_count++];memset(entry,0,sizeof(*entry));entry->id=id;entry->checked=checked;entry->disabled=disabled;entry->submenu=submenu!=NULL;translate(label,entry->label,256);MENUITEMINFOW item;memset(&item,0,sizeof(item));item.cbSize=sizeof(item);item.fMask=MIIM_FTYPE|MIIM_DATA|MIIM_ID|MIIM_STATE;item.fType=MFT_OWNERDRAW;item.wID=id;item.fState=(checked?MFS_CHECKED:0)|(disabled?MFS_DISABLED:0);item.dwItemData=(ULONG_PTR)entry;if(submenu){item.fMask|=MIIM_SUBMENU;item.hSubMenu=submenu;}InsertMenuItemW(menu,(UINT)-1,TRUE,&item);}
static void append_separator(HMENU menu){AppendMenuW(menu,MF_SEPARATOR,0,NULL);}
static HMENU build_menu(void){g_menu_count=0;menu_palette();HMENU menu=CreatePopupMenu(),regions=CreatePopupMenu();if(!menu||!regions){if(menu)DestroyMenu(menu);if(regions)DestroyMenu(regions);return NULL;}style_menu(menu);style_menu(regions);int selected=0;for(int i=0;i<9;++i)selected+=g_settings.regions[i];for(int i=0;i<9;++i)append_menu(regions,CMD_REGION+i,region_labels[i],g_settings.regions[i],g_settings.regions[i]&&selected<=1,NULL);
    append_menu(menu,CMD_SHOW,"Abrir Orion Memory",0,0,NULL);append_menu(menu,CMD_CLEAN,"Limpar memória agora",0,g_busy,NULL);append_separator(menu);append_menu(menu,0,"Partes da memória para limpar",0,0,regions);append_separator(menu);append_menu(menu,CMD_SETTINGS,"Configurações",0,0,NULL);append_menu(menu,CMD_HISTORY,"Histórico de limpezas",0,0,NULL);append_menu(menu,CMD_ABOUT,"Sobre o Orion Memory",0,0,NULL);append_separator(menu);append_menu(menu,CMD_EXIT,"Sair",0,g_busy||native_authorization_busy(),NULL);return menu;}
static void release_menu(HMENU menu){if(menu)DestroyMenu(menu);if(g_menu_font)DeleteObject(g_menu_font);if(g_menu_brush)DeleteObject(g_menu_brush);g_menu_font=NULL;g_menu_brush=NULL;g_menu_count=0;}
static LRESULT measure_menu(MEASUREITEMSTRUCT *measure){if(measure->CtlType!=ODT_MENU||!measure->itemData)return FALSE;MenuEntry *entry=(MenuEntry*)measure->itemData;HDC dc=GetDC(g_hwnd);HGDIOBJ old=SelectObject(dc,g_menu_font);SIZE text={0};GetTextExtentPoint32W(dc,entry->label,(int)wcslen(entry->label),&text);SelectObject(dc,old);ReleaseDC(g_hwnd,dc);measure->itemWidth=(UINT)max(dpi_scale(226),text.cx+dpi_scale(52));measure->itemHeight=(UINT)g_menu_row;return TRUE;}
static LRESULT draw_menu(DRAWITEMSTRUCT *draw){if(draw->CtlType!=ODT_MENU||!draw->itemData)return FALSE;MenuEntry *entry=(MenuEntry*)draw->itemData;int selected=(draw->itemState&ODS_SELECTED)&&!entry->disabled;HBRUSH bg=CreateSolidBrush(selected?g_menu_selected:g_menu_bg);FillRect(draw->hDC,&draw->rcItem,bg);DeleteObject(bg);SetBkMode(draw->hDC,TRANSPARENT);COLORREF foreground=entry->disabled?(g_settings.dark?RGB(145,145,145):RGB(125,125,125)):g_menu_fg;SetTextColor(draw->hDC,foreground);HGDIOBJ old_font=SelectObject(draw->hDC,g_menu_font);RECT text=draw->rcItem;text.left+=dpi_scale(32);text.right-=dpi_scale(22);DrawTextW(draw->hDC,entry->label,-1,&text,DT_SINGLELINE|DT_VCENTER|DT_LEFT|DT_NOPREFIX);SelectObject(draw->hDC,old_font);HPEN pen=CreatePen(PS_SOLID,max(1,dpi_scale(1)),foreground);HGDIOBJ old_pen=SelectObject(draw->hDC,pen);int cy=(draw->rcItem.top+draw->rcItem.bottom)/2;if(entry->checked){int cx=draw->rcItem.left+dpi_scale(16);MoveToEx(draw->hDC,cx-dpi_scale(4),cy,NULL);LineTo(draw->hDC,cx-dpi_scale(1),cy+dpi_scale(3));LineTo(draw->hDC,cx+dpi_scale(5),cy-dpi_scale(4));}if(entry->submenu){int cx=draw->rcItem.right-dpi_scale(12);MoveToEx(draw->hDC,cx-dpi_scale(2),cy-dpi_scale(4),NULL);LineTo(draw->hDC,cx+dpi_scale(2),cy);LineTo(draw->hDC,cx-dpi_scale(2),cy+dpi_scale(4));}SelectObject(draw->hDC,old_pen);DeleteObject(pen);return TRUE;}
#include "native_draw.inc"
#include "task_launch.inc"
#include "native_authorize.inc"
#include "native_settings.inc"
#include "native_actions.inc"
#include "native_automation.inc"
#include "main_window.inc"
#include "native_popup.inc"
static void native_preferences_changed(void){load_settings(1);native_automation_preferences_changed();update_memory();native_main_refresh();PostMessageW(g_hwnd,WM_APP+21,0,0);}
static void native_cleanup_completed(int success,const wchar_t *summary){g_busy=0;update_memory();native_main_refresh();if(g_settings.notifications&&summary&&summary[0]){NOTIFYICONDATAW data=tray_data();data.uFlags=NIF_INFO;data.dwInfoFlags=NIIF_USER|NIIF_LARGE_ICON|(g_settings.sound?0:NIIF_NOSOUND);data.hBalloonIcon=LoadIconW(g_instance,MAKEINTRESOURCEW(1));translate("Liberação de memória",data.szInfoTitle,64);copyw(data.szInfo,256,summary);Shell_NotifyIconW(NIM_MODIFY,&data);}if(g_exit_pending)PostMessageW(g_hwnd,WM_CLOSE,0,0);}
static int native_save_boolean(const char *key,int enabled){load_settings(1);char *next=json_replace(g_document?g_document:"{}",key,enabled?"true":"false");if(!next)return 0;int ok=atomic_settings_write(next);free(next);if(ok)native_preferences_changed();return ok;}
static void native_execute_command(UINT command){if(command>=CMD_REGION&&command<CMD_REGION+9){if(!toggle_region((int)command-CMD_REGION))show_error("Não foi possível salvar a preferência. Mantenha pelo menos uma região selecionada.");native_preferences_changed();}else if(command==CMD_DARK)native_save_boolean("DarkTheme",!g_settings.dark);else if(command==CMD_TOPMOST)native_save_boolean("AlwaysOnTop",!g_settings.always_on_top);else if(command==CMD_GRAPH)native_save_boolean("ShowGraph",!g_settings.show_graph);else if(command==CMD_REFRESH)update_memory();else request_ui((int)command);}
static void show_menu(void){KillTimer(g_hwnd,CLICK_TIMER);load_settings(0);update_dpi();POINT cursor;GetCursorPos(&cursor);native_popup_open(g_hwnd,0,cursor);}

static LRESULT CALLBACK host_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){
    if(message==g_taskbar_created&&g_hwnd){g_registered=0;g_percent=-1;update_memory();return 0;}
    switch(message){
        case WM_CREATE:g_hwnd=hwnd;return 0;
        case WM_TIMER:if(wp==POLL_TIMER){if(load_settings(0)){native_automation_preferences_changed();update_memory();native_main_refresh();native_settings_refresh();}native_actions_tick();if(g_exit_pending&&!g_busy&&!native_authorization_busy())PostMessageW(hwnd,WM_CLOSE,0,0);if(GetTickCount64()-g_last_memory_tick>=(ULONGLONG)g_settings.refresh_seconds*1000)update_memory();native_automation_tick();}else if(wp==CLICK_TIMER){KillTimer(hwnd,CLICK_TIMER);tray_action(g_settings.click);}return 0;
        case WM_TRAY:{UINT event=(UINT)(lp&0xFFFF);if(event==WM_LBUTTONUP){if(g_skip_up)g_skip_up=0;else{KillTimer(hwnd,CLICK_TIMER);SetTimer(hwnd,CLICK_TIMER,GetDoubleClickTime(),NULL);}}else if(event==WM_LBUTTONDBLCLK){KillTimer(hwnd,CLICK_TIMER);g_skip_up=1;tray_action(g_settings.double_click);}else if(event==NIN_KEYSELECT){KillTimer(hwnd,CLICK_TIMER);request_ui(CMD_SHOW);}else if(event==WM_RBUTTONUP||event==WM_CONTEXTMENU)show_menu();return 0;}
        case WM_HOTKEY:if(wp==HOTKEY_ID)request_ui(CMD_CLEAN);return 0;
        case WM_REQUEST:if(wp>=CMD_SHOW&&wp<=CMD_TOGGLE&&lp==0)request_ui((int)wp);return 0;
        case WM_APP+20:native_preferences_changed();return 0;case WM_APP+21:native_settings_refresh();return 0;
        case WM_CLEAN_BUSY:g_busy=wp!=0;if(!g_busy&&g_exit_pending)PostMessageW(hwnd,WM_CLOSE,0,0);return 0;
        case WM_BALLOON:if(g_settings.notifications){NOTIFYICONDATAW data=tray_data();data.uFlags=NIF_INFO;data.dwInfoFlags=NIIF_INFO|(g_settings.sound?0:NIIF_NOSOUND);copyw(data.szInfoTitle,64,L"Orion Memory");translate(wp?"Concluído":"Não concluído",data.szInfo,256);Shell_NotifyIconW(NIM_MODIFY,&data);}return 0;
        case WM_SETTINGCHANGE:case WM_DISPLAYCHANGE:case WM_DPICHANGED:update_dpi();g_percent=-1;update_memory();return 0;
        case WM_MEASUREITEM:return measure_menu((MEASUREITEMSTRUCT*)lp);
        case WM_DRAWITEM:return draw_menu((DRAWITEMSTRUCT*)lp);
        case WM_CLOSE:if(g_busy||native_authorization_busy()){g_exit_pending=1;return 0;}popup_close_all();native_settings_close();native_actions_close();if(g_main_hwnd)DestroyWindow(g_main_hwnd);DestroyWindow(hwnd);return 0;
        case WM_DESTROY:{KillTimer(hwnd,POLL_TIMER);KillTimer(hwnd,CLICK_TIMER);if(g_hotkey_registered)UnregisterHotKey(hwnd,HOTKEY_ID);NOTIFYICONDATAW data=tray_data();Shell_NotifyIconW(NIM_DELETE,&data);if(g_icon){DestroyIcon(g_icon);g_icon=NULL;}PostQuitMessage(0);return 0;}
    }return DefWindowProcW(hwnd,message,wp,lp);
}
static int setup_paths(const wchar_t *test_dir){GetModuleFileNameW(NULL,g_folder,MAX_PATH);wchar_t *slash=wcsrchr(g_folder,L'\\');if(!slash)return 0;*slash=0;if(test_dir){if(!GetFullPathNameW(test_dir,MAX_PATH,g_data_dir,NULL))return 0;}else{if(FAILED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,SHGFP_TYPE_CURRENT,g_data_dir)))return 0;size_t len=wcslen(g_data_dir);if(len+24>=MAX_PATH)return 0;wcscat(g_data_dir,L"\\OrionOS\\OrionMemory");}return path_join(g_settings_path,MAX_PATH,g_data_dir,L"settings.json");}
static int check(int condition,const char *description){char line[768];int len=snprintf(line,sizeof(line),"%s: %s\r\n",condition?"PASS":"FAIL",description);DWORD written;HANDLE output=GetStdHandle(STD_OUTPUT_HANDLE);if(output&&output!=INVALID_HANDLE_VALUE)WriteFile(output,line,(DWORD)len,&written,NULL);return condition?0:1;}
static int storage_self_test(void) {
    wchar_t temporary[MAX_PATH],backup[MAX_PATH];
    if(!GetTempPathW(MAX_PATH,temporary))return check(0,"isolated native storage fixture directory");
    swprintf(g_data_dir,MAX_PATH,L"%lsOrionMemory.NativeTests.%lu.%lu",temporary,GetCurrentProcessId(),GetTickCount());
    if(!CreateDirectoryW(g_data_dir,NULL))return check(0,"isolated native storage fixture directory");
    path_join(g_settings_path,MAX_PATH,g_data_dir,L"settings.json");swprintf(backup,MAX_PATH,L"%ls.bak",g_settings_path);
    const char *fixture="{\"NativeAppearanceRevision\":20,\"Unknown\":{\"keep\":[7,true]},\"Profile\":0,\"TrayTextColor\":\"#ABCDEF\"}";
    int failures=0,ok=atomic_settings_write(fixture);if(ok)load_settings(1);if(ok)ok=toggle_region(0);
    char *primary=read_utf8(g_settings_path);int backup_absent=GetFileAttributesW(backup)==INVALID_FILE_ATTRIBUTES;Json primary_json;memset(&primary_json,0,sizeof(primary_json));
    int parsed=primary&&json_init(&primary_json,primary,strlen(primary));
    failures+=check(ok&&parsed&&strstr(primary,"\"Unknown\":{\"keep\":[7,true]}")&&backup_absent,"atomic native region edits preserve unknown values without writing redundant backups");
    int profile=parsed?json_find(&primary_json,"Profile"):-1;
    failures+=check(parsed&&profile>=0&&primary_json.tokens[profile].type==J_NULL&&json_bool(&primary_json,"GlobalWorkingSets",0)&&json_bool(&primary_json,"LowPriorityStandby",0)&&!json_bool(&primary_json,"FullStandby",1),"native edits retire legacy profiles without changing their effective region choices");
    json_free(&primary_json);free(primary);
    const char *only_one="{\"NativeAppearanceRevision\":20,\"GlobalWorkingSets\":false,\"SystemFileCache\":false,\"ModifiedPageList\":false,\"FullStandby\":false,\"LowPriorityStandby\":true,\"ModifiedFileCache\":false,\"RegistryCache\":false,\"CombineMemory\":false,\"BackgroundWorkingSets\":false}";
    ok=atomic_settings_write(only_one);if(ok)load_settings(1);int rejected=!toggle_region(4);primary=read_utf8(g_settings_path);
    failures+=check(ok&&rejected&&primary&&!strcmp(primary,only_one),"native submenu refuses to disable the final selected region");
    free(primary);
    ok=atomic_settings_write("{\"NativeAppearanceRevision\":19,\"TrayFontSize\":4,\"Unknown\":{\"keep\":7}}");if(ok)load_settings(1);
    failures+=check(ok&&g_settings.font_size==8&&g_document&&strstr(g_document,"\"Unknown\":{\"keep\":7}"),"appearance migration applies the new eight-point default and preserves unrelated preferences");
    char *smaller=json_replace(g_document?g_document:"{}","TrayFontSize","1");ok=smaller&&atomic_settings_write(smaller);free(smaller);if(ok)load_settings(1);
    failures+=check(ok&&g_settings.font_size==1,"one-time appearance migration does not reset a subsequently chosen smaller font");
    DeleteFileW(g_settings_path);DeleteFileW(backup);RemoveDirectoryW(g_data_dir);free(g_document);g_document=NULL;g_settings_loaded=0;default_settings(&g_settings);return failures;
}
static int icon_ink_height(HICON icon,int size){ICONINFO info;memset(&info,0,sizeof(info));if(!icon||!GetIconInfo(icon,&info))return 0;BITMAPINFO format;memset(&format,0,sizeof(format));format.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);format.bmiHeader.biWidth=size;format.bmiHeader.biHeight=-size;format.bmiHeader.biPlanes=1;format.bmiHeader.biBitCount=32;format.bmiHeader.biCompression=BI_RGB;uint32_t pixels[32*32];HDC dc=GetDC(NULL);int top=size,bottom=-1;if(size<=32&&GetDIBits(dc,info.hbmColor,0,(UINT)size,pixels,&format,DIB_RGB_COLORS))for(int y=0;y<size;++y)for(int x=0;x<size;++x)if((pixels[y*size+x]>>24)>10){if(y<top)top=y;if(y>bottom)bottom=y;}ReleaseDC(NULL,dc);DeleteObject(info.hbmColor);DeleteObject(info.hbmMask);return bottom<0?0:bottom-top+1;}
static int native_ui_self_test(void){int failures=0;Popup menu;memset(&menu,0,sizeof(menu));popup_build(&menu);int expected[]={CMD_SHOW,CMD_CLEAN,0,0,0,CMD_SETTINGS,CMD_HISTORY,CMD_ABOUT,0,CMD_EXIT};int menu_ok=menu.count==10;for(int i=0;i<10&&menu_ok;++i)menu_ok&=menu.rows[i].command==(UINT)expected[i];menu_ok&=menu.rows[2].separator&&menu.rows[4].separator&&menu.rows[8].separator&&menu.rows[3].submenu;memset(&menu,0,sizeof(menu));menu.kind=4;popup_build(&menu);menu_ok&=menu.count==9;for(int i=0;i<9&&menu_ok;++i)menu_ok&=menu.rows[i].command==(UINT)(CMD_REGION+i);failures+=check(menu_ok,"rounded native popups contain exact command order, themed separators and nine direct regions");wchar_t percentage[64];double precise=100.0*(1000000.0-682870.0)/1000000.0;format_percent(precise,percentage,64);failures+=check(!wcscmp(percentage,L"31,713%"),"main percentages retain three real decimal places and the Brazilian comma");Settings original=g_settings,parsed;Json settings_json;const char *settings_text="{\"NativeAppearanceRevision\":20,\"TrayFontSize\":1,\"RefreshSeconds\":5}";int ok=json_init(&settings_json,settings_text,strlen(settings_text));if(ok)parse_settings(&parsed,&settings_json);json_free(&settings_json);Settings defaults;default_settings(&defaults);failures+=check(ok&&defaults.font_size==8&&parsed.font_size==1,"native tray font defaults to eight points and accepts one point");failures+=check(ok&&defaults.refresh_seconds==1&&parsed.refresh_seconds==5,"native refresh preference controls memory sampling without delaying worker completion or settings polling");g_settings=defaults;g_settings.transparent=1;g_settings.border=0;g_settings.font_size=1;HICON smallest=render_icon(88,16);int small_height=icon_ink_height(smallest,16);if(smallest)DestroyIcon(smallest);g_settings.font_size=4;HICON normal=render_icon(88,16);int normal_height=icon_ink_height(normal,16);if(normal)DestroyIcon(normal);g_settings.font_size=9;HICON large=render_icon(88,16);int large_height=icon_ink_height(large,16);if(large)DestroyIcon(large);failures+=check(small_height>0&&small_height<=3&&normal_height>small_height&&normal_height<=6&&large_height>normal_height,"one/four/nine point tray fonts honor their actual ink size without scaling to fill the icon");g_settings=original;return failures;}
static int self_test(void){g_test_mode=1;int failures=0;default_settings(&g_settings);Json j;const char *sample="{\"Unknown\":{\"deep\":[1,true,\"preserve\"]},\"LowPriorityStandby\":true,\"GlobalWorkingSets\":false,\"Language\":\"pt-BR\",\"Unicode\":\"\\u00e7\\ud83d\\ude00\"}";int ok=json_init(&j,sample,strlen(sample));failures+=check(ok,"native JSON parser accepts nested unknown keys and escaped Unicode");if(ok){wchar_t unicode[16];failures+=check(json_wide(&j,"Unicode",unicode,16)&&unicode[0]==0x00E7&&unicode[1]==0xD83D&&unicode[2]==0xDE00,"native JSON decodes escaped surrogate pairs without corruption");}json_free(&j);char *edited=json_replace(sample,"LowPriorityStandby","false");failures+=check(edited&&strstr(edited,"\"Unknown\":{\"deep\":[1,true,\"preserve\"]}")&&strstr(edited,"\"LowPriorityStandby\":false"),"region edits preserve unknown JSON properties and formatting");free(edited);const char *invalid[]={"{\"x\":true,}","{\"x\":01}","{\"x\":tru}","{\"x\":\"bad\\z\"}","[]","{\"x\":1} garbage"};ok=1;for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);++i){if(json_init(&j,invalid[i],strlen(invalid[i])))ok=0;json_free(&j);}failures+=check(ok,"malformed or incomplete settings are rejected");
    char *large=(char*)malloc(160000);int used=0;if(large){used+=snprintf(large+used,160000-used,"{\"Unknown\":[");for(int i=0;i<10000;++i)used+=snprintf(large+used,160000-used,"%s%d",i?",":"",i);used+=snprintf(large+used,160000-used,"],\"Tail\":42}");}Json grow;int growth_ok=large&&json_init(&grow,large,(size_t)used);failures+=check(growth_ok&&grow.count>10000&&json_number(&grow,"Tail",0)==42&&grow.capacity<=16384,"JSON token storage grows on demand and preserves parent offsets across reallocations");if(large)json_free(&grow);free(large);
    failures+=storage_self_test();HICON warmup=render_icon(88,16);if(warmup)DestroyIcon(warmup);DWORD before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);int values[]={0,1,9,14,88,100},sizes[]={16,20,24,32,48};ok=1;for(unsigned a=0;a<sizeof(sizes)/sizeof(*sizes);++a)for(unsigned b=0;b<sizeof(values)/sizeof(*values);++b){HICON icon=render_icon(values[b],sizes[a]);if(!icon)ok=0;else{ICONINFO info;if(GetIconInfo(icon,&info)){BITMAP bmp;GetObjectW(info.hbmColor,sizeof(bmp),&bmp);if(bmp.bmWidth!=sizes[a]||bmp.bmHeight!=sizes[a])ok=0;DeleteObject(info.hbmColor);DeleteObject(info.hbmMask);}else ok=0;DestroyIcon(icon);}}failures+=check(ok,"native tray values 0/1/9/14/88/100 render at five DPI sizes");DWORD after=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);failures+=check(after<=before+2,"native icon rendering releases all GDI objects");update_dpi();HMENU menu=build_menu();failures+=check(menu&&GetMenuItemCount(menu)==10&&GetMenuItemCount(GetSubMenu(menu,3))==9,"native popup contains the requested order and direct region submenu");release_menu(menu);failures+=native_ui_self_test();failures+=native_actions_self_test();failures+=native_authorization_self_test();failures+=native_automation_self_test();return failures?1:0;}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR command_line,int show) {
    (void)previous;(void)command_line;(void)show;g_instance=instance;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int argc;wchar_t **argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    int minimized=0,selftest=0,initial=CMD_SHOW;const wchar_t *test_dir=NULL,*test_show=NULL,*authorization_mode=NULL,*authorization_sid=NULL;
    for(int i=1;i<argc;++i) {
        if(!wcscmp(argv[i],L"--minimized"))minimized=1;
        else if(!wcscmp(argv[i],L"--authorized-start"))g_authorized=1;
        else if(!wcscmp(argv[i],L"--self-test"))selftest=1;
        else if(!wcscmp(argv[i],L"--test-show")&&i+1<argc)test_show=argv[++i];
        else if(!wcscmp(argv[i],L"--native-configure-authorization")&&i+2<argc){authorization_mode=argv[++i];authorization_sid=argv[++i];}
        else if(!wcscmp(argv[i],L"--native-test-dir")&&i+1<argc){test_dir=argv[++i];g_test_mode=1;}
        else if(!wcscmp(argv[i],L"--settings"))initial=CMD_SETTINGS;
        else if(!wcscmp(argv[i],L"--history"))initial=CMD_HISTORY;
        else if(!wcscmp(argv[i],L"--about"))initial=CMD_ABOUT;
        else if(!wcscmp(argv[i],L"--clean"))initial=CMD_CLEAN;
        else if(!wcscmp(argv[i],L"--exit"))initial=CMD_EXIT;
    }
    if(selftest){int result=self_test();LocalFree(argv);return result;}
    if(authorization_mode){int result=g_test_mode?2:native_authorization_worker(authorization_mode,authorization_sid);LocalFree(argv);return result;}
    int worker_result=0;if(native_actions_command_line(argc,argv,&worker_result)){LocalFree(argv);return worker_result;}
    if(!setup_paths(test_dir)){LocalFree(argv);return 2;}
    if(!g_test_mode) {
        HWND other=FindWindowW(HOST_CLASS,NULL);
        if(other){if(!minimized)PostMessageW(other,WM_REQUEST,(WPARAM)initial,0);LocalFree(argv);return 0;}
        if(initial==CMD_EXIT){LocalFree(argv);return 0;}
    }
    default_settings(&g_settings);load_settings(1);
    int saved_minimized=0;
    if(g_document){Json saved;if(json_init(&saved,g_document,strlen(g_document)))saved_minimized=json_bool(&saved,"StartMinimized",0);json_free(&saved);}
    if((!minimized||saved_minimized)&&try_authorized_start(initial)){LocalFree(argv);return 0;}
    wchar_t mutex_name[160];
    if(g_test_mode)swprintf(mutex_name,160,L"Local\\OrionOS.OrionMemory.Native.Test.%lu",GetCurrentProcessId());else copyw(mutex_name,160,HOST_MUTEX);
    g_mutex=CreateMutexW(NULL,TRUE,mutex_name);DWORD error=GetLastError();
    if(!g_mutex){LocalFree(argv);return 3;}
    if(error==ERROR_ALREADY_EXISTS&&!g_test_mode){HWND other=FindWindowW(HOST_CLASS,NULL);for(int retry=0;!other&&retry<30;++retry){Sleep(10);other=FindWindowW(HOST_CLASS,NULL);}if(other&&!minimized)PostMessageW(other,WM_REQUEST,(WPARAM)initial,0);CloseHandle(g_mutex);LocalFree(argv);return other?0:4;}
    g_taskbar_created=RegisterWindowMessageW(L"TaskbarCreated");WNDCLASSEXW cls;memset(&cls,0,sizeof(cls));cls.cbSize=sizeof(cls);cls.lpfnWndProc=host_proc;cls.hInstance=instance;cls.lpszClassName=HOST_CLASS;
    RegisterClassExW(&cls);HWND hwnd=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,HOST_CLASS,L"Orion Memory native resident",WS_POPUP,-32000,-32000,1,1,NULL,NULL,instance,NULL);
    if(!hwnd){ReleaseMutex(g_mutex);CloseHandle(g_mutex);LocalFree(argv);return 5;}
    /* Only fixed, enum-validated UI commands cross from a normal launcher to
       an already authorized resident. No pointer, path or arbitrary command is accepted. */
    if(process_elevated())ChangeWindowMessageFilterEx(hwnd,WM_REQUEST,MSGFLT_ALLOW,NULL);
    if(!g_test_mode&&ns_bool("StartWithWindows",1))ns_startup(1);
    bind_hotkey();update_memory();native_automation_preferences_changed();SetTimer(hwnd,POLL_TIMER,1000,NULL);
    if(!minimized&&!saved_minimized&&!g_test_mode)request_ui(initial);else if(initial!=CMD_SHOW&&!g_test_mode)request_ui(initial);
    if(g_test_mode&&test_show){if(!wcscmp(test_show,L"main"))native_main_show();else if(!wcscmp(test_show,L"settings"))native_settings_show(NULL);else if(!wcscmp(test_show,L"history"))native_open_history(NULL);else if(!wcscmp(test_show,L"about"))native_open_about(NULL);else if(!wcscmp(test_show,L"confirmation"))native_test_confirmation(NULL);else if(!wcscmp(test_show,L"menu")||!wcscmp(test_show,L"regions")){POINT point={300,300};native_popup_open(g_hwnd,!wcscmp(test_show,L"regions")?4:0,point);}}
    MSG message;while(GetMessageW(&message,NULL,0,0)>0){if(native_settings_message(&message)||native_main_message(&message))continue;TranslateMessage(&message);DispatchMessageW(&message);}
    json_free(&g_language_json);free(g_language_doc);free(g_document);ReleaseMutex(g_mutex);CloseHandle(g_mutex);LocalFree(argv);return 0;
}