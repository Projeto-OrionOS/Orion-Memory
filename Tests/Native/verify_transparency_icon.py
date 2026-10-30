"""Checagens offline da transparência e do ícone do aplicativo nativo.
Execute a partir de qualquer diretório: python Tests/Native/verify_transparency_icon.py
Não substitui a compilação nem os testes visuais no Windows.
"""
from pathlib import Path
import json
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
host = (ROOT / 'NativeHost/host.c').read_text(encoding='utf-8')
draw = (ROOT / 'NativeHost/native_draw.inc').read_text(encoding='utf-8')
main = (ROOT / 'NativeHost/main_window.inc').read_text(encoding='utf-8')
settings = (ROOT / 'NativeHost/native_settings.inc').read_text(encoding='utf-8')
resource = (ROOT / 'NativeHost/host.rc').read_text(encoding='utf-8')
popup = (ROOT / 'NativeHost/native_popup.inc').read_text(encoding='utf-8')
ico_file = ROOT / 'Assets/OrionMemory.ico'
icon = ico_file.read_bytes()
reserved, kind, frames = struct.unpack_from('<HHH', icon, 0)
icon_sizes = {256 if icon[6 + i*16] == 0 else icon[6 + i*16] for i in range(frames)}
key = 'Usar transparência nas janelas'
locales = list((ROOT / 'Languages').glob('*.json'))

checks = {
    'recurso de icone aponta para a nova RAM': '1 ICON "../Assets/OrionMemory.ico"' in resource,
    'ICO Windows valido': reserved == 0 and kind == 1 and frames >= 8,
    'ICO contempla 16, 32, 48 e 256px': {16, 32, 48, 256}.issubset(icon_sizes),
    'percentual da bandeja continua dinamico': 'static HICON render_icon(int value,int size)' in host and 'update_memory(void)' in host,
    'janela e bandeja tem opcoes separadas': 'window_blur,transparent,' in host,
    'carregamento do valor persistente WindowBlur': 'json_bool(j,"WindowBlur",0)' in host,
    'opcao restaurada na pagina Principal': f'"WindowBlur","{key}"' in settings,
    'transparencia desligada por padrao': 'int dark,window_blur,transparent' in host and 'default_settings(Settings *s){memset(s,0,' in host,
    'efeito translucido reversivel': 'SetLayeredWindowAttributes(window,0,235,LWA_ALPHA)' in draw and 'SetWindowLongPtrW(window,GWL_EXSTYLE,exstyle&~' in draw,
    'blur DWM tratado como best-effort': 'int backdrop=translucent?3:1' in draw,
    'popups ficam opacos e legiveis': 'OrionMemory.RoundedPopup' in draw,
    'mudanca ao vivo na janela principal': 'g_main_visual_blur!=g_settings.window_blur' in main,
    'mudanca ao vivo na janela de configuracoes': 'transparency_changed=g_ns_window_blur!=g_settings.window_blur' in settings,
    'layout geral reorganizado e idioma preservado': 'for(int i=0;i<8;++i)' in settings and 'ns_card(dc,400,62,"Idioma"' in settings and 'g_ns_language_box' in settings,
    'continua com 30 idiomas': len(locales) == 30,
    'todas as traducoes tem a opcao': all(key in json.loads(path.read_text(encoding='utf-8')) for path in locales),
    'menu personalizado da bandeja foi preservado': 'popup_open_sub' in popup and 'tray_region_short_labels[9]' in popup,
    'modelo de limpeza manual nao alterado': 'a_real_operation(int index,AResult *r,void *unused)' in (ROOT/'NativeHost/native_actions.inc').read_text(encoding='utf-8'),
}
for name, ok in checks.items():
    print(('OK  ' if ok else 'ERRO'), name)
if not all(checks.values()):
    sys.exit(1)
print(f'OK: {len(checks)} verificacoes estaticas. Nao substituem a compilacao para Windows.')