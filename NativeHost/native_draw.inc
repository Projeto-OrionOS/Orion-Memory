/* Shared light GDI primitives. Returned fonts belong to the caller. */
static int ui_scale(HWND window,int value){UINT dpi=window?GetDpiForWindow(window):96;return MulDiv(value,(int)(dpi?dpi:96),96);}
static HFONT ui_font(HWND window,int pixels,int weight){return CreateFontW(-ui_scale(window,pixels),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");}
static COLORREF ui_background(void){return g_settings.dark?RGB(32,32,32):RGB(243,243,243);}
static COLORREF ui_surface(void){return g_settings.dark?RGB(40,41,44):RGB(255,255,255);}
static COLORREF ui_text(void){return g_settings.dark?RGB(245,245,245):RGB(26,26,26);}
static COLORREF ui_muted(void){return g_settings.dark?RGB(173,183,196):RGB(85,85,85);}
static COLORREF ui_accent(void){return g_settings.dark?RGB(81,187,255):RGB(0,103,192);}
static COLORREF ui_outline(void){return g_settings.dark?RGB(61,65,72):RGB(219,219,219);}
static COLORREF ui_selection_bg(void){return g_settings.dark?RGB(29,60,91):RGB(224,237,250);}
static COLORREF ui_selection_fg(void){return ui_text();}
static COLORREF ui_focus_outline(void){return g_settings.dark?RGB(94,129,155):RGB(85,135,180);}
static void ui_fill(HDC dc,RECT rectangle,COLORREF color){HBRUSH brush=CreateSolidBrush(color);FillRect(dc,&rectangle,brush);DeleteObject(brush);}
static void ui_round(HDC dc,RECT rectangle,COLORREF color,int radius){HBRUSH brush=CreateSolidBrush(color);HGDIOBJ old_brush=SelectObject(dc,brush),old_pen=SelectObject(dc,GetStockObject(NULL_PEN));RoundRect(dc,rectangle.left,rectangle.top,rectangle.right+1,rectangle.bottom+1,radius*2,radius*2);SelectObject(dc,old_pen);SelectObject(dc,old_brush);DeleteObject(brush);}
static void ui_round_border(HDC dc,RECT rectangle,COLORREF color,COLORREF outline,int radius){HBRUSH brush=CreateSolidBrush(color);HPEN pen=CreatePen(PS_SOLID,1,outline);HGDIOBJ old_brush=SelectObject(dc,brush),old_pen=SelectObject(dc,pen);RoundRect(dc,rectangle.left,rectangle.top,rectangle.right,rectangle.bottom,radius*2,radius*2);SelectObject(dc,old_pen);SelectObject(dc,old_brush);DeleteObject(pen);DeleteObject(brush);}
typedef struct {HFONT font;LONG height,weight;int script;} UIFontFallback;
static UIFontFallback g_ui_fallbacks[6];static unsigned g_ui_fallback_next;
static int ui_missing_glyphs(HDC dc,const wchar_t *text){WORD glyphs[384];size_t length=wcslen(text);if(length>384)length=384;if(GetGlyphIndicesW(dc,text,(int)length,glyphs,GGI_MARK_NONEXISTING_GLYPHS)==GDI_ERROR)return (int)length;int missing=0;for(size_t i=0;i<length;++i)if(glyphs[i]==0xFFFF)++missing;return missing;}
static HFONT ui_text_font(HDC dc,HFONT original,const wchar_t *text){int script=0;for(const wchar_t *p=text;*p;++p)if((*p>=0x2E80&&*p<=0xA4CF)||(*p>=0xF900&&*p<=0xFAFF)){script=1;break;}else if(*p>=0xAC00&&*p<=0xD7AF)script=2;if(!script)return original;HGDIOBJ old=SelectObject(dc,original);int missing=ui_missing_glyphs(dc,text);if(!missing){SelectObject(dc,old);return original;}LOGFONTW description;if(!GetObjectW(original,sizeof(description),&description)){SelectObject(dc,old);return original;}
 for(unsigned i=0;i<6;++i)if(g_ui_fallbacks[i].font&&g_ui_fallbacks[i].height==description.lfHeight&&g_ui_fallbacks[i].weight==description.lfWeight&&g_ui_fallbacks[i].script==script){SelectObject(dc,g_ui_fallbacks[i].font);if(ui_missing_glyphs(dc,text)<missing){SelectObject(dc,old);return g_ui_fallbacks[i].font;}}
 const wchar_t *families[]={L"Microsoft YaHei UI",L"MS UI Gothic",L"Yu Gothic UI",L"Malgun Gothic",L"Sans Serif Collection"};HFONT best=NULL;for(unsigned i=0;i<sizeof(families)/sizeof(*families);++i){copyw(description.lfFaceName,LF_FACESIZE,families[i]);description.lfCharSet=DEFAULT_CHARSET;HFONT candidate=CreateFontIndirectW(&description);if(!candidate)continue;SelectObject(dc,candidate);int count=ui_missing_glyphs(dc,text);SelectObject(dc,original);if(count<missing){if(best)DeleteObject(best);best=candidate;missing=count;if(!missing)break;}else DeleteObject(candidate);}
 SelectObject(dc,old);if(best){UIFontFallback *cache=&g_ui_fallbacks[g_ui_fallback_next++%6];if(cache->font)DeleteObject(cache->font);cache->font=best;cache->height=description.lfHeight;cache->weight=description.lfWeight;cache->script=script;return best;}return original;}
static void ui_release_text_fonts(void){for(unsigned i=0;i<6;++i){if(g_ui_fallbacks[i].font)DeleteObject(g_ui_fallbacks[i].font);memset(&g_ui_fallbacks[i],0,sizeof(g_ui_fallbacks[i]));}}
static void ui_text_draw(HDC dc,RECT rectangle,const wchar_t *text,HFONT font,COLORREF color,UINT flags){HFONT actual=ui_text_font(dc,font,text);HGDIOBJ old=SelectObject(dc,actual);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);DrawTextW(dc,text,-1,&rectangle,flags|DT_NOPREFIX);SelectObject(dc,old);}
/* Only the dirty rectangle exists as a temporary bitmap; no window bitmap is retained. */
static void ui_paint_buffered(HWND window,HDC target,const RECT *invalid,void (*paint)(HWND,HDC)){RECT client,dirty;GetClientRect(window,&client);if(!IntersectRect(&dirty,&client,invalid))return;int width=dirty.right-dirty.left,height=dirty.bottom-dirty.top;HDC memory=CreateCompatibleDC(target);HBITMAP bitmap=memory?CreateCompatibleBitmap(target,width,height):NULL;if(!memory||!bitmap){if(memory)DeleteDC(memory);paint(window,target);return;}HGDIOBJ old=SelectObject(memory,bitmap);SetViewportOrgEx(memory,-dirty.left,-dirty.top,NULL);IntersectClipRect(memory,dirty.left,dirty.top,dirty.right,dirty.bottom);paint(window,memory);BitBlt(target,dirty.left,dirty.top,width,height,memory,dirty.left,dirty.top,SRCCOPY);SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);}
/* The numeric RAM icon in the tray is deliberately independent of window transparency.
   Preserve an opaque shell popup for legible 16px menus. Acrylic backdrop is best-effort
   (Windows 11); the layered alpha provides actual, reversible transparency on older Windows. */
static void ui_apply_window(HWND window){
    if(!window)return;
    int dark=g_settings.dark?1:0,corner=2;
    wchar_t class_name[96]=L"";
    GetClassNameW(window,class_name,96);
    int popup=!wcscmp(class_name,L"OrionMemory.RoundedPopup");
    int translucent=g_settings.window_blur&&!popup;
    int backdrop=translucent?3:1; /* DWMSBT_TRANSIENTWINDOW / DWMSBT_NONE */
    DwmSetWindowAttribute(window,20,&dark,sizeof(dark));
    DwmSetWindowAttribute(window,33,&corner,sizeof(corner));
    DwmSetWindowAttribute(window,38,&backdrop,sizeof(backdrop));
    COLORREF background=ui_background(),outline=ui_outline(),text=ui_text();
    DwmSetWindowAttribute(window,35,&background,sizeof(background));
    DwmSetWindowAttribute(window,34,&outline,sizeof(outline));
    DwmSetWindowAttribute(window,36,&text,sizeof(text));
    LONG_PTR exstyle=GetWindowLongPtrW(window,GWL_EXSTYLE);
    if(translucent){
        if(!(exstyle&WS_EX_LAYERED))SetWindowLongPtrW(window,GWL_EXSTYLE,exstyle|WS_EX_LAYERED);
        SetLayeredWindowAttributes(window,0,235,LWA_ALPHA);
    }else if(exstyle&WS_EX_LAYERED&&!popup){
        SetLayeredWindowAttributes(window,0,255,LWA_ALPHA);
        SetWindowLongPtrW(window,GWL_EXSTYLE,exstyle&~(LONG_PTR)WS_EX_LAYERED);
    }
}
static LRESULT CALLBACK ui_button_proc(HWND window,UINT message,WPARAM wp,LPARAM lp){WNDPROC original=(WNDPROC)GetPropW(window,L"Orion.ButtonProc");if(message==WM_ERASEBKGND&&(GetWindowLongPtrW(window,GWL_STYLE)&BS_TYPEMASK)==BS_OWNERDRAW)return 1;if(message==WM_MOUSEMOVE&&!GetPropW(window,L"Orion.Hover")){SetPropW(window,L"Orion.Hover",(HANDLE)1);TRACKMOUSEEVENT tracking={sizeof(tracking),TME_LEAVE,window,0};TrackMouseEvent(&tracking);InvalidateRect(window,NULL,FALSE);}if(message==WM_MOUSELEAVE){RemovePropW(window,L"Orion.Hover");InvalidateRect(window,NULL,FALSE);}if(message==WM_NCDESTROY){RemovePropW(window,L"Orion.ButtonProc");RemovePropW(window,L"Orion.Hover");}return CallWindowProcW(original,window,message,wp,lp);}
static void ui_prepare_button(HWND button){if(!GetPropW(button,L"Orion.ButtonProc")){WNDPROC old=(WNDPROC)SetWindowLongPtrW(button,GWLP_WNDPROC,(LONG_PTR)ui_button_proc);SetPropW(button,L"Orion.ButtonProc",(HANDLE)old);} }
static void ui_draw_button_ex(DRAWITEMSTRUCT *item,int primary){RECT r=item->rcItem;ui_fill(item->hDC,r,ui_background());COLORREF base=primary?RGB(0,103,192):ui_surface(),foreground=primary?RGB(255,255,255):ui_text(),border=primary?base:ui_outline();int enabled=IsWindowEnabled(item->hwndItem),hot=GetPropW(item->hwndItem,L"Orion.Hover")!=NULL;if(!enabled){base=ui_surface();foreground=ui_muted();}else if(item->itemState&ODS_SELECTED){base=primary?RGB(0,76,145):(g_settings.dark?RGB(55,55,55):RGB(228,228,228));}else if(hot){base=primary?RGB(20,120,212):(g_settings.dark?RGB(53,53,53):RGB(244,244,244));}if(item->itemState&ODS_FOCUS)border=ui_focus_outline();ui_round_border(item->hDC,r,base,border,ui_scale(item->hwndItem,6));wchar_t text[256];GetWindowTextW(item->hwndItem,text,256);HFONT font=(HFONT)SendMessageW(item->hwndItem,WM_GETFONT,0,0);int owned=!font;if(owned)font=ui_font(item->hwndItem,12,primary?FW_SEMIBOLD:FW_NORMAL);ui_text_draw(item->hDC,r,text,font,foreground,DT_CENTER|DT_VCENTER|DT_SINGLELINE);if(owned)DeleteObject(font);}
static LRESULT ui_draw_button(DRAWITEMSTRUCT *item){ui_draw_button_ex(item,0);return TRUE;}
static void ui_center(HWND window,HWND owner){RECT rectangle,area;GetWindowRect(window,&rectangle);if(owner&&IsWindowVisible(owner))GetWindowRect(owner,&area);else{POINT mouse;GetCursorPos(&mouse);MONITORINFO info;memset(&info,0,sizeof(info));info.cbSize=sizeof(info);GetMonitorInfoW(MonitorFromPoint(mouse,MONITOR_DEFAULTTONEAREST),&info);area=info.rcWork;}SetWindowPos(window,NULL,area.left+(area.right-area.left-(rectangle.right-rectangle.left))/2,area.top+(area.bottom-area.top-(rectangle.bottom-rectangle.top))/2,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}