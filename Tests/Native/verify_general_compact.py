"""Static coverage for the 740x530 compact Geral settings view."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[2]
s=(root/'NativeHost/native_settings.inc').read_text(encoding='utf-8')
d=(root/'NativeHost/native_draw.inc').read_text(encoding='utf-8')
checks={
 "default window 740x530": 'int cw=740,ch=530;' in s,
 "sensible resize limit": 'ui_scale(hwnd,700),ui_scale(hwnd,510)' in s,
 "rounded cards": 'ui_scale(g_ns_hwnd,12)' in s,
 "DWM corners on Win11": 'DwmSetWindowAttribute(window,33,&corner' in d,
 "all general cards compact": all(x in s for x in ['ns_card(dc,12,120,"Inicialização"','ns_card(dc,142,116,"Janela e aparência"','ns_card(dc,268,125,"Segurança"','ns_card(dc,399,65,"Idioma"']),
 "general requires no scroll at 740x530": '{459,646,641,540,537}' in s and (459-(530-63)+7)<=0,
 "language and authorization visible": '"Idioma",465,429,319,30' in s and 'ns_page_visible(359,29)' in s,
 "navigation resets scroll on switch": 'g_ns_page_scroll[g_ns_page]=0;' in s,
 "Apply/Cancel not removed": all(x in s for x in ['NS_APPLY 5093','ns_commit();return 0;','WM_CLOSE:DestroyWindow']),
 "all nine cleanup regions preserved": 'for(int i=0;i<9;++i)' in s,
 "no new UI toolkits": 'WebView2' not in s and 'Qt' not in s,
}
for name,good in checks.items():print(('OK' if good else 'FAIL'),name)
if not all(checks.values()): raise SystemExit(1)
print(f'{len(checks)}/{len(checks)} static checks passed')