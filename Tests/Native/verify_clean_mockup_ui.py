#!/usr/bin/env python3
"""Regressões estáticas para o redesign limpo da v1.0.0.

Este teste não substitui compilação, teste visual ou medição no Windows.
"""
from pathlib import Path
import json

root=Path(__file__).resolve().parents[2]
s=(root/'NativeHost/native_settings.inc').read_text(encoding='utf8')
checks={
    'Cinco abas originais com nomenclatura enxuta':'"Geral","Limpeza","Aparência","Bandeja","Avançado"' in s,
    'GDI com cartões de borda discreta':'ui_round_border(dc,r,ui_surface(),ui_outline()' in s,
    'Sem DLL de desenho adicional':'#include <d2d1.h>' not in s and '#include <gdiplus.h>' not in s,
    'Navegação apenas com texto':'ns_nav_caption(i,text,128)' in s and 'if(id>=NS_NAV&&id<NS_NAV+5)' in s,
    'Cinco páginas com rolagem independente':'g_ns_page_scroll[5]' in s and 'case WM_MOUSEWHEEL:' in s and 'ns_scroll_max()' in s,
    'Barra de ações fixa':'RECT footer=' in s and 'height-49' in s,
    'Aplicar desativado sem alterações':'IsWindowEnabled(draw->hwndItem)' in s and 'EnableWindow' in s and 'ns_is_dirty()' in s,
    'Cancelar descarta alterações':'case WM_CLOSE:DestroyWindow(hwnd);' in s,
    'Cartões de configuração Geral':all(x in s for x in ['"Inicialização"','"Janela e aparência"','"Segurança"','"Idioma"']),
    'Perfis de limpeza existentes':all(x in s for x in ['NS_PROFILE_LEVE','NS_PROFILE_EQUILIBRADO','NS_PROFILE_PERSONALIZADO']),
    '9 áreas de limpeza intactas':'NS_REGION_CONTENT (9*NS_REGION_ROW)' in s and 'ns_region_details[selected]' in s,
    'Classificação de risco':all(x in s for x in ['L"Recomendado"','L"Cautela"','L"Pode travar"']),
    'Alerta para operações críticas':'MB_ICONWARNING|MB_DEFBUTTON2' in s,
    'Prévia real da bandeja claro/escuro':all(x in s for x in ['HICON icon_dark=render_icon','HICON icon_light=render_icon','ns_draw_preview(dc,']),
    'Sem texturas ou imagens decorativas na UI':'LoadImageW' not in s and 'Gdiplus' not in s,
    'Amostra de notificação sem simular limpeza':'Nenhuma limpeza foi realizada.' in s,
    'Atalho global continua acessível':'EnableHotkey' in s and 'g_ns_hotkey' in s,
    'Diagnóstico e RAM residente':'GetProcessMemoryInfo' in s and 'Abrir pasta de logs' in s,
    'Sem limpeza automática':'ns_add(NS_FIRST+80' not in s,
    '44 idiomas preservados':len(list((root/'Languages').glob('*.json')))==44,
    'Identificação pública preservada':'v1.0.0' in (root/'README.md').read_text(encoding='utf8'),
}
for key, ok in checks.items():
    print(('OK    ' if ok else 'FALHA ')+key)
print(f'\n{sum(checks.values())}/{len(checks)} verificações estáticas')
raise SystemExit(0 if all(checks.values()) else 1)
