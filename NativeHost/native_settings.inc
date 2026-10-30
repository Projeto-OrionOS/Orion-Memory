/* Native settings pages. Included after native_draw.inc and authorization helpers.
   No CLR, common-control library, font catalogue or bitmap cache is resident. */
#ifndef ORION_NATIVE_SETTINGS_INC
#define ORION_NATIVE_SETTINGS_INC
#include <windowsx.h>
#define NS_CLASS L"OrionMemory.NativeSettings"
#define NS_REGION_CLASS L"OrionMemory.NativeRegionList"
#define NS_SLIDER_CLASS L"OrionMemory.NativeColorSlider"
#define NS_SPINNER_CLASS L"OrionMemory.NumericSpinner"
#define NS_FIRST 5100
#define NS_NAV 5000
#define NS_CLOSE 5090
#define NS_APPLY 5093
#define NS_OPEN_LOG 5094
#define NS_PROFILE_LEVE 5550
#define NS_PROFILE_EQUILIBRADO 5551
#define NS_PROFILE_PERSONALIZADO 5552
#define NS_TEST_NOTIFICATION 5553
#define NS_COPY_HEX 5554
#define NS_DEFAULTS 5091
#define NS_AUTHORIZE 5092
#define NS_COLOR_ROLE 5300
#define NS_PALETTE 5320
#define NS_SLIDER 5340
#define NS_NUMBER_UP 5400
#define NS_NUMBER_DOWN 5450
#define NS_COLOR_TIMER 91
#define NS_REFRESH (WM_APP+72)
#define NS_REGION_ROW 34
#define NS_REGION_HEIGHT 33
#define NS_REGION_CONTENT (9*NS_REGION_ROW)
enum { NS_CHECK=1,NS_EDIT,NS_COMBO,NS_COLOR,NS_COLOR_EDIT,NS_NUMBER,NS_HOTKEY };
#define NS_STRETCH 1
#define NS_RIGHT 2
#define NS_BOTTOM 4
typedef struct {int id,kind,page,x,y,w,h,anchor,region,min,max,fallback,checked;const char *key,*label;HWND hwnd;} NSField;
typedef struct {wchar_t code[32],country[96],name[96];} NSLanguage;
static HWND g_ns_hwnd,g_ns_region;static int g_ns_authorized;
/* Transactional preferences: Cancel never writes to disk or the Run key. */
static char *g_ns_draft,*g_ns_baseline;
static Settings g_ns_preview;
static int g_ns_region_hint=-1;
/* 5 independently scrollable content panes; title/menu/footer remain stationary. */
static int g_ns_page; /* tentative declaration; defined alongside the settings globals. */
static int g_ns_page_scroll[5]={0,0,0,0,0};
static const int g_ns_page_bottom[5]={756,831,814,816,806};
static int ns_view_bottom(void){if(!g_ns_hwnd)return 660;RECT r;GetClientRect(g_ns_hwnd,&r);return MulDiv(r.bottom,96,(int)GetDpiForWindow(g_ns_hwnd))-64;}
static int ns_scroll_max(void){return max(0,g_ns_page_bottom[g_ns_page]-ns_view_bottom()+7);}
static int ns_scroll(void){int maximum=ns_scroll_max();g_ns_page_scroll[g_ns_page]=clampi(g_ns_page_scroll[g_ns_page],0,maximum);return g_ns_page_scroll[g_ns_page];}
static int ns_page_visible(int y,int h){int bot=ns_view_bottom(),scroll=ns_scroll();return y-scroll>=12&&y+h-scroll<=bot;}

/* Classificacao relativa de impacto (nao e promessa de ganho de desempenho).
   0: menor impacto, 1: cautela, 2: avancado, com possivel congelamento aparente. */
static const int ns_region_risk[9]={1,1,2,2,0,2,2,1,1};
static const char *ns_risk_names[3]={"RECOMENDADO · MENOR IMPACTO","ATENÇÃO","AVANÇADO"};
static const char *ns_risk_long[3]={
    "Impacto geralmente menor; ainda pode gerar recarregamento de dados.",
    "Pode aumentar atividade de disco, CPU ou atrasar alguns aplicativos.",
    "Pode causar lentidão intensa ou travamento aparente. Salve seu trabalho antes."
};
static DWORD ns_profile_mask(int kind){return kind==0?(1u<<4):(1u<<4)|(1u<<8);}
static DWORD ns_current_mask(void){DWORD mask=0;for(int i=0;i<9;++i)if(g_ns_preview.regions[i])mask|=1u<<i;return mask;}
static int ns_profile_selected(void){DWORD mask=ns_current_mask();return mask==ns_profile_mask(0)?0:mask==ns_profile_mask(1)?1:2;}
static COLORREF ns_risk_color(int risk){return risk==0?RGB(93,201,143):risk==1?RGB(242,187,91):RGB(251,118,114);}

static NSField g_ns_fields[64];static int g_ns_count,g_ns_page,g_ns_updating,g_ns_region_scroll,g_ns_color_role,g_ns_color_dirty,g_ns_fonts_loaded;
static HFONT g_ns_font,g_ns_bold,g_ns_title;static HBRUSH g_ns_edit_brush,g_ns_spinner_brush;static UINT g_ns_dpi;
static HWND g_ns_spinners[64];static int g_ns_spinner_hover[64];
static COLORREF g_ns_colors[4];static HWND g_ns_sliders[3],g_ns_font_box,g_ns_language_box,g_ns_hex,g_ns_font_size,g_ns_hotkey,g_ns_authorize;
static NSLanguage g_ns_languages[48];static int g_ns_language_count;static wchar_t g_ns_status[320],g_ns_labels_language[32];static int g_ns_theme=-1,g_ns_window_blur=-1;
static const COLORREF ns_palette[]={RGB(111,181,255),RGB(15,28,50),RGB(249,194,86),RGB(255,127,123),RGB(255,255,255),RGB(32,32,32),RGB(76,217,100),RGB(255,176,32)};
static const char *ns_pages[]={"Geral","Limpeza","Aparência","Bandeja","Avançado"};
static const char *ns_pages_previous[]={"Principal","Liberação de memória","Aparência","Ícone de notificação","Avançado"};
static void native_settings_show(HWND owner);static void native_settings_refresh(void);static void native_settings_close(void);static int native_settings_message(MSG *message);
static void native_preferences_changed(void);
static void ns_layout(void);static void ns_refresh_fields(void);static void ns_status(const char *text);static int ns_save(const char *key,const char *raw);static void ns_flush_color(void);
static const char *ns_document(void){return g_ns_draft?g_ns_draft:g_document;}
static void ns_preview_refresh(void){
    Json json;memset(&json,0,sizeof(json));
    if(g_ns_draft&&json_init(&json,g_ns_draft,strlen(g_ns_draft)))parse_settings(&g_ns_preview,&json);
    json_free(&json);
}
static int ns_is_dirty(void){return g_ns_draft&&g_ns_baseline&&strcmp(g_ns_draft,g_ns_baseline)!=0;}
static void ns_nav_caption(int page,wchar_t *out,int capacity){
    /* Localize with existing 30-language keys. Brazilian Portuguese uses shorter captions. */
    translate(!wcscmp(g_settings.language,L"pt-BR")?ns_pages[page]:ns_pages_previous[page],out,capacity);
}
static const char *ns_region_short[]={
 "Pode recarregar aplicativos", "Aumenta leituras do disco", "Pode provocar gravações",
 "Reduz caches de espera", "Menor impacto relativo", "Pode elevar uso de disco",
 "Reconstrói cache do Registro", "Consome CPU temporariamente", "Pode recarregar processos"
};
static const char *ns_region_details[]={
    "Reduz a RAM residente de processos. Aplicativos podem recarregar dados e ficar lentos.",
    "Diminui o cache de arquivos do Windows. Pode elevar leituras do disco.",
    "Solicita descarregar páginas modificadas. Pode provocar gravações e longas esperas.",
    "Descarta toda a lista de espera. Pode prejudicar a resposta do sistema e de jogos.",
    "Descarta primeiro páginas em espera de baixa prioridade. Menor impacto relativo.",
    "Força a limpeza de buffers de arquivos modificados. Pode elevar o uso do disco.",
    "Esvazia o cache do Registro. O Windows poderá reconstruí-lo, causando atrasos.",
    "Combina páginas iguais. Pode consumir CPU durante a operação.",
    "Reduz RAM residente de processos em segundo plano. Apps podem recarregar dados."
};
static NSField *ns_field(int id){for(int i=0;i<g_ns_count;++i)if(g_ns_fields[i].id==id)return &g_ns_fields[i];return NULL;}
static int ns_bool(const char *key,int fallback){Json j;memset(&j,0,sizeof(j));int result=fallback;const char *doc=ns_document();if(doc&&json_init(&j,doc,strlen(doc)))result=json_bool(&j,key,fallback);json_free(&j);return result;}
static int ns_number(const char *key,int fallback){Json j;memset(&j,0,sizeof(j));int result=fallback;const char *doc=ns_document();if(doc&&json_init(&j,doc,strlen(doc)))result=(int)json_number(&j,key,fallback);json_free(&j);return result;}
static int ns_quote(const wchar_t *wide,char *out,int capacity){char utf8[1024];if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,-1,utf8,sizeof(utf8),NULL,NULL))return 0;int at=0;out[at++]='"';for(int i=0;utf8[i];++i){unsigned char c=(unsigned char)utf8[i];if(at+8>=capacity)return 0;if(c=='"'||c=='\\'){out[at++]='\\';out[at++]=(char)c;}else if(c<32){snprintf(out+at,(size_t)(capacity-at),"\\u%04x",c);at+=6;}else out[at++]=(char)c;}out[at++]='"';out[at]=0;return 1;}
static int ns_save(const char *key,const char *raw){
    if(!g_ns_draft){ns_status("Config. indisponíveis");return 0;}
    char *next=json_replace(g_ns_draft,key,raw);
    if(!next){ns_status("Erro ao preparar");return 0;}
    Json j;memset(&j,0,sizeof(j));int valid=json_init(&j,next,strlen(next));json_free(&j);
    if(!valid){free(next);ns_status("Configuração inválida.");return 0;}
    free(g_ns_draft);g_ns_draft=next;ns_preview_refresh();
    ns_status(ns_is_dirty()?"Alterações pendentes — clique em Aplicar":"Nenhuma alteração pendente");
    ns_refresh_fields();return 1;
}
static int ns_save_number(NSField *field){wchar_t text[48],*end;GetWindowTextW(field->hwnd,text,48);int previous=ns_number(field->key,field->fallback);long value=wcstol(text,&end,10);if(end==text||*end)value=previous;value=clampi((int)value,field->min,field->max);if(!strcmp(field->key,"WarningPercent")&&value>=g_ns_preview.danger)value=g_ns_preview.danger-1;if(!strcmp(field->key,"DangerPercent")&&value<=g_ns_preview.warning)value=g_ns_preview.warning+1;swprintf(text,48,L"%ld",value);int updating=g_ns_updating;g_ns_updating=1;SetWindowTextW(field->hwnd,text);g_ns_updating=updating;if(previous==value)return 1;char raw[32];snprintf(raw,sizeof(raw),"%ld",value);return ns_save(field->key,raw);}
static int ns_startup(int enabled){if(g_test_mode)return 1;HKEY key;LONG result=RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,NULL,0,KEY_SET_VALUE,NULL,&key,NULL);if(result!=ERROR_SUCCESS)return 0;if(enabled){wchar_t path[MAX_PATH],command[MAX_PATH+40];GetModuleFileNameW(NULL,path,MAX_PATH);swprintf(command,MAX_PATH+40,L"\"%ls\"%ls",path,ns_bool("StartMinimized",0)?L" --minimized":L"");result=RegSetValueExW(key,L"OrionMemory",0,REG_SZ,(const BYTE*)command,(DWORD)((wcslen(command)+1)*sizeof(wchar_t)));}else{result=RegDeleteValueW(key,L"OrionMemory");if(result==ERROR_FILE_NOT_FOUND)result=ERROR_SUCCESS;}RegCloseKey(key);return result==ERROR_SUCCESS;}
static void ns_invalidate(int x,int y,int w,int h){if(!g_ns_hwnd)return;RECT r={ui_scale(g_ns_hwnd,x),ui_scale(g_ns_hwnd,y),ui_scale(g_ns_hwnd,x+w),ui_scale(g_ns_hwnd,y+h)};InvalidateRect(g_ns_hwnd,&r,FALSE);}
static void ns_status(const char *text){wchar_t next[320];translate(text,next,320);if(!wcscmp(next,g_ns_status))return;copyw(g_ns_status,320,next);if(g_ns_hwnd){RECT r;GetClientRect(g_ns_hwnd,&r);r.left=ui_scale(g_ns_hwnd,12);r.top=r.bottom-ui_scale(g_ns_hwnd,56);r.right-=ui_scale(g_ns_hwnd,395);InvalidateRect(g_ns_hwnd,&r,FALSE);}}
static int ns_object_find(Json *j,int object,const char *key){char name[128];for(int n=object+1;n<j->tokens[object].next;){int value=n+1;if(json_text(j,n,name,sizeof(name))&&!strcmp(name,key))return value;n=j->tokens[value].next;}return -1;}
static void ns_token_wide(Json *j,int token,wchar_t *to,int cap){char value[768];to[0]=0;if(json_text(j,token,value,sizeof(value)))MultiByteToWideChar(CP_UTF8,0,value,-1,to,cap);}
static int __cdecl ns_language_order(const void *a,const void *b){const NSLanguage *one=(const NSLanguage*)a,*two=(const NSLanguage*)b;int result=CompareStringEx(L"pt-BR",NORM_IGNORECASE,one->country,-1,two->country,-1,NULL,NULL,0);if(result==CSTR_EQUAL)return wcscmp(one->code,two->code);return result-CSTR_EQUAL;}
static void ns_load_languages(void){g_ns_language_count=0;wchar_t path[MAX_PATH];path_join(path,MAX_PATH,g_folder,L"Languages.json");char *array=read_utf8(path);if(array){size_t len=strlen(array);char *wrapped=(char*)malloc(len+16);if(wrapped){snprintf(wrapped,len+16,"{\"items\":%s}",array);Json j;if(json_init(&j,wrapped,strlen(wrapped))){int items=json_find(&j,"items");if(items>=0&&j.tokens[items].type==J_ARRAY)for(int n=items+1;n<j.tokens[items].next&&g_ns_language_count<48;n=j.tokens[n].next){if(j.tokens[n].type!=J_OBJECT)continue;NSLanguage *language=&g_ns_languages[g_ns_language_count];ns_token_wide(&j,ns_object_find(&j,n,"Code"),language->code,32);ns_token_wide(&j,ns_object_find(&j,n,"Country"),language->country,96);ns_token_wide(&j,ns_object_find(&j,n,"Name"),language->name,96);if(language->code[0]&&language->country[0])++g_ns_language_count;}}json_free(&j);free(wrapped);}free(array);}if(!g_ns_language_count){copyw(g_ns_languages[0].code,32,L"pt-BR");copyw(g_ns_languages[0].country,96,L"Brasil");copyw(g_ns_languages[0].name,96,L"Português");g_ns_language_count=1;}qsort(g_ns_languages,(size_t)g_ns_language_count,sizeof(NSLanguage),ns_language_order);}
static void ns_fonts(void){if(g_ns_font)DeleteObject(g_ns_font);if(g_ns_bold)DeleteObject(g_ns_bold);if(g_ns_title)DeleteObject(g_ns_title);if(g_ns_edit_brush)DeleteObject(g_ns_edit_brush);g_ns_font=ui_font(g_ns_hwnd,12,FW_NORMAL);g_ns_bold=ui_font(g_ns_hwnd,12,FW_SEMIBOLD);g_ns_title=ui_font(g_ns_hwnd,17,FW_SEMIBOLD);g_ns_edit_brush=CreateSolidBrush(ui_surface());if(g_ns_spinner_brush)DeleteObject(g_ns_spinner_brush);g_ns_spinner_brush=CreateSolidBrush(ui_background());g_ns_dpi=GetDpiForWindow(g_ns_hwnd);}
static HWND ns_button(HWND parent,int id,const wchar_t *label,int x,int y,int w,int h){HWND control=CreateWindowExW(0,L"BUTTON",label,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,ui_scale(g_ns_hwnd,x),ui_scale(g_ns_hwnd,y),ui_scale(g_ns_hwnd,w),ui_scale(g_ns_hwnd,h),parent,(HMENU)(INT_PTR)id,g_instance,NULL);SendMessageW(control,WM_SETFONT,(WPARAM)g_ns_font,FALSE);ui_prepare_button(control);return control;}

/* Unified numeric spinner. The Win32 EDIT remains a real editable/accessibility
   control; its host paints one rounded surface, divider and two blue chevrons.
   Arrow presses travel through the existing validated numeric-save path. */
static void ns_spinner_paint(HWND hwnd,HDC dc){
    int index=(int)GetWindowLongPtrW(hwnd,GWLP_USERDATA)-1;
    if(index<0||index>=g_ns_count)return;
    NSField *field=&g_ns_fields[index];
    RECT r;GetClientRect(hwnd,&r);
    ui_fill(dc,r,ui_surface());
    ui_round_border(dc,r,ui_background(),GetFocus()==field->hwnd?ui_focus_outline():ui_outline(),ui_scale(hwnd,7));
    int right=r.right-ui_scale(hwnd,28),mid=(r.top+r.bottom)/2;
    HPEN divider=CreatePen(PS_SOLID,1,ui_outline());
    HGDIOBJ previous=SelectObject(dc,divider);
    MoveToEx(dc,right,r.top+ui_scale(hwnd,7),NULL);
    LineTo(dc,right,r.bottom-ui_scale(hwnd,7));
    SelectObject(dc,previous);DeleteObject(divider);
    int hovered=g_ns_spinner_hover[index];
    for(int direction=0;direction<2;++direction){
        int cy=direction==0?(r.top+mid)/2:(mid+r.bottom)/2;
        int cx=(right+r.right)/2;
        COLORREF accent=(hovered==direction+1)?ui_accent():g_settings.dark?RGB(110,170,246):RGB(0,103,192);
        HPEN pen=CreatePen(PS_SOLID,max(1,ui_scale(hwnd,2)),accent);
        previous=SelectObject(dc,pen);
        int size=ui_scale(hwnd,4);
        if(direction==0){MoveToEx(dc,cx-size,cy+size/2,NULL);LineTo(dc,cx,cy-size/2);LineTo(dc,cx+size,cy+size/2);}
        else {MoveToEx(dc,cx-size,cy-size/2,NULL);LineTo(dc,cx,cy+size/2);LineTo(dc,cx+size,cy-size/2);}
        SelectObject(dc,previous);DeleteObject(pen);
    }
}
static LRESULT CALLBACK ns_spinner_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){
    int index=(int)GetWindowLongPtrW(hwnd,GWLP_USERDATA)-1;
    switch(message){
    case WM_NCCREATE:{CREATESTRUCTW *create=(CREATESTRUCTW*)lp;SetWindowLongPtrW(hwnd,GWLP_USERDATA,(LONG_PTR)create->lpCreateParams);return TRUE;}
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);ns_spinner_paint(hwnd,dc);EndPaint(hwnd,&ps);return 0;}
    case WM_PRINTCLIENT:ns_spinner_paint(hwnd,(HDC)wp);return 0;
    case WM_CTLCOLOREDIT:case WM_CTLCOLORSTATIC:
        SetTextColor((HDC)wp,ui_text());SetBkColor((HDC)wp,ui_background());return (LRESULT)g_ns_spinner_brush;
    case WM_COMMAND:return SendMessageW(g_ns_hwnd,WM_COMMAND,wp,lp);
    case WM_MOUSEMOVE:{
        if(index<0||index>=g_ns_count)break;
        RECT rect;GetClientRect(hwnd,&rect);int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);
        int current=x>=rect.right-ui_scale(hwnd,28)?(y<rect.bottom/2?1:2):0;
        if(current!=g_ns_spinner_hover[index]){g_ns_spinner_hover[index]=current;InvalidateRect(hwnd,NULL,FALSE);}
        TRACKMOUSEEVENT track={sizeof(track),TME_LEAVE,hwnd,0};TrackMouseEvent(&track);
        return 0;}
    case WM_MOUSELEAVE:
        if(index>=0&&index<g_ns_count){g_ns_spinner_hover[index]=0;InvalidateRect(hwnd,NULL,FALSE);}return 0;
    case WM_LBUTTONDOWN:{
        if(index<0||index>=g_ns_count)break;
        RECT rect;GetClientRect(hwnd,&rect);
        int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);
        if(x>=rect.right-ui_scale(hwnd,28)){
            int command=(y<rect.bottom/2?NS_NUMBER_UP:NS_NUMBER_DOWN)+index;
            SendMessageW(g_ns_hwnd,WM_COMMAND,MAKEWPARAM(command,BN_CLICKED),0);
        }
        SetFocus(g_ns_fields[index].hwnd);
        SendMessageW(g_ns_fields[index].hwnd,EM_SETSEL,0,-1);
        return 0;}
    case WM_SETCURSOR:
        if(LOWORD(lp)==HTCLIENT){SetCursor(LoadCursorW(NULL,IDC_ARROW));return TRUE;}break;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
static void ns_create_spinners(void){
    for(int i=0;i<g_ns_count;++i){
        NSField *field=&g_ns_fields[i];
        if(field->kind!=NS_NUMBER)continue;
        HWND container=CreateWindowExW(0,NS_SPINNER_CLASS,L"",WS_CHILD|WS_CLIPCHILDREN,
            0,0,1,1,g_ns_hwnd,NULL,g_instance,(LPVOID)(INT_PTR)(i+1));
        if(!container)continue;
        g_ns_spinners[i]=container;
        SetParent(field->hwnd,container);
        SetWindowPos(field->hwnd,NULL,ui_scale(container,10),ui_scale(container,7),
            ui_scale(container,42),ui_scale(container,19),SWP_NOZORDER|SWP_NOACTIVATE);
    }
}

static NSField *ns_add(int id,int kind,int page,const char *key,const char *label,int x,int y,int w,int h,int anchor,int fallback){NSField *field=&g_ns_fields[g_ns_count++];memset(field,0,sizeof(*field));field->id=id;field->kind=kind;field->page=page;field->key=key;field->label=label;field->x=x;field->y=y;field->w=w;field->h=h;field->anchor=anchor;field->fallback=fallback;field->region=-1;if(kind==NS_NUMBER)field->w=w=56;wchar_t title[256];translate(label?label:"",title,256);if(kind==NS_CHECK||kind==NS_COLOR)field->hwnd=ns_button(g_ns_hwnd,id,title,x,y,w,h);else if(kind==NS_COMBO)field->hwnd=CreateWindowExW(0,L"COMBOBOX",L"",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS,x,y,w,240,g_ns_hwnd,(HMENU)(INT_PTR)id,g_instance,NULL);else field->hwnd=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL|(kind==NS_NUMBER?ES_NUMBER:0)|(kind==NS_HOTKEY?ES_READONLY:0),x,y,w,h,g_ns_hwnd,(HMENU)(INT_PTR)id,g_instance,NULL);SendMessageW(field->hwnd,WM_SETFONT,(WPARAM)g_ns_font,FALSE);if(kind==NS_NUMBER){field->min=1;field->max=100;SendMessageW(field->hwnd,EM_SETLIMITTEXT,4,0);}return field;}
static void ns_set_wide(HWND hwnd,const wchar_t *text){wchar_t current[768];GetWindowTextW(hwnd,current,768);if(wcscmp(current,text))SetWindowTextW(hwnd,text);}
static void ns_set_text(HWND hwnd,const char *utf8){wchar_t text[320];translate(utf8,text,320);ns_set_wide(hwnd,text);}
static void ns_show(HWND hwnd,int visible){if(((GetWindowLongPtrW(hwnd,GWL_STYLE)&WS_VISIBLE)!=0)!=visible)ShowWindow(hwnd,visible?SW_SHOWNA:SW_HIDE);}
static void ns_move(HWND hwnd,int x,int y,int w,int h){RECT old;GetWindowRect(hwnd,&old);MapWindowPoints(NULL,GetParent(hwnd),(POINT*)&old,2);x=ui_scale(g_ns_hwnd,x);y=ui_scale(g_ns_hwnd,y);w=ui_scale(g_ns_hwnd,w);h=ui_scale(g_ns_hwnd,h);if(old.left!=x||old.top!=y||old.right-old.left!=w||old.bottom-old.top!=h)SetWindowPos(hwnd,NULL,x,y,w,h,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);}
static void ns_language_caption(NSLanguage *language,wchar_t *caption,int capacity){const wchar_t *name=!wcsncmp(language->code,L"pt-",3)?L"Português":language->name;swprintf(caption,capacity,L"%ls — %ls",language->country,name);int cjk=!wcsncmp(language->code,L"zh-",3)||!wcsncmp(language->code,L"ja-",3)||!wcsncmp(language->code,L"ko-",3);if(!cjk)return;HDC dc=GetDC(g_ns_hwnd);HGDIOBJ old=SelectObject(dc,g_ns_font);int missing=ui_missing_glyphs(dc,caption);SelectObject(dc,old);ReleaseDC(g_ns_hwnd,dc);if(missing){if(!wcscmp(language->code,L"zh-CN"))name=L"Chinês (simplificado)";else if(!wcscmp(language->code,L"zh-TW"))name=L"Chinês (tradicional)";else if(!wcsncmp(language->code,L"ja-",3))name=L"Japonês";else name=L"Coreano";swprintf(caption,capacity,L"%ls — %ls",language->country,name);}}
static void ns_create_fields(void){
  g_ns_count=0;
  const struct{const char *key,*label;int fallback;} general[]={
    {"AlwaysOnTop","Manter a janela principal sempre na frente",0},
    {"StartWithWindows","Carregar com o sistema",1},
    {"StartMinimized","Iniciar minimizado",0},
    {"ConfirmEveryClean","Confirmar o início da liberação de memória",0},
    {"SkipUac","Executar operações administrativas com menos solicitações",1},
    {"CloseToTray","Ao fechar, continuar na bandeja",1},
    {"DarkTheme","Usar tema escuro",1},
    {"WindowBlur","Usar transparência nas janelas",0}
  };
  const int positions[]={289,91,131,510,550,171,339,379};
  for(int i=0;i<8;++i)ns_add(NS_FIRST+i,NS_CHECK,0,general[i].key,general[i].label,236,positions[i],650,28,NS_STRETCH,general[i].fallback);
  g_ns_authorize=ns_button(g_ns_hwnd,NS_AUTHORIZE,L"",720,597,205,28);
  ns_set_text(g_ns_authorize,"Autorizar Orion Memory");
  g_ns_language_box=ns_add(NS_FIRST+10,NS_COMBO,0,"Language","Idioma",650,702,278,30,NS_RIGHT,0)->hwnd;
  for(int i=0;i<9;++i){
     NSField *f=ns_add(NS_FIRST+20+i,NS_CHECK,1,region_keys[i],region_labels[i],0,0,690,NS_REGION_HEIGHT,NS_STRETCH,default_regions[i]);
     f->region=i;SetParent(f->hwnd,g_ns_region);
  }
  ns_add(NS_FIRST+29,NS_CHECK,1,"EnableHotkey","Usar atalho global",236,782,276,28,0,1);
  g_ns_hotkey=ns_add(NS_FIRST+30,NS_HOTKEY,1,NULL,"",696,783,225,28,NS_RIGHT,0)->hwnd;
  g_ns_font_box=ns_add(NS_FIRST+40,NS_COMBO,2,"TrayFontFamily","Fonte",318,111,325,30,0,0)->hwnd;
  NSField *size=ns_add(NS_FIRST+41,NS_NUMBER,2,"TrayFontSize","Tamanho",702,110,68,26,NS_RIGHT,8);
  size->min=1;size->max=18;g_ns_font_size=size->hwnd;
  const struct{const char *key,*label;int fallback;} style[]={
    {"TrayFontBold","Usar negrito",1},{"TrayTransparent","Usar fundo transparente",0},
    {"TrayBorder","Exibir bordas",0},{"TrayRounded","Exibir cantos arredondados",1},
    {"TrayColorBackground","Usar a cor do nível como fundo",0},{"TrayAntialias","Usar suavização de bordas",1}
  };
  ns_add(NS_FIRST+42,NS_CHECK,2,style[0].key,style[0].label,824,110,124,28,NS_RIGHT,style[0].fallback);
  const int ypos[]={0,248,296,296,344,344};
  const int xpos[]={0,236,236,600,236,600};
  for(int i=1;i<6;++i)ns_add(NS_FIRST+42+i,NS_CHECK,2,style[i].key,style[i].label,xpos[i],ypos[i],340,28,(i==3||i==5)?NS_RIGHT:0,style[i].fallback);
  const char *color_names[]={"Cor do texto","Cor do fundo","Cor do nível de alerta","Cor do nível de perigo"};
  for(int i=0;i<4;++i)ns_add(NS_COLOR_ROLE+i,NS_COLOR,2,NULL,color_names[i],236,478+i*38,260,32,0,0);
  g_ns_hex=ns_add(NS_FIRST+50,NS_COLOR_EDIT,2,NULL,"Exemplo: #6FB5FF",620,628,217,30,NS_STRETCH,0)->hwnd;
  SendMessageW(g_ns_hex,EM_SETLIMITTEXT,7,0);
  for(int i=0;i<8;++i)ns_button(g_ns_hwnd,NS_PALETTE+i,L"",236+i*34,671,27,27);
  NSField *warning=ns_add(NS_FIRST+60,NS_NUMBER,3,"WarningPercent","Acionar nível de alerta em",672,112,66,25,NS_RIGHT,75);warning->min=1;warning->max=98;
  NSField *danger=ns_add(NS_FIRST+61,NS_NUMBER,3,"DangerPercent","Acionar nível de perigo em",672,162,66,25,NS_RIGHT,90);danger->min=2;danger->max=100;
  ns_add(NS_FIRST+62,NS_COMBO,3,"TraySingleClick","Clique simples",638,347,287,29,NS_RIGHT,0);
  ns_add(NS_FIRST+63,NS_COMBO,3,"TrayDoubleClick","Clique duplo",638,399,287,29,NS_RIGHT,1);
  ns_add(NS_FIRST+64,NS_CHECK,3,"ShowNotifications","Exibir resultados da liberação de memória",236,550,350,28,0,1);
  ns_add(NS_FIRST+65,NS_CHECK,3,"NotificationSound","Habilitar sons de notificação",236,610,350,28,0,1);
  ns_add(NS_FIRST+70,NS_CHECK,4,"DebugLog","Registrar resultados em um log de diagnóstico",236,111,650,27,NS_STRETCH,0);
  ns_add(NS_FIRST+71,NS_CHECK,4,"ConfirmAggressiveClean","Confirmar antes de operações agressivas",236,155,650,27,NS_STRETCH,1);
  ns_add(NS_FIRST+72,NS_CHECK,4,"ShowGraph","Exibir gráfico na janela principal",236,324,650,27,NS_STRETCH,0);
  NSField *refresh=ns_add(NS_FIRST+73,NS_NUMBER,4,"RefreshSeconds","Atualizar métricas a cada (s)",736,463,66,25,NS_RIGHT,1);
  refresh->min=1;refresh->max=5;
  const wchar_t *popular[]={L"Segoe UI",L"Arial",L"Calibri",L"Consolas",L"Tahoma",L"Verdana"};
  for(unsigned i=0;i<sizeof(popular)/sizeof(*popular);++i)SendMessageW(g_ns_font_box,CB_ADDSTRING,0,(LPARAM)popular[i]);
  for(int i=0;i<g_ns_language_count;++i){wchar_t caption[220];ns_language_caption(&g_ns_languages[i],caption,220);SendMessageW(g_ns_language_box,CB_ADDSTRING,0,(LPARAM)caption);}
}
static void ns_refresh_fields(void){
 if(!g_ns_hwnd)return;
 HWND apply_button=GetDlgItem(g_ns_hwnd,NS_APPLY);
 if(apply_button){int dirty=ns_is_dirty()||g_ns_color_dirty;
   if(IsWindowEnabled(apply_button)!=dirty){EnableWindow(apply_button,dirty);InvalidateRect(apply_button,NULL,FALSE);}}
 g_ns_updating=1;
 int theme_changed=g_ns_theme!=g_settings.dark||g_ns_dpi!=GetDpiForWindow(g_ns_hwnd);
 int transparency_changed=g_ns_window_blur!=g_settings.window_blur;
 int language_changed=wcscmp(g_ns_labels_language,g_settings.language)!=0;
 if(theme_changed){g_ns_theme=g_settings.dark;ns_fonts();}
 if(theme_changed||transparency_changed){g_ns_window_blur=g_settings.window_blur;ui_apply_window(g_ns_hwnd);}
 wchar_t text[320];HWND focus=GetFocus();
 if(language_changed){translate("Configurações — Orion Memory",text,320);ns_set_wide(g_ns_hwnd,text);}
 for(int i=0;i<g_ns_count;++i){NSField *f=&g_ns_fields[i];
  if(language_changed&&f->label&&*f->label&&(f->kind==NS_CHECK||f->kind==NS_COLOR))ns_set_text(f->hwnd,f->label);
  if(theme_changed)SendMessageW(f->hwnd,WM_SETFONT,(WPARAM)g_ns_font,FALSE);
  if(f->kind==NS_CHECK){int checked=f->region>=0?g_ns_preview.regions[f->region]:ns_bool(f->key,f->fallback);if(f->checked!=checked||theme_changed||language_changed){f->checked=checked;InvalidateRect(f->hwnd,NULL,FALSE);}}
  else if(f->kind==NS_NUMBER&&focus!=f->hwnd){int value=ns_number(f->key,f->fallback);/* All visible numeric fields read from the draft. */swprintf(text,320,L"%d",clampi(value,f->min,f->max));ns_set_wide(f->hwnd,text);}
 }
 int font_index=(int)SendMessageW(g_ns_font_box,CB_FINDSTRINGEXACT,(WPARAM)-1,(LPARAM)g_ns_preview.font);
 if(font_index==CB_ERR)font_index=(int)SendMessageW(g_ns_font_box,CB_ADDSTRING,0,(LPARAM)g_ns_preview.font);
 if(SendMessageW(g_ns_font_box,CB_GETCURSEL,0,0)!=font_index)SendMessageW(g_ns_font_box,CB_SETCURSEL,font_index,0);
 for(int i=0;i<g_ns_language_count;++i)if(!wcscmp(g_ns_languages[i].code,g_ns_preview.language)){if(SendMessageW(g_ns_language_box,CB_GETCURSEL,0,0)!=i)SendMessageW(g_ns_language_box,CB_SETCURSEL,i,0);break;}
 const char *actions[]={"Nenhuma ação","Exibir / ocultar","Liberar memória"};
 for(int i=0;i<g_ns_count;++i){NSField *f=&g_ns_fields[i];if(f->kind==NS_COMBO&&f->key&&!strncmp(f->key,"Tray",4)&&f->hwnd!=g_ns_font_box){if(language_changed){SendMessageW(f->hwnd,CB_RESETCONTENT,0,0);for(int a=0;a<3;++a){translate(actions[a],text,320);SendMessageW(f->hwnd,CB_ADDSTRING,0,(LPARAM)text);}}int selected=ns_number(f->key,f->fallback);if(SendMessageW(f->hwnd,CB_GETCURSEL,0,0)!=selected)SendMessageW(f->hwnd,CB_SETCURSEL,selected,0);}}
 if(focus!=g_ns_hotkey){wchar_t key[64],caption[128]=L"";UINT scan=MapVirtualKeyW(g_ns_preview.hotkey_key,MAPVK_VK_TO_VSC);if(!GetKeyNameTextW((LONG)(scan<<16),key,64))swprintf(key,64,L"%lc",(wchar_t)g_ns_preview.hotkey_key);if(g_ns_preview.hotkey_modifiers&2)wcscat(caption,L"Ctrl + ");if(g_ns_preview.hotkey_modifiers&1)wcscat(caption,L"Alt + ");if(g_ns_preview.hotkey_modifiers&4)wcscat(caption,L"Shift + ");if(g_ns_preview.hotkey_modifiers&8)wcscat(caption,L"Win + ");wcsncat(caption,key,60);ns_set_wide(g_ns_hotkey,caption);}
 COLORREF loaded[]={g_ns_preview.text_color,g_ns_preview.background_color,g_ns_preview.warning_color,g_ns_preview.danger_color};int color_changed=0;
 for(int i=0;i<4;++i)if((!g_ns_color_dirty||i!=g_ns_color_role)&&g_ns_colors[i]!=loaded[i]){g_ns_colors[i]=loaded[i];color_changed=1;InvalidateRect(GetDlgItem(g_ns_hwnd,NS_COLOR_ROLE+i),NULL,FALSE);}
 if(focus!=g_ns_hex){COLORREF c=g_ns_colors[g_ns_color_role];swprintf(text,320,L"#%02X%02X%02X",GetRValue(c),GetGValue(c),GetBValue(c));ns_set_wide(g_ns_hex,text);}
 if(color_changed||theme_changed)for(int i=0;i<3;++i)if(g_ns_sliders[i])InvalidateRect(g_ns_sliders[i],NULL,FALSE);
 for(int page=0;page<5;++page){HWND button=GetDlgItem(g_ns_hwnd,NS_NAV+page);if(language_changed){wchar_t caption[128];ns_nav_caption(page,caption,128);ns_set_wide(button,caption);}if(theme_changed)SendMessageW(button,WM_SETFONT,(WPARAM)g_ns_font,FALSE);}
 HWND extra_buttons[]={GetDlgItem(g_ns_hwnd,NS_CLOSE),GetDlgItem(g_ns_hwnd,NS_APPLY),GetDlgItem(g_ns_hwnd,NS_DEFAULTS),g_ns_authorize};const char *extra_labels[]={"Cancelar","Aplicar","Restaurar padrões","Configurar autorização"};
 for(int i=0;i<4;++i){if(language_changed)ns_set_text(extra_buttons[i],extra_labels[i]);if(theme_changed)SendMessageW(extra_buttons[i],WM_SETFONT,(WPARAM)g_ns_font,FALSE);}
 ns_show(g_ns_authorize,g_ns_page==0&&g_ns_preview.skip_uac&&!g_ns_authorized);
 EnableWindow(GetDlgItem(g_ns_hwnd,NS_APPLY),ns_is_dirty());
 if(language_changed)copyw(g_ns_labels_language,32,g_settings.language);
 g_ns_updating=0;
 if(theme_changed)for(int i=0;i<g_ns_count;++i)if(g_ns_spinners[i])InvalidateRect(g_ns_spinners[i],NULL,FALSE);
 if(theme_changed||transparency_changed||language_changed)RedrawWindow(g_ns_hwnd,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);
 else if(g_ns_page==2)InvalidateRect(g_ns_hwnd,NULL,FALSE);
 else if(g_ns_page==1){ns_invalidate(194,342,650,125);for(int i=0;i<3;++i)InvalidateRect(GetDlgItem(g_ns_hwnd,NS_PROFILE_LEVE+i),NULL,FALSE);}
}
static void ns_layout(void){
  if(!g_ns_hwnd)return;
  RECT rect;GetClientRect(g_ns_hwnd,&rect);
  int width=MulDiv(rect.right,96,(int)GetDpiForWindow(g_ns_hwnd));
  int height=MulDiv(rect.bottom,96,(int)GetDpiForWindow(g_ns_hwnd));
  int extra=width-970,scroll=ns_scroll();
  for(int i=0;i<5;++i)ns_move(GetDlgItem(g_ns_hwnd,NS_NAV+i),16,32+i*45,176,38);
  ns_move(GetDlgItem(g_ns_hwnd,NS_APPLY),width-122,height-49,106,36);
  ns_move(GetDlgItem(g_ns_hwnd,NS_CLOSE),width-249,height-49,114,36);
  ns_move(GetDlgItem(g_ns_hwnd,NS_DEFAULTS),width-420,height-49,158,36);
  for(int i=0;i<3;++i){int left=230+i*((width-270)/3+6);int w=(width-270)/3-8;
    ns_move(GetDlgItem(g_ns_hwnd,NS_PROFILE_LEVE+i),left,85-scroll,w,60);
    ns_show(GetDlgItem(g_ns_hwnd,NS_PROFILE_LEVE+i),g_ns_page==1&&ns_page_visible(85,60));
  }
  ns_move(GetDlgItem(g_ns_hwnd,NS_TEST_NOTIFICATION),240,713-scroll,186,32);
  ns_show(GetDlgItem(g_ns_hwnd,NS_TEST_NOTIFICATION),g_ns_page==3&&ns_page_visible(713,32));
  ns_move(GetDlgItem(g_ns_hwnd,NS_COPY_HEX),width-118,628-scroll,78,30);
  ns_show(GetDlgItem(g_ns_hwnd,NS_COPY_HEX),g_ns_page==2&&ns_page_visible(628,30));
  for(int i=0;i<g_ns_count;++i){NSField *f=&g_ns_fields[i];
    int x=f->x+((f->anchor&NS_RIGHT)?extra:0),y=f->y-scroll;
    int w=f->w+((f->anchor&NS_STRETCH)?extra:0),h=f->h;
    if(f->kind==NS_NUMBER&&g_ns_spinners[i]){
      HWND spinner=g_ns_spinners[i];ns_move(spinner,x-8,y-2,w+40,32);
      int sw=ui_scale(spinner,w+40),sh=ui_scale(spinner,32);
      SetWindowPos(f->hwnd,NULL,ui_scale(spinner,10),ui_scale(spinner,7),
         max(20,sw-ui_scale(spinner,48)),max(15,sh-ui_scale(spinner,14)),SWP_NOZORDER|SWP_NOACTIVATE);
      int enabled=f->page==g_ns_page&&ns_page_visible(f->y-2,32);
      ns_show(spinner,enabled);ns_show(f->hwnd,enabled);
    }else{
      if(f->region>=0)ns_move(f->hwnd,8,f->region*NS_REGION_ROW-g_ns_region_scroll,width-277,NS_REGION_HEIGHT);
      else ns_move(f->hwnd,x,y,max(30,w),f->kind==NS_COMBO?240:h);
      int enabled=f->page==g_ns_page&&(f->region>=0||ns_page_visible(f->y,h));
      ns_show(f->hwnd,enabled);
    }
  }
  ns_move(g_ns_region,227,258-scroll,width-264,308);
  ns_show(g_ns_region,g_ns_page==1&&ns_page_visible(258,308));
  ns_move(g_ns_authorize,width-250,591-scroll,214,31);
  ns_show(g_ns_authorize,g_ns_page==0&&g_ns_preview.skip_uac&&!g_ns_authorized&&ns_page_visible(591,31));
  for(int i=0;i<8;++i){HWND button=GetDlgItem(g_ns_hwnd,NS_PALETTE+i);
    ns_move(button,236+i*34,671-scroll,27,27);ns_show(button,g_ns_page==2&&ns_page_visible(671,27));}
  for(int i=0;i<3;++i){ns_move(g_ns_sliders[i],635,493+i*44-scroll,width-662,26);
    ns_show(g_ns_sliders[i],g_ns_page==2&&ns_page_visible(493+i*44,26));}
  ns_move(GetDlgItem(g_ns_hwnd,NS_OPEN_LOG),236,208-scroll,186,31);
  ns_show(GetDlgItem(g_ns_hwnd,NS_OPEN_LOG),g_ns_page==4&&ns_page_visible(208,31));
  RedrawWindow(g_ns_hwnd,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);
}
static void ns_label(HDC dc,const char *label,int x,int y,int w,int h,HFONT font,COLORREF color,UINT flags){
  wchar_t text[768];translate(label,text,768);
  RECT r={ui_scale(g_ns_hwnd,x),ui_scale(g_ns_hwnd,y),ui_scale(g_ns_hwnd,x+w),ui_scale(g_ns_hwnd,y+h)};
  ui_text_draw(dc,r,text,font,color,flags|DT_NOPREFIX);
}
static void ns_card(HDC dc,int y,int h,const char *title,const char *subtitle){
  RECT client;GetClientRect(g_ns_hwnd,&client);
  RECT r={ui_scale(g_ns_hwnd,209),ui_scale(g_ns_hwnd,y),client.right-ui_scale(g_ns_hwnd,17),ui_scale(g_ns_hwnd,y+h)};
  ui_round_border(dc,r,ui_surface(),ui_outline(),ui_scale(g_ns_hwnd,10));
  int w=MulDiv(client.right,96,(int)GetDpiForWindow(g_ns_hwnd));
  ns_label(dc,title,233,y+16,w-265,26,g_ns_bold,ui_text(),DT_SINGLELINE|DT_VCENTER);
  if(subtitle&&*subtitle)ns_label(dc,subtitle,233,y+44,w-265,28,g_ns_font,ui_muted(),DT_SINGLELINE|DT_END_ELLIPSIS|DT_VCENTER);
}
static void ns_info_line(HDC dc,const char *title,int x,int y,int w){
  ns_label(dc,title,x,y,w,21,g_ns_font,ui_muted(),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
}
static void ns_draw_preview(HDC dc,int x,int y,int width,int light,HICON icon){
  RECT r={ui_scale(g_ns_hwnd,x),ui_scale(g_ns_hwnd,y),ui_scale(g_ns_hwnd,x+width),ui_scale(g_ns_hwnd,y+68)};
  COLORREF bg=light?RGB(233,239,246):RGB(20,30,48);
  ui_round_border(dc,r,bg,ui_outline(),ui_scale(g_ns_hwnd,7));
  int icon_size=ui_scale(g_ns_hwnd,25);
  if(icon)DrawIconEx(dc,ui_scale(g_ns_hwnd,x+12),ui_scale(g_ns_hwnd,y+26),icon,icon_size,icon_size,0,NULL,DI_NORMAL);
  RECT label={ui_scale(g_ns_hwnd,x+52),ui_scale(g_ns_hwnd,y+27),ui_scale(g_ns_hwnd,x+width-8),ui_scale(g_ns_hwnd,y+51)};
  ui_text_draw(dc,label,L"14:25",g_ns_font,light?RGB(26,31,38):RGB(230,236,245),DT_VCENTER|DT_SINGLELINE);
  ns_label(dc,light?"TEMA CLARO":"TEMA ESCURO",x+12,y+4,width-24,18,g_ns_font,light?RGB(55,65,77):ui_muted(),DT_SINGLELINE);
}
static void ns_draw_usage_meter(HDC dc,int x,int y,int w,int warning,int danger){
  int from=x;
  for(int i=0;i<32;++i){
    int percent=i*100/32;
    COLORREF c=percent<warning?RGB(91,194,141):percent<danger?RGB(245,187,76):RGB(248,109,106);
    RECT block={ui_scale(g_ns_hwnd,from+i*w/32),ui_scale(g_ns_hwnd,y),ui_scale(g_ns_hwnd,from+(i+1)*w/32),ui_scale(g_ns_hwnd,y+8)};
    ui_fill(dc,block,c);
  }
  ns_label(dc,"Normal",x,y+18,w/3,19,g_ns_font,ui_muted(),DT_SINGLELINE);
  ns_label(dc,"Alerta",x+w/3,y+18,w/3,19,g_ns_font,ui_muted(),DT_CENTER|DT_SINGLELINE);
  ns_label(dc,"Perigo",x+2*w/3,y+18,w/3,19,g_ns_font,ui_muted(),DT_RIGHT|DT_SINGLELINE);
}
static void ns_paint(HWND hwnd,HDC dc){
  RECT client;GetClientRect(hwnd,&client);ui_fill(dc,client,ui_background());
  int w=MulDiv(client.right,96,(int)GetDpiForWindow(hwnd));
  int h=MulDiv(client.bottom,96,(int)GetDpiForWindow(hwnd));
  RECT nav={ui_scale(hwnd,8),ui_scale(hwnd,10),ui_scale(hwnd,201),client.bottom-ui_scale(hwnd,64)};
  ui_round_border(dc,nav,ui_surface(),ui_outline(),ui_scale(hwnd,10));
  int saved=SaveDC(dc);
  IntersectClipRect(dc,ui_scale(hwnd,205),ui_scale(hwnd,10),client.right-ui_scale(hwnd,12),ui_scale(hwnd,ns_view_bottom()));
  SetViewportOrgEx(dc,0,-ui_scale(hwnd,ns_scroll()),NULL);
  if(g_ns_page==0){
    ns_card(dc,18,194,"Inicialização","Defina como o Orion Memory inicia junto com o Windows.");
    ns_info_line(dc,"Inicia automaticamente com o sistema.",263,113,w-300);
    ns_info_line(dc,"Abre minimizado na área de notificação.",263,153,w-300);
    ns_info_line(dc,"Mantém o aplicativo em execução na bandeja.",263,193,w-300);
    ns_card(dc,225,192,"Janela e aparência","Personalize o comportamento e a apresentação da janela.");
    ns_info_line(dc,"Mantém a janela visível acima das demais.",263,311,w-300);
    ns_info_line(dc,"Aplica o tema escuro à interface.",263,361,w-300);
    ns_info_line(dc,"Usa transparência quando suportada pelo Windows.",263,401,w-300);
    ns_card(dc,430,207,"Segurança","Proteja suas configurações e gerencie autorizações.");
    ns_info_line(dc,"Solicita confirmação antes da liberação de memória.",263,531,w-300);
    ns_info_line(dc,"Configuração autorizada, sem desativar o UAC do Windows.",263,574,w-300);
    RECT status={ui_scale(hwnd,233),ui_scale(hwnd,592),client.right-ui_scale(hwnd,25),ui_scale(hwnd,625)};
    ui_round_border(dc,status,ui_background(),ui_outline(),ui_scale(hwnd,6));
    ns_label(dc,g_ns_authorized?"Autorização administrativa configurada":"Autorização administrativa ainda não configurada",245,597,w-540,25,g_ns_font,g_ns_authorized?RGB(91,194,141):ui_muted(),DT_SINGLELINE|DT_END_ELLIPSIS|DT_VCENTER);
    ns_card(dc,650,97,"Idioma","Selecione o idioma do aplicativo.");
    ns_label(dc,"Idioma do aplicativo:",469,706,182,26,g_ns_font,ui_text(),DT_SINGLELINE|DT_VCENTER);
  }
  else if(g_ns_page==1){
    ns_card(dc,18,138,"Perfil de limpeza","Escolha um perfil para selecionar as operações manuais.");
    ns_card(dc,167,419,"Áreas incluídas na limpeza","Nove operações com riscos diferentes. Passe o mouse para ver os detalhes.");
    ns_card(dc,598,137,"Informações sobre a limpeza","Impacto possível da operação selecionada (sem promessa de mais FPS).");
    int selected=g_ns_region_hint>=0&&g_ns_region_hint<9?g_ns_region_hint:4;
    int risk=ns_region_risk[selected];
    ns_label(dc,region_labels[selected],234,665,w-315,23,g_ns_bold,ui_text(),DT_SINGLELINE|DT_END_ELLIPSIS);
    ns_label(dc,ns_risk_names[risk],w-192,665,157,23,g_ns_bold,ns_risk_color(risk),DT_RIGHT|DT_SINGLELINE);
    ns_label(dc,ns_region_details[selected],234,694,w-275,36,g_ns_font,ui_muted(),DT_WORDBREAK);
    ns_card(dc,744,75,"Atalho global",NULL);
    ns_label(dc,"Liberar memória manualmente com uma combinação de teclas.",234,775,446,21,g_ns_font,ui_muted(),DT_SINGLELINE);
  }
  else if(g_ns_page==2){
    ns_card(dc,18,138,"Tipografia","Defina a fonte, o tamanho e o peso do indicador.");
    ns_label(dc,"Fonte:",237,112,78,28,g_ns_font,ui_text(),DT_VCENTER|DT_SINGLELINE);
    ns_label(dc,"Tamanho:",646,111,66,28,g_ns_font,ui_text(),DT_VCENTER|DT_SINGLELINE);
    ns_label(dc,"pt",w-134,112,24,26,g_ns_font,ui_muted(),DT_VCENTER|DT_SINGLELINE);
    ns_card(dc,168,220,"Estilo do ícone","Configure a apresentação do indicador na bandeja do Windows.");
    ns_info_line(dc,"Integrado ao visual do Windows.",263,272,318);
    ns_info_line(dc,"Adiciona uma borda ao indicador.",263,319,318);
    ns_info_line(dc,"Aplica a cor do nível ao fundo.",263,369,318);
    ns_info_line(dc,"Mantém cantos e linhas suaves.",630,319,310);
    ns_card(dc,400,412,"Paleta e personalização","Ajuste as cores e confira a prévia sem modificar o Windows.");
    ns_label(dc,"Personalizar cor (R / G / B)",625,461,w-663,24,g_ns_font,ui_muted(),DT_SINGLELINE);
    ns_label(dc,"R",612,494,21,25,g_ns_font,ui_text(),DT_SINGLELINE|DT_VCENTER);
    ns_label(dc,"G",612,538,21,25,g_ns_font,ui_text(),DT_SINGLELINE|DT_VCENTER);
    ns_label(dc,"B",612,582,21,25,g_ns_font,ui_text(),DT_SINGLELINE|DT_VCENTER);
    ns_label(dc,"Prévia do indicador na barra de tarefas",610,677,w-637,21,g_ns_font,ui_muted(),DT_SINGLELINE);
    Settings prior=g_settings;g_settings=g_ns_preview;
    g_settings.text_color=g_ns_colors[0];g_settings.background_color=g_ns_colors[1];
    g_settings.warning_color=g_ns_colors[2];g_settings.danger_color=g_ns_colors[3];
    int usage=g_ns_color_role==2?g_settings.warning:g_ns_color_role==3?g_settings.danger:min(74,g_settings.warning-1);
    g_settings.dark=1;HICON icon_dark=render_icon(usage,ui_scale(hwnd,25));
    g_settings.dark=0;HICON icon_light=render_icon(usage,ui_scale(hwnd,25));
    g_settings=prior;
    int card_width=(w-635)/2-9;
    ns_draw_preview(dc,611,709,card_width,0,icon_dark);
    ns_draw_preview(dc,621+card_width,709,card_width,1,icon_light);
    if(icon_dark)DestroyIcon(icon_dark);if(icon_light)DestroyIcon(icon_light);
  }
  else if(g_ns_page==3){
    ns_card(dc,18,228,"Níveis de utilização","Configure os percentuais de alerta e perigo no indicador.");
    ns_label(dc,"Acionar o nível de alerta em:",238,110,340,28,g_ns_font,ui_text(),DT_SINGLELINE|DT_VCENTER);
    ns_label(dc,"Acionar o nível de perigo em:",238,160,340,28,g_ns_font,ui_text(),DT_SINGLELINE|DT_VCENTER);
    ns_draw_usage_meter(dc,w-275,118,235,g_ns_preview.warning,g_ns_preview.danger);
    ns_label(dc,"Os níveis informam o uso da RAM. Não executam limpeza automática.",238,206,w-280,22,g_ns_font,ui_muted(),DT_SINGLELINE);
    ns_card(dc,258,197,"Interações com a bandeja","Escolha o que acontece ao clicar no indicador.");
    ns_label(dc,"Clique simples:",238,347,285,29,g_ns_font,ui_text(),DT_SINGLELINE|DT_VCENTER);
    ns_label(dc,"Clique duplo:",238,399,285,29,g_ns_font,ui_text(),DT_SINGLELINE|DT_VCENTER);
    ns_card(dc,467,345,"Notificações","Configure os avisos exibidos após uma limpeza manual.");
    ns_info_line(dc,"Mostra o resultado de cada operação.",265,572,345);
    ns_info_line(dc,"Reproduz um som ao concluir a operação.",265,632,345);
    ns_label(dc,"Prévia de notificação (exemplo)",625,543,w-660,22,g_ns_font,ui_muted(),DT_SINGLELINE);
    RECT bubble={ui_scale(hwnd,620),ui_scale(hwnd,574),client.right-ui_scale(hwnd,28),ui_scale(hwnd,704)};
    ui_round_border(dc,bubble,ui_background(),ui_outline(),ui_scale(hwnd,8));
    ns_label(dc,"Orion Memory",637,589,w-684,25,g_ns_bold,ui_text(),DT_SINGLELINE);
    ns_label(dc,"Limpeza concluída (exemplo)",637,623,w-684,25,g_ns_font,ui_text(),DT_SINGLELINE);
    ns_label(dc,"Nenhuma limpeza foi executada.",637,659,w-684,25,g_ns_font,ui_muted(),DT_SINGLELINE);
    ns_label(dc,"Teste a notificação sem alterar a memória do computador.",438,716,190,40,g_ns_font,ui_muted(),DT_WORDBREAK);
  }
  else{
    ns_card(dc,18,238,"Diagnóstico e segurança","Registre informações e confirme operações potencialmente invasivas.");
    ns_info_line(dc,"Salva informações locais sobre a limpeza de memória.",263,135,w-300);
    ns_info_line(dc,"Exibe avisos antes de procedimentos avançados.",263,180,w-300);
    ns_label(dc,"Operações agressivas podem provocar lentidão e uso intenso de disco.",465,218,w-500,28,g_ns_font,ns_risk_color(1),DT_WORDBREAK);
    ns_card(dc,267,125,"Janela principal","Preferências da janela de monitoramento.");
    ns_info_line(dc,"Exibe um gráfico das métricas de RAM quando a janela estiver aberta.",263,350,w-300);
    ns_card(dc,403,123,"Frequência de atualização","Defina o intervalo de consulta das métricas.");
    ns_label(dc,"Atualizar métricas a cada:",236,460,300,29,g_ns_font,ui_text(),DT_SINGLELINE|DT_VCENTER);
    ns_label(dc,"segundo(s)",w-137,463,98,26,g_ns_font,ui_muted(),DT_SINGLELINE|DT_VCENTER);
    ns_card(dc,540,264,"Informações técnicas","Dados locais do Orion Memory e do sistema.");
    ns_label(dc,"Plataforma",238,613,206,24,g_ns_font,ui_muted(),DT_SINGLELINE);
    ns_label(dc,"Windows x64 · C / Win32",238,641,320,29,g_ns_bold,ui_text(),DT_SINGLELINE);
    ns_label(dc,"Autorização",579,613,200,24,g_ns_font,ui_muted(),DT_SINGLELINE);
    ns_label(dc,g_ns_authorized?"Configurada":"Pendente",579,641,170,27,g_ns_bold,g_ns_authorized?RGB(91,194,141):ns_risk_color(1),DT_SINGLELINE);
    PROCESS_MEMORY_COUNTERS_EX proc;memset(&proc,0,sizeof(proc));
    if(GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*)&proc,sizeof(proc))){
      wchar_t measure[128];swprintf(measure,128,L"RAM residente: %.2f MB",(double)proc.WorkingSetSize/(1024.0*1024.0));
      RECT area={ui_scale(hwnd,238),ui_scale(hwnd,706),client.right-ui_scale(hwnd,30),ui_scale(hwnd,738)};
      ui_text_draw(dc,area,measure,g_ns_font,ui_text(),DT_SINGLELINE|DT_VCENTER);
    }
    ns_label(dc,"A instalação não exige DLLs de terceiros.",238,755,w-270,25,g_ns_font,ui_muted(),DT_SINGLELINE);
  }
  RestoreDC(dc,saved);
  /* Always-fixed bottom bar; no decorative icons or bitmaps. */
  RECT footer={0,client.bottom-ui_scale(hwnd,59),client.right,client.bottom};
  ui_fill(dc,footer,ui_surface());
  HPEN line=CreatePen(PS_SOLID,1,ui_outline());HGDIOBJ old=SelectObject(dc,line);
  MoveToEx(dc,0,footer.top,NULL);LineTo(dc,footer.right,footer.top);SelectObject(dc,old);DeleteObject(line);
  RECT message={ui_scale(hwnd,20),footer.top,client.right-ui_scale(hwnd,440),footer.bottom};
  ui_text_draw(dc,message,g_ns_status,g_ns_font,ns_is_dirty()?ui_accent():ui_muted(),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
  /* Fixed, discreet scroll indicator (GDI only). */
  int max_scroll=ns_scroll_max();
  if(max_scroll>0){
    int top=ui_scale(hwnd,15),body=ui_scale(hwnd,ns_view_bottom()-26);
    int thumb=max(ui_scale(hwnd,34),MulDiv(body,ns_view_bottom()-15,g_ns_page_bottom[g_ns_page]));
    int offset=MulDiv(max(0,body-thumb),ns_scroll(),max_scroll);
    RECT bar={client.right-ui_scale(hwnd,11),top+offset,client.right-ui_scale(hwnd,7),top+offset+thumb};
    ui_round(dc,bar,ui_muted(),ui_scale(hwnd,2));
  }
}

static LRESULT ns_draw(DRAWITEMSTRUCT *draw){int id=(int)draw->CtlID;HDC dc=draw->hDC;RECT r=draw->rcItem;ui_fill(dc,r,(id==NS_CLOSE||id==NS_DEFAULTS||id==NS_APPLY)?ui_background():ui_surface());NSField *f=ns_field(id);wchar_t text[320];GetWindowTextW(draw->hwndItem,text,320);
 if(draw->CtlType==ODT_COMBOBOX){int selected=(draw->itemState&ODS_SELECTED)!=0;ui_round(dc,r,selected?ui_selection_bg():ui_surface(),ui_scale(g_ns_hwnd,4));if(draw->itemID!=(UINT)-1)SendMessageW(draw->hwndItem,CB_GETLBTEXT,draw->itemID,(LPARAM)text);r.left+=ui_scale(g_ns_hwnd,8);ui_text_draw(dc,r,text,g_ns_font,selected?ui_selection_fg():ui_text(),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);return TRUE;}
 if(id>=NS_NAV&&id<NS_NAV+5){int active=id==NS_NAV+g_ns_page;ui_round(dc,r,active?ui_selection_bg():GetPropW(draw->hwndItem,L"Orion.Hover")?(g_settings.dark?RGB(50,50,50):RGB(246,246,246)):ui_surface(),ui_scale(g_ns_hwnd,5));if(active){RECT marker={r.left+ui_scale(g_ns_hwnd,3),r.top+ui_scale(g_ns_hwnd,8),r.left+ui_scale(g_ns_hwnd,6),r.bottom-ui_scale(g_ns_hwnd,8)};ui_round(dc,marker,ui_accent(),ui_scale(g_ns_hwnd,2));}r.left+=ui_scale(g_ns_hwnd,12);ui_text_draw(dc,r,text,g_ns_font,ui_selection_fg(),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);return TRUE;}
 if(id==NS_AUTHORIZE){
    ui_fill(dc,r,ui_surface());
    RECT button=r;InflateRect(&button,-1,-1);
    int hover=GetPropW(draw->hwndItem,L"Orion.Hover")!=NULL;
    COLORREF fill=hover?(g_settings.dark?RGB(47,54,65):RGB(226,237,250)):(g_settings.dark?RGB(34,37,43):RGB(248,249,251));
    ui_round_border(dc,button,fill,(draw->itemState&ODS_FOCUS)?ui_focus_outline():ui_outline(),ui_scale(g_ns_hwnd,6));
    ui_text_draw(dc,button,text,g_ns_font,ui_text(),DT_CENTER|DT_SINGLELINE|DT_VCENTER);
    return TRUE;
 }
 if(id>=NS_PALETTE&&id<NS_PALETTE+8){ui_round(dc,r,ns_palette[id-NS_PALETTE],ui_scale(g_ns_hwnd,3));return TRUE;}
 if(f&&(f->kind==NS_CHECK||f->kind==NS_COLOR)){int checked=f->kind==NS_COLOR?id==NS_COLOR_ROLE+g_ns_color_role:f->checked;ui_round(dc,r,f->kind==NS_COLOR&&checked?ui_selection_bg():ui_surface(),ui_scale(g_ns_hwnd,5));int sz=ui_scale(g_ns_hwnd,17),left=r.left+ui_scale(g_ns_hwnd,2),top=r.top+(r.bottom-r.top-sz)/2;RECT box={left,top,left+sz,top+sz};ui_round_border(dc,box,f->kind==NS_COLOR?g_ns_colors[id-NS_COLOR_ROLE]:checked?ui_accent():ui_background(),(draw->itemState&ODS_FOCUS)?ui_focus_outline():checked?ui_accent():ui_outline(),ui_scale(g_ns_hwnd,4));if(checked&&f->kind==NS_CHECK){HPEN pen=CreatePen(PS_SOLID,max(1,ui_scale(g_ns_hwnd,2)),g_settings.dark?RGB(15,25,35):RGB(255,255,255));HGDIOBJ old=SelectObject(dc,pen);MoveToEx(dc,left+ui_scale(g_ns_hwnd,4),top+ui_scale(g_ns_hwnd,8),NULL);LineTo(dc,left+ui_scale(g_ns_hwnd,7),top+ui_scale(g_ns_hwnd,11));LineTo(dc,left+ui_scale(g_ns_hwnd,13),top+ui_scale(g_ns_hwnd,5));SelectObject(dc,old);DeleteObject(pen);}r.left=box.right+ui_scale(g_ns_hwnd,8);
 if(f->region>=0){
    int risk=ns_region_risk[f->region];
    int badgeWidth=ui_scale(g_ns_hwnd,111),detailWidth=ui_scale(g_ns_hwnd,208);
    RECT badge={r.right-badgeWidth-ui_scale(g_ns_hwnd,5),r.top+ui_scale(g_ns_hwnd,4),r.right-ui_scale(g_ns_hwnd,5),r.bottom-ui_scale(g_ns_hwnd,4)};
    RECT name=r;name.right=max(name.left+ui_scale(g_ns_hwnd,135),badge.left-detailWidth-ui_scale(g_ns_hwnd,8));
    ui_text_draw(dc,name,text,g_ns_font,ui_text(),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
    RECT detail=r;detail.left=name.right+ui_scale(g_ns_hwnd,5);detail.right=badge.left-ui_scale(g_ns_hwnd,6);
    wchar_t desc[160];translate(ns_region_short[f->region],desc,160);
    ui_text_draw(dc,detail,desc,g_ns_font,ui_muted(),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
    ui_round(dc,badge,ui_background(),ui_scale(g_ns_hwnd,7));
    const wchar_t *tag=risk==0?L"Recomendado":risk==1?L"Cautela":L"Pode travar";
    ui_text_draw(dc,badge,tag,g_ns_font,ns_risk_color(risk),DT_CENTER|DT_SINGLELINE|DT_VCENTER);
 }else ui_text_draw(dc,r,text,g_ns_font,ui_text(),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
 return TRUE;}
 if((id>=NS_NUMBER_UP&&id<NS_NUMBER_UP+g_ns_count)||(id>=NS_NUMBER_DOWN&&id<NS_NUMBER_DOWN+g_ns_count)){ui_round(dc,r,GetPropW(draw->hwndItem,L"Orion.Hover")?ui_selection_bg():ui_surface(),ui_scale(g_ns_hwnd,3));ui_text_draw(dc,r,text,g_ns_font,ui_muted(),DT_CENTER|DT_SINGLELINE|DT_VCENTER);return TRUE;}
 if(id>=NS_PROFILE_LEVE&&id<=NS_PROFILE_PERSONALIZADO){
    int selected=ns_profile_selected()==id-NS_PROFILE_LEVE;
    ui_round_border(dc,r,selected?ui_selection_bg():ui_surface(),selected?ui_accent():ui_outline(),ui_scale(g_ns_hwnd,6));
    RECT name=r;name.bottom=r.top+(r.bottom-r.top)*2/3;
    ui_text_draw(dc,name,text,g_ns_bold,ui_text(),DT_SINGLELINE|DT_VCENTER|DT_CENTER);
    RECT subtitle=r;subtitle.top=name.bottom-ui_scale(g_ns_hwnd,4);
    const wchar_t *hint=id==NS_PROFILE_LEVE?L"Menor impacto":id==NS_PROFILE_EQUILIBRADO?L"Seleção equilibrada":L"Seleção manual";
    ui_text_draw(dc,subtitle,hint,g_ns_font,ui_muted(),DT_SINGLELINE|DT_CENTER|DT_VCENTER);
    return TRUE;
 }
 if(id==NS_APPLY){
   int enabled=IsWindowEnabled(draw->hwndItem);
   ui_round_border(dc,r,enabled?ui_selection_bg():ui_surface(),enabled?ui_accent():ui_outline(),ui_scale(g_ns_hwnd,6));
   ui_text_draw(dc,r,text,g_ns_font,enabled?ui_text():ui_muted(),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
   return TRUE;
 }ui_draw_button(draw);return TRUE;}
static int ns_hex_color(const wchar_t *input,COLORREF *color){const wchar_t *p=input;if(*p=='#')++p;if(wcslen(p)!=6)return 0;unsigned value=0;for(int i=0;i<6;++i){int h=p[i]>='0'&&p[i]<='9'?p[i]-'0':p[i]>='a'&&p[i]<='f'?p[i]-'a'+10:p[i]>='A'&&p[i]<='F'?p[i]-'A'+10:-1;if(h<0)return 0;value=(value<<4)|(unsigned)h;}*color=RGB((value>>16)&255,(value>>8)&255,value&255);return 1;}
static void ns_color_text(void){if(!g_ns_hex)return;COLORREF c=g_ns_colors[g_ns_color_role];wchar_t code[16];swprintf(code,16,L"#%02X%02X%02X",GetRValue(c),GetGValue(c),GetBValue(c));g_ns_updating=1;SetWindowTextW(g_ns_hex,code);g_ns_updating=0;for(int i=0;i<3;++i)InvalidateRect(g_ns_sliders[i],NULL,FALSE);InvalidateRect(g_ns_hwnd,NULL,FALSE);}
static void ns_flush_color(void){if(!g_ns_color_dirty)return;KillTimer(g_ns_hwnd,NS_COLOR_TIMER);g_ns_color_dirty=0;COLORREF c=g_ns_colors[g_ns_color_role];char raw[16];snprintf(raw,sizeof(raw),"\"#%02X%02X%02X\"",GetRValue(c),GetGValue(c),GetBValue(c));const char *keys[]={"TrayTextColor","TrayBackgroundColor","TrayWarningColor","TrayDangerColor"};ns_save(keys[g_ns_color_role],raw);}
static void ns_edit_color(int show_error){wchar_t code[16];COLORREF color;GetWindowTextW(g_ns_hex,code,16);if(!ns_hex_color(code,&color)){if(show_error)ns_status("Cor HEX inválida");return;}if(g_ns_colors[g_ns_color_role]!=color){g_ns_colors[g_ns_color_role]=color;g_ns_color_dirty=1;ns_flush_color();}for(int i=0;i<3;++i)InvalidateRect(g_ns_sliders[i],NULL,FALSE);InvalidateRect(g_ns_hwnd,NULL,FALSE);}
static int g_ns_region_drag,g_ns_region_drag_y,g_ns_region_drag_scroll;
static int ns_region_range(HWND hwnd){RECT r;GetClientRect(hwnd,&r);return max(0,NS_REGION_CONTENT-MulDiv(r.bottom,96,(int)GetDpiForWindow(hwnd)));}
static void ns_region_geometry(HWND hwnd,RECT *track,RECT *thumb){RECT r;GetClientRect(hwnd,&r);int pad=ui_scale(hwnd,7),height=max(1,r.bottom-pad*2),visible=MulDiv(r.bottom,96,(int)GetDpiForWindow(hwnd)),range=ns_region_range(hwnd);track->left=r.right-ui_scale(hwnd,8);track->right=track->left+ui_scale(hwnd,4);track->top=pad;track->bottom=r.bottom-pad;*thumb=*track;int length=max(ui_scale(hwnd,24),MulDiv(height,min(NS_REGION_CONTENT,visible),NS_REGION_CONTENT));length=min(length,height);int travel=height-length;thumb->top+=range?MulDiv(travel,g_ns_region_scroll,range):0;thumb->bottom=thumb->top+length;}
static void ns_region_set_scroll(HWND hwnd,int next){next=clampi(next,0,ns_region_range(hwnd));int old=g_ns_region_scroll;if(next==old)return;g_ns_region_scroll=next;ScrollWindowEx(hwnd,0,ui_scale(hwnd,old-next),NULL,NULL,NULL,NULL,SW_SCROLLCHILDREN|SW_INVALIDATE);InvalidateRect(hwnd,NULL,FALSE);}
static void ns_region_ensure_visible(HWND button){RECT child,parent;GetWindowRect(button,&child);MapWindowPoints(NULL,g_ns_region,(POINT*)&child,2);GetClientRect(g_ns_region,&parent);int dpi=(int)GetDpiForWindow(g_ns_region);if(child.top<0)ns_region_set_scroll(g_ns_region,g_ns_region_scroll+MulDiv(child.top,96,dpi));else if(child.bottom>parent.bottom)ns_region_set_scroll(g_ns_region,g_ns_region_scroll+MulDiv(child.bottom-parent.bottom,96,dpi));}
static LRESULT CALLBACK ns_region_button_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){WNDPROC original=(WNDPROC)GetPropW(hwnd,L"Orion.Settings.RegionProc");if(message==WM_GETDLGCODE)return CallWindowProcW(original,hwnd,message,wp,lp)|DLGC_WANTARROWS;if(message==WM_SETFOCUS){ns_region_ensure_visible(hwnd);if(g_ns_region_hint!=GetDlgCtrlID(hwnd)-(NS_FIRST+20)){g_ns_region_hint=GetDlgCtrlID(hwnd)-(NS_FIRST+20);InvalidateRect(g_ns_hwnd,NULL,FALSE);}}
 if(message==WM_MOUSEMOVE){NSField *field=ns_field(GetDlgCtrlID(hwnd));if(field&&g_ns_region_hint!=field->region){g_ns_region_hint=field->region;InvalidateRect(g_ns_hwnd,NULL,FALSE);}TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,hwnd,0};TrackMouseEvent(&t);}
 if(message==WM_MOUSELEAVE){g_ns_region_hint=-1;InvalidateRect(g_ns_hwnd,NULL,FALSE);}if(message==WM_KEYDOWN&&(wp==VK_UP||wp==VK_DOWN||wp==VK_HOME||wp==VK_END||wp==VK_PRIOR||wp==VK_NEXT)){NSField *f=ns_field(GetDlgCtrlID(hwnd));if(f){int next=f->region;if(wp==VK_UP)--next;else if(wp==VK_DOWN)++next;else if(wp==VK_HOME)next=0;else if(wp==VK_END)next=8;else next+=wp==VK_NEXT?7:-7;next=clampi(next,0,8);NSField *target=ns_field(NS_FIRST+20+next);if(target)SetFocus(target->hwnd);}return 0;}if(message==WM_NCDESTROY)RemovePropW(hwnd,L"Orion.Settings.RegionProc");return CallWindowProcW(original,hwnd,message,wp,lp);}
static LRESULT CALLBACK ns_region_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){switch(message){case WM_MOUSEWHEEL:ns_region_set_scroll(hwnd,g_ns_region_scroll-GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA*(NS_REGION_ROW*2));return 0;
 case WM_LBUTTONDOWN:{RECT track,thumb;ns_region_geometry(hwnd,&track,&thumb);int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp),range=ns_region_range(hwnd);if(range&&x>=track.left-ui_scale(hwnd,7)){SetFocus(hwnd);if(y<thumb.top||y>thumb.bottom){int travel=max(1,track.bottom-track.top-(thumb.bottom-thumb.top));ns_region_set_scroll(hwnd,MulDiv(y-track.top-(thumb.bottom-thumb.top)/2,range,travel));}g_ns_region_drag=1;g_ns_region_drag_y=y;g_ns_region_drag_scroll=g_ns_region_scroll;SetCapture(hwnd);}return 0;}
 case WM_MOUSEMOVE:if(g_ns_region_drag&&GetCapture()==hwnd){RECT track,thumb;ns_region_geometry(hwnd,&track,&thumb);int travel=max(1,track.bottom-track.top-(thumb.bottom-thumb.top));ns_region_set_scroll(hwnd,g_ns_region_drag_scroll+MulDiv(GET_Y_LPARAM(lp)-g_ns_region_drag_y,ns_region_range(hwnd),travel));}return 0;
 case WM_LBUTTONUP:if(GetCapture()==hwnd)ReleaseCapture();g_ns_region_drag=0;return 0;case WM_CAPTURECHANGED:g_ns_region_drag=0;return 0;
 case WM_KEYDOWN:if(wp==VK_HOME)ns_region_set_scroll(hwnd,0);else if(wp==VK_END)ns_region_set_scroll(hwnd,ns_region_range(hwnd));else if(wp==VK_UP||wp==VK_DOWN)ns_region_set_scroll(hwnd,g_ns_region_scroll+(wp==VK_DOWN?NS_REGION_ROW:-NS_REGION_ROW));else if(wp==VK_PRIOR||wp==VK_NEXT)ns_region_set_scroll(hwnd,g_ns_region_scroll+(wp==VK_NEXT?130:-130));return 0;
 case WM_GETDLGCODE:return DLGC_WANTARROWS;case WM_DRAWITEM:return ns_draw((DRAWITEMSTRUCT*)lp);case WM_COMMAND:return SendMessageW(g_ns_hwnd,message,wp,lp);
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);RECT r;GetClientRect(hwnd,&r);ui_fill(dc,r,ui_surface());if(ns_region_range(hwnd)>0){RECT track,thumb;ns_region_geometry(hwnd,&track,&thumb);ui_round(dc,track,ui_outline(),ui_scale(hwnd,2));ui_round(dc,thumb,g_ns_region_drag?ui_accent():ui_muted(),ui_scale(hwnd,2));}EndPaint(hwnd,&ps);return 0;}
 case WM_ERASEBKGND:return 1;}return DefWindowProcW(hwnd,message,wp,lp);}
static void ns_slider_set(HWND hwnd,int channel,int x){RECT r;GetClientRect(hwnd,&r);int left=ui_scale(hwnd,19),right=r.right-ui_scale(hwnd,35);int value=right>left?clampi(MulDiv(x-left,255,right-left),0,255):0;COLORREF c=g_ns_colors[g_ns_color_role];int rgb[]={GetRValue(c),GetGValue(c),GetBValue(c)};if(rgb[channel]==value)return;rgb[channel]=value;g_ns_colors[g_ns_color_role]=RGB(rgb[0],rgb[1],rgb[2]);g_ns_color_dirty=1;ns_color_text();SetTimer(g_ns_hwnd,NS_COLOR_TIMER,100,NULL);}
static LRESULT CALLBACK ns_slider_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){int channel=(int)(INT_PTR)GetWindowLongPtrW(hwnd,GWLP_USERDATA);switch(message){case WM_CREATE:SetWindowLongPtrW(hwnd,GWLP_USERDATA,(LONG_PTR)((CREATESTRUCTW*)lp)->lpCreateParams);return 0;case WM_LBUTTONDOWN:SetFocus(hwnd);SetCapture(hwnd);ns_slider_set(hwnd,channel,GET_X_LPARAM(lp));return 0;case WM_MOUSEMOVE:if(GetCapture()==hwnd)ns_slider_set(hwnd,channel,GET_X_LPARAM(lp));return 0;case WM_LBUTTONUP:if(GetCapture()==hwnd)ReleaseCapture();ns_flush_color();return 0;case WM_CAPTURECHANGED:ns_flush_color();return 0;case WM_KEYDOWN:if(wp==VK_LEFT||wp==VK_RIGHT){COLORREF c=g_ns_colors[g_ns_color_role];int rgb[]={GetRValue(c),GetGValue(c),GetBValue(c)};rgb[channel]=clampi(rgb[channel]+(wp==VK_RIGHT?1:-1),0,255);g_ns_colors[g_ns_color_role]=RGB(rgb[0],rgb[1],rgb[2]);g_ns_color_dirty=1;ns_color_text();ns_flush_color();return 0;}break;case WM_GETDLGCODE:return DLGC_WANTARROWS;case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);RECT r;GetClientRect(hwnd,&r);HBRUSH b=CreateSolidBrush(ui_surface());FillRect(dc,&r,b);DeleteObject(b);COLORREF c=g_ns_colors[g_ns_color_role];int values[]={GetRValue(c),GetGValue(c),GetBValue(c)},value=values[channel],left=ui_scale(hwnd,19),right=r.right-ui_scale(hwnd,35),cy=r.bottom/2;RECT lane={left,cy-ui_scale(hwnd,2),right,cy+ui_scale(hwnd,2)};ui_round(dc,lane,ui_outline(),ui_scale(hwnd,2));int thumb=left+MulDiv(value,max(1,right-left),255);lane.right=thumb;ui_round(dc,lane,ui_accent(),ui_scale(hwnd,2));RECT knob={thumb-ui_scale(hwnd,5),cy-ui_scale(hwnd,5),thumb+ui_scale(hwnd,5),cy+ui_scale(hwnd,5)};ui_round(dc,knob,ui_accent(),ui_scale(hwnd,5));wchar_t letter[2]={(wchar_t)(channel==0?'R':channel==1?'G':'B'),0},number[8];swprintf(number,8,L"%d",value);RECT label=r;label.right=left-ui_scale(hwnd,3);ui_text_draw(dc,label,letter,g_ns_font,ui_text(),DT_VCENTER|DT_SINGLELINE);label.left=right+ui_scale(hwnd,7);label.right=r.right;ui_text_draw(dc,label,number,g_ns_font,ui_muted(),DT_RIGHT|DT_VCENTER|DT_SINGLELINE);EndPaint(hwnd,&ps);return 0;}case WM_ERASEBKGND:return 1;}return DefWindowProcW(hwnd,message,wp,lp);}
static int CALLBACK ns_font_enum(const LOGFONTW *font,const TEXTMETRICW *metric,DWORD type,LPARAM parameter){(void)metric;(void)type;(void)parameter;if(font->lfFaceName[0]!='@'&&SendMessageW(g_ns_font_box,CB_GETCOUNT,0,0)<512&&SendMessageW(g_ns_font_box,CB_FINDSTRINGEXACT,(WPARAM)-1,(LPARAM)font->lfFaceName)==CB_ERR)SendMessageW(g_ns_font_box,CB_ADDSTRING,0,(LPARAM)font->lfFaceName);return 1;}
static void ns_save_hotkey(UINT modifiers,UINT key){
    char raw[32];snprintf(raw,sizeof(raw),"%u",modifiers);
    if(!ns_save("HotkeyModifiers",raw))return;
    snprintf(raw,sizeof(raw),"%u",key);if(!ns_save("HotkeyKey",raw))return;
    wchar_t text[64],label[160]=L"";
    UINT scan=MapVirtualKeyW(key,MAPVK_VK_TO_VSC_EX);
    LONG flags=(LONG)((scan&255)<<16)|((scan&0xFF00)?(1<<24):0);
    if(!GetKeyNameTextW(flags,text,64))swprintf(text,64,L"%lc",(wchar_t)key);
    if(modifiers&2)wcscat(label,L"Ctrl + ");
    if(modifiers&1)wcscat(label,L"Alt + ");
    if(modifiers&4)wcscat(label,L"Shift + ");
    if(modifiers&8)wcscat(label,L"Win + ");
    wcsncat(label,text,60);SetWindowTextW(g_ns_hotkey,label);
}
static LRESULT CALLBACK ns_edit_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){WNDPROC original=(WNDPROC)GetPropW(hwnd,L"Orion.Settings.EditProc");NSField *field=(NSField*)GetPropW(hwnd,L"Orion.Settings.Field");if(message==WM_GETDLGCODE)return CallWindowProcW(original,hwnd,message,wp,lp)|DLGC_WANTALLKEYS;
 if(message==WM_KEYDOWN&&field){if(wp==VK_TAB){SetFocus(GetNextDlgTabItem(g_ns_hwnd,hwnd,GetKeyState(VK_SHIFT)<0));return 0;}if(wp==VK_ESCAPE){if(field->kind==NS_HOTKEY)return 0;if(field->kind==NS_COLOR_EDIT)ns_color_text();else{g_ns_updating=1;wchar_t text[32];swprintf(text,32,L"%d",ns_number(field->key,field->fallback));SetWindowTextW(hwnd,text);g_ns_updating=0;}SendMessageW(hwnd,EM_SETSEL,0,-1);return 0;}if(field->kind==NS_HOTKEY){if(wp==VK_CONTROL||wp==VK_MENU||wp==VK_SHIFT||wp==VK_LWIN||wp==VK_RWIN)return 0;UINT mods=(GetKeyState(VK_MENU)<0?1:0)|(GetKeyState(VK_CONTROL)<0?2:0)|(GetKeyState(VK_SHIFT)<0?4:0)|((GetKeyState(VK_LWIN)<0||GetKeyState(VK_RWIN)<0)?8:0);if(!mods){ns_status("Inclua Ctrl, Alt, Shift ou Windows na combinação.");return 0;}ns_save_hotkey(mods,(UINT)wp);return 0;}if(wp==VK_RETURN){if(field->kind==NS_NUMBER)ns_save_number(field);else if(field->kind==NS_COLOR_EDIT)ns_edit_color(1);return 0;}if(field->kind==NS_NUMBER&&(wp==VK_UP||wp==VK_DOWN)){wchar_t text[32];GetWindowTextW(hwnd,text,32);int value=clampi(_wtoi(text)+(wp==VK_UP?1:-1),field->min,field->max);swprintf(text,32,L"%d",value);SetWindowTextW(hwnd,text);ns_save_number(field);return 0;}}
 if(message==WM_CHAR&&(wp==VK_RETURN||wp==VK_ESCAPE||(field&&field->kind==NS_HOTKEY)))return 0;if(message==WM_SETFOCUS||message==WM_KILLFOCUS){RECT r;GetWindowRect(hwnd,&r);MapWindowPoints(NULL,g_ns_hwnd,(POINT*)&r,2);if(field&&field->kind==NS_NUMBER){int i=(int)(field-g_ns_fields);if(i>=0&&i<g_ns_count&&g_ns_spinners[i])InvalidateRect(g_ns_spinners[i],NULL,FALSE);}
 else {InflateRect(&r,ui_scale(hwnd,8),ui_scale(hwnd,4));InvalidateRect(g_ns_hwnd,&r,FALSE);}}if(message==WM_NCDESTROY){RemovePropW(hwnd,L"Orion.Settings.EditProc");RemovePropW(hwnd,L"Orion.Settings.Field");}return CallWindowProcW(original,hwnd,message,wp,lp);}
static void ns_combo_paint(HWND hwnd,HDC dc){RECT r;GetClientRect(hwnd,&r);ui_fill(dc,r,ui_surface());ui_round_border(dc,r,ui_background(),GetFocus()==hwnd?ui_focus_outline():ui_outline(),ui_scale(hwnd,5));wchar_t text[320]=L"";int selected=(int)SendMessageW(hwnd,CB_GETCURSEL,0,0);if(selected>=0)SendMessageW(hwnd,CB_GETLBTEXT,selected,(LPARAM)text);RECT label=r;label.left+=ui_scale(hwnd,9);label.right-=ui_scale(hwnd,30);ui_text_draw(dc,label,text,g_ns_font,ui_text(),DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);int cx=r.right-ui_scale(hwnd,15),cy=(r.bottom-r.top)/2;HPEN pen=CreatePen(PS_SOLID,max(1,ui_scale(hwnd,1)),ui_muted());HGDIOBJ old=SelectObject(dc,pen);MoveToEx(dc,cx-ui_scale(hwnd,4),cy-ui_scale(hwnd,2),NULL);LineTo(dc,cx,cy+ui_scale(hwnd,2));LineTo(dc,cx+ui_scale(hwnd,4),cy-ui_scale(hwnd,2));SelectObject(dc,old);DeleteObject(pen);}
static LRESULT CALLBACK ns_combo_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){WNDPROC original=(WNDPROC)GetPropW(hwnd,L"Orion.Settings.ComboProc");if(message==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);ns_combo_paint(hwnd,dc);EndPaint(hwnd,&ps);return 0;}if(message==WM_PRINTCLIENT){ns_combo_paint(hwnd,(HDC)wp);return 0;}if(message==WM_ERASEBKGND)return 1;if(message==WM_NCDESTROY)RemovePropW(hwnd,L"Orion.Settings.ComboProc");LRESULT result=CallWindowProcW(original,hwnd,message,wp,lp);if(message==WM_SETFOCUS||message==WM_KILLFOCUS||message==CB_SETCURSEL||message==WM_ENABLE)InvalidateRect(hwnd,NULL,FALSE);return result;}
static void ns_subclass_edits(void){for(int i=0;i<g_ns_count;++i){NSField *f=&g_ns_fields[i];if(f->kind==NS_NUMBER||f->kind==NS_COLOR_EDIT||f->kind==NS_HOTKEY){WNDPROC old=(WNDPROC)SetWindowLongPtrW(f->hwnd,GWLP_WNDPROC,(LONG_PTR)ns_edit_proc);SetPropW(f->hwnd,L"Orion.Settings.EditProc",(HANDLE)old);SetPropW(f->hwnd,L"Orion.Settings.Field",f);}else if(f->kind==NS_COMBO){WNDPROC old=(WNDPROC)SetWindowLongPtrW(f->hwnd,GWLP_WNDPROC,(LONG_PTR)ns_combo_proc);SetPropW(f->hwnd,L"Orion.Settings.ComboProc",(HANDLE)old);}else if(f->kind==NS_CHECK||f->kind==NS_COLOR){ui_prepare_button(f->hwnd);if(f->region>=0){WNDPROC old=(WNDPROC)SetWindowLongPtrW(f->hwnd,GWLP_WNDPROC,(LONG_PTR)ns_region_button_proc);SetPropW(f->hwnd,L"Orion.Settings.RegionProc",(HANDLE)old);}}}}
static int ns_doc_set(char **document,const char *key,const char *raw){char *next=json_replace(*document,key,raw);free(*document);*document=next;return next!=NULL;}
static void ns_defaults(void){char *doc=_strdup(ns_document()?ns_document():"{}");if(!doc)return;const struct{const char *key,*raw;} defaults[]={ {"TrayFontFamily","\"Segoe UI\""},{"TrayFontSize","8"},{"TrayFontBold","true"},{"DarkTheme","true"},{"WindowBlur","false"},{"TrayTransparent","false"},{"TrayBorder","false"},{"TrayRounded","true"},{"TrayColorBackground","false"},{"TrayAntialias","true"},{"TrayTextColor","\"#6FB5FF\""},{"TrayBackgroundColor","\"#0F1C32\""},{"TrayWarningColor","\"#F9C256\""},{"TrayDangerColor","\"#FF7F7B\""},{"WarningPercent","75"},{"DangerPercent","90"},{"TraySingleClick","0"},{"TrayDoubleClick","1"},{"StartWithWindows","true"},{"StartMinimized","false"},{"AlwaysOnTop","false"},{"ConfirmEveryClean","false"},{"CloseToTray","true"},{"EnableHotkey","true"},{"ShowNotifications","true"},{"NotificationSound","true"},{"SkipUac","true"},{"DebugLog","false"},{"ShowGraph","false"},{"ConfirmAggressiveClean","true"},{"RefreshSeconds","1"},{"HotkeyModifiers","3"},{"HotkeyKey","77"},{"Language","\"pt-BR\""},{"Profile","null"},{"AutoThresholdEnabled","false"},{"AutoThresholdPercent","88"},{"AutoThreshold","88"},{"AutoIntervalEnabled","false"},{"AutoIntervalMinutes","30"},{"MinimumCooldownMinutes","15"},{"AllowAggressiveAuto","false"}};for(unsigned i=0;i<sizeof(defaults)/sizeof(*defaults);++i)if(!ns_doc_set(&doc,defaults[i].key,defaults[i].raw))return;for(int i=0;i<9;++i)if(!ns_doc_set(&doc,region_keys[i],default_regions[i]?"true":"false"))return;if(doc){free(g_ns_draft);g_ns_draft=doc;ns_preview_refresh();g_ns_color_dirty=0;ns_refresh_fields();ns_status("Padrões pendentes");}}

static int ns_commit(void){
    if(!g_ns_draft)return 0;
    ns_flush_color();
    HWND focus=GetFocus();
    for(int i=0;i<g_ns_count;++i)if(g_ns_fields[i].hwnd==focus&&g_ns_fields[i].kind==NS_NUMBER)ns_save_number(&g_ns_fields[i]);
    if(!ns_is_dirty()){ns_status("Tudo salvo");return 1;}
    load_settings(0);
    if(!g_ns_baseline||!g_document||strcmp(g_ns_baseline,g_document)!=0){
        ns_status("Reabra as opções");return 0;
    }
    char *temporary=g_ns_draft;g_ns_draft=NULL;
    int original_start=ns_bool("StartWithWindows",1);
    g_ns_draft=temporary;
    if(g_ns_preview.skip_uac!=g_settings.skip_uac&&!native_configure_authorization(g_ns_hwnd,g_ns_preview.skip_uac))return 0;
    if(!ns_startup(ns_bool("StartWithWindows",1))){ns_status("Falha na inicialização");return 0;}
    if(!atomic_settings_write(g_ns_draft)){
        temporary=g_ns_draft;g_ns_draft=NULL;ns_startup(original_start);g_ns_draft=temporary;
        ns_status("Erro ao salvar");return 0;
    }
    native_preferences_changed();
    char *baseline=_strdup(g_document?g_document:"{}");
    char *draft=baseline?_strdup(baseline):NULL;
    if(baseline&&draft){free(g_ns_baseline);free(g_ns_draft);g_ns_baseline=baseline;g_ns_draft=draft;ns_preview_refresh();}
    else{free(baseline);free(draft);}
    ns_status("Alterações aplicadas");ns_refresh_fields();return 1;
}
static LRESULT CALLBACK native_settings_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){switch(message){case WM_CREATE:{g_ns_hwnd=hwnd;g_ns_baseline=_strdup(g_document?g_document:"{}");g_ns_draft=g_ns_baseline?_strdup(g_ns_baseline):NULL;g_ns_preview=g_settings;ns_preview_refresh();memset(g_ns_spinners,0,sizeof(g_ns_spinners));memset(g_ns_spinner_hover,0,sizeof(g_ns_spinner_hover));g_ns_region_scroll=0;g_ns_color_dirty=0;g_ns_fonts_loaded=0;g_ns_theme=g_ns_window_blur=-1;g_ns_labels_language[0]=0;g_ns_authorized=native_authorization_ready();ns_fonts();ns_load_languages();g_ns_region=CreateWindowExW(WS_EX_CONTROLPARENT,NS_REGION_CLASS,L"",WS_CHILD|WS_TABSTOP|WS_CLIPCHILDREN,0,0,1,1,hwnd,NULL,g_instance,NULL);ns_create_fields();ns_create_spinners();ns_subclass_edits();for(int i=0;i<5;++i){wchar_t text[128];ns_nav_caption(i,text,128);HWND b=ns_button(hwnd,NS_NAV+i,text,14,18+i*37,162,33);ui_prepare_button(b);}ui_prepare_button(ns_button(hwnd,NS_CLOSE,L"",0,0,84,29));ui_prepare_button(ns_button(hwnd,NS_APPLY,L"",0,0,88,30));ui_prepare_button(ns_button(hwnd,NS_DEFAULTS,L"",0,0,104,30));ui_prepare_button(ns_button(hwnd,NS_OPEN_LOG,L"Abrir pasta de logs",0,0,160,27));
 for(int i=0;i<3;++i){const wchar_t *name=i==0?L"Leve":i==1?L"Equilibrado":L"Personalizado";ui_prepare_button(ns_button(hwnd,NS_PROFILE_LEVE+i,name,0,0,105,27));}
 ui_prepare_button(ns_button(hwnd,NS_TEST_NOTIFICATION,L"Testar notificação",0,0,184,27));
 ui_prepare_button(ns_button(hwnd,NS_COPY_HEX,L"Copiar",0,0,64,27));ui_prepare_button(g_ns_authorize);for(int i=0;i<8;++i)ui_prepare_button(GetDlgItem(hwnd,NS_PALETTE+i));for(int i=0;i<3;++i)g_ns_sliders[i]=CreateWindowExW(0,NS_SLIDER_CLASS,L"",WS_CHILD|WS_TABSTOP,0,0,1,1,hwnd,(HMENU)(INT_PTR)(NS_SLIDER+i),g_instance,(LPVOID)(INT_PTR)i);ns_status("Nenhuma alteração pendente");ns_refresh_fields();ns_layout();return 0;}
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);ui_paint_buffered(hwnd,dc,&ps.rcPaint,ns_paint);EndPaint(hwnd,&ps);return 0;}case WM_ERASEBKGND:return 1;case WM_SIZE:ns_layout();return 0;
 case WM_GETMINMAXINFO:{MINMAXINFO *limits=(MINMAXINFO*)lp;RECT size={0,0,ui_scale(hwnd,850),ui_scale(hwnd,580)};AdjustWindowRectExForDpi(&size,(DWORD)GetWindowLongPtrW(hwnd,GWL_STYLE),FALSE,(DWORD)GetWindowLongPtrW(hwnd,GWL_EXSTYLE),GetDpiForWindow(hwnd));limits->ptMinTrackSize.x=size.right-size.left;limits->ptMinTrackSize.y=size.bottom-size.top;return 0;}
 case WM_MOUSEWHEEL:{
    int delta=GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA;
    int maximum=ns_scroll_max();
    if(maximum>0){g_ns_page_scroll[g_ns_page]=clampi(ns_scroll()-delta*55,0,maximum);ns_layout();}
    return 0;
 }
 case WM_DPICHANGED:{RECT *suggested=(RECT*)lp;SetWindowPos(hwnd,NULL,suggested->left,suggested->top,suggested->right-suggested->left,suggested->bottom-suggested->top,SWP_NOZORDER|SWP_NOACTIVATE);g_ns_dpi=0;ns_refresh_fields();ns_layout();return 0;}
 case NS_REFRESH:ns_refresh_fields();return 0;
 case WM_TIMER:if(wp==NS_COLOR_TIMER)ns_flush_color();return 0;
 case WM_MEASUREITEM:{MEASUREITEMSTRUCT *item=(MEASUREITEMSTRUCT*)lp;if(item->CtlType==ODT_COMBOBOX){item->itemHeight=(UINT)ui_scale(hwnd,26);return TRUE;}break;}
 case WM_DRAWITEM:return ns_draw((DRAWITEMSTRUCT*)lp);
 case WM_CTLCOLOREDIT:case WM_CTLCOLORSTATIC:case WM_CTLCOLORLISTBOX:SetTextColor((HDC)wp,ui_text());SetBkColor((HDC)wp,ui_surface());return (LRESULT)g_ns_edit_brush;
 case WM_COMMAND:{int id=LOWORD(wp),notification=HIWORD(wp);if(g_ns_updating)return 0;if(id>=NS_NAV&&id<NS_NAV+5){ns_flush_color();g_ns_page=id-NS_NAV;ns_layout();return 0;}if(id==NS_CLOSE){SendMessageW(hwnd,WM_CLOSE,0,0);return 0;}if(id==NS_APPLY){ns_commit();return 0;}if(id==NS_OPEN_LOG){SHCreateDirectoryExW(hwnd,g_data_dir,NULL);ShellExecuteW(hwnd,L"open",g_data_dir,NULL,NULL,SW_SHOWNORMAL);return 0;}if(id==NS_DEFAULTS){ns_defaults();return 0;}
 if(id>=NS_PROFILE_LEVE&&id<=NS_PROFILE_PERSONALIZADO){
     int profile=id-NS_PROFILE_LEVE;
     if(profile==2){ns_status("Personalizado: escolha as operações manualmente.");return 0;}
     DWORD mask=ns_profile_mask(profile);char *next=_strdup(ns_document()?ns_document():"{}");
     if(!next){ns_status("Não foi possível configurar o perfil.");return 0;}
     int valid=1;
     for(int index=0;index<9&&valid;++index){char *updated=json_replace(next,region_keys[index],mask&(1u<<index)?"true":"false");free(next);next=updated;valid=updated!=NULL;}
     if(valid){char *updated=json_replace(next,"Profile","null");free(next);next=updated;valid=updated!=NULL;}
     if(valid){free(g_ns_draft);g_ns_draft=next;ns_preview_refresh();ns_status("Perfil selecionado; clique em Aplicar.");ns_refresh_fields();InvalidateRect(g_ns_region,NULL,FALSE);}
     else{free(next);ns_status("Não foi possível configurar o perfil.");}
     return 0;
 }
 if(id==NS_TEST_NOTIFICATION){
     NOTIFYICONDATAW data=tray_data();data.uFlags=NIF_INFO;data.dwInfoFlags=NIIF_INFO|NIIF_NOSOUND;
     copyw(data.szInfoTitle,64,L"Orion Memory · teste");
     copyw(data.szInfo,256,L"Notificação de teste. Nenhuma limpeza foi realizada.");
     if(g_ns_preview.sound)data.dwInfoFlags=NIIF_INFO;
     Shell_NotifyIconW(NIM_MODIFY,&data);return 0;
 }
 if(id==NS_COPY_HEX){wchar_t color[20];GetWindowTextW(g_ns_hex,color,20);if(OpenClipboard(hwnd)){
     EmptyClipboard();size_t bytes=(wcslen(color)+1)*sizeof(wchar_t);HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,bytes);
     if(memory){void *data=GlobalLock(memory);if(data){memcpy(data,color,bytes);GlobalUnlock(memory);if(!SetClipboardData(CF_UNICODETEXT,memory))GlobalFree(memory);}else GlobalFree(memory);}
     CloseClipboard();ns_status("Cor hexadecimal copiada.");}return 0;
 }
 if(id==NS_AUTHORIZE){if(native_configure_authorization(hwnd,1)){g_ns_authorized=native_authorization_ready();ns_save("SkipUac","true");ns_refresh_fields();}return 0;}
 if(id>=NS_COLOR_ROLE&&id<NS_COLOR_ROLE+4){ns_flush_color();g_ns_color_role=id-NS_COLOR_ROLE;ns_color_text();for(int i=0;i<4;++i)InvalidateRect(GetDlgItem(hwnd,NS_COLOR_ROLE+i),NULL,FALSE);return 0;}
 if(id>=NS_PALETTE&&id<NS_PALETTE+8){g_ns_colors[g_ns_color_role]=ns_palette[id-NS_PALETTE];g_ns_color_dirty=1;ns_color_text();ns_flush_color();return 0;}
 if(id>=NS_NUMBER_UP&&id<NS_NUMBER_UP+g_ns_count){NSField *f=&g_ns_fields[id-NS_NUMBER_UP];if(f->kind==NS_NUMBER){wchar_t text[32];GetWindowTextW(f->hwnd,text,32);swprintf(text,32,L"%d",clampi(_wtoi(text)+1,f->min,f->max));SetWindowTextW(f->hwnd,text);ns_save_number(f);}return 0;}
 if(id>=NS_NUMBER_DOWN&&id<NS_NUMBER_DOWN+g_ns_count){NSField *f=&g_ns_fields[id-NS_NUMBER_DOWN];if(f->kind==NS_NUMBER){wchar_t text[32];GetWindowTextW(f->hwnd,text,32);swprintf(text,32,L"%d",clampi(_wtoi(text)-1,f->min,f->max));SetWindowTextW(f->hwnd,text);ns_save_number(f);}return 0;}
 NSField *f=ns_field(id);if(!f)return 0;
 if(f->kind==NS_CHECK&&notification==BN_CLICKED){if(f->region>=0){
     if(!f->checked&&ns_region_risk[f->region]==2){
         wchar_t warning[512];swprintf(warning,512,L"Essa operação pode provocar lentidão intensa, picos de disco ou travamento aparente.\n\nSalve o trabalho antes de executá-la.\n\nDeseja incluir essa operação na limpeza manual?");
         if(MessageBoxW(hwnd,warning,L"Orion Memory — operação avançada",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES)return 0;
     }
     int count=0;for(int i=0;i<9;++i)count+=g_ns_preview.regions[i];if(f->checked&&count<=1){ns_status("Selecione uma região");return 0;}if(ns_save(f->key,f->checked?"false":"true"))ns_save("Profile","null");ns_refresh_fields();return 0;}int value=!f->checked;ns_save(f->key,value?"true":"false");ns_refresh_fields();return 0;}
 if(f->kind==NS_NUMBER&&notification==EN_KILLFOCUS){ns_save_number(f);ns_refresh_fields();return 0;}
 if(f->kind==NS_COLOR_EDIT&&notification==EN_CHANGE){ns_edit_color(0);return 0;}if(f->kind==NS_COLOR_EDIT&&notification==EN_KILLFOCUS){ns_edit_color(1);return 0;}
 if(f->kind==NS_COMBO&&notification==CBN_DROPDOWN&&f->hwnd==g_ns_font_box&&!g_ns_fonts_loaded){g_ns_fonts_loaded=1;LOGFONTW font;memset(&font,0,sizeof(font));font.lfCharSet=DEFAULT_CHARSET;HDC dc=GetDC(hwnd);EnumFontFamiliesExW(dc,&font,ns_font_enum,0,0);ReleaseDC(hwnd,dc);return 0;}
 if(f->kind==NS_COMBO&&notification==CBN_SELCHANGE){int selected=(int)SendMessageW(f->hwnd,CB_GETCURSEL,0,0);if(selected<0)return 0;char raw[1024];if(f->hwnd==g_ns_language_box){if(selected<g_ns_language_count&&ns_quote(g_ns_languages[selected].code,raw,sizeof(raw)))ns_save("Language",raw);}else if(f->hwnd==g_ns_font_box){wchar_t face[LF_FACESIZE];SendMessageW(f->hwnd,CB_GETLBTEXT,selected,(LPARAM)face);if(ns_quote(face,raw,sizeof(raw)))ns_save("TrayFontFamily",raw);}else{snprintf(raw,sizeof(raw),"%d",selected);ns_save(f->key,raw);}ns_refresh_fields();return 0;}break;}
 case WM_CLOSE:DestroyWindow(hwnd);return 0;
 case WM_DESTROY:KillTimer(hwnd,NS_COLOR_TIMER);free(g_ns_draft);free(g_ns_baseline);g_ns_draft=g_ns_baseline=NULL;ui_release_text_fonts();if(g_ns_font)DeleteObject(g_ns_font);if(g_ns_bold)DeleteObject(g_ns_bold);if(g_ns_title)DeleteObject(g_ns_title);if(g_ns_edit_brush)DeleteObject(g_ns_edit_brush);if(g_ns_spinner_brush)DeleteObject(g_ns_spinner_brush);g_ns_font=g_ns_bold=g_ns_title=NULL;g_ns_edit_brush=g_ns_spinner_brush=NULL;g_ns_hwnd=NULL;g_ns_region=NULL;g_ns_count=0;return 0;
 }return DefWindowProcW(hwnd,message,wp,lp);}
static void native_settings_show(HWND owner){if(g_ns_hwnd){ShowWindow(g_ns_hwnd,SW_SHOWNORMAL);SetForegroundWindow(g_ns_hwnd);return;}WNDCLASSEXW cls;memset(&cls,0,sizeof(cls));cls.cbSize=sizeof(cls);cls.hInstance=g_instance;cls.hCursor=LoadCursor(NULL,IDC_ARROW);cls.lpfnWndProc=ns_region_proc;cls.lpszClassName=NS_REGION_CLASS;RegisterClassExW(&cls);cls.lpfnWndProc=ns_slider_proc;cls.lpszClassName=NS_SLIDER_CLASS;RegisterClassExW(&cls);cls.lpfnWndProc=ns_spinner_proc;cls.lpszClassName=NS_SPINNER_CLASS;RegisterClassExW(&cls);cls.lpfnWndProc=native_settings_proc;cls.lpszClassName=NS_CLASS;cls.hIcon=LoadIconW(g_instance,MAKEINTRESOURCEW(1));RegisterClassExW(&cls);UINT dpi=owner?GetDpiForWindow(owner):96;if(!dpi)dpi=96;RECT bounds={0,0,MulDiv(970,(int)dpi,96),MulDiv(760,(int)dpi,96)};DWORD style=(WS_OVERLAPPEDWINDOW&~WS_MAXIMIZEBOX)|WS_CLIPCHILDREN;AdjustWindowRectExForDpi(&bounds,style,FALSE,0,dpi);HWND window=CreateWindowExW(0,NS_CLASS,L"Configurações — Orion Memory",style,CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,owner,NULL,g_instance,NULL);if(window){ui_center(window,owner);ui_apply_window(window);ShowWindow(window,SW_SHOW);RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);}}
static void native_settings_refresh(void){if(g_ns_hwnd)PostMessageW(g_ns_hwnd,NS_REFRESH,0,0);}
static void native_settings_close(void){if(g_ns_hwnd)SendMessageW(g_ns_hwnd,WM_CLOSE,0,0);}
static int native_settings_message(MSG *message){return g_ns_hwnd&&(message->hwnd==g_ns_hwnd||IsChild(g_ns_hwnd,message->hwnd))&&IsDialogMessageW(g_ns_hwnd,message);}
#endif



