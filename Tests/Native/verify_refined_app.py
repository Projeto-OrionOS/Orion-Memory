"""Checagens offline dos requisitos de UI v1.0.0; nao substituem build Win64."""
from pathlib import Path
import json, re, sys
R=Path(__file__).resolve().parents[2]
settings=(R/'NativeHost/native_settings.inc').read_text(encoding='utf-8')
host=(R/'NativeHost/host.c').read_text(encoding='utf-8')
draw=(R/'NativeHost/native_draw.inc').read_text(encoding='utf-8')
checks={
'versao publica v1.0.0': 'v1.0.0' in (R/'README.md').read_text(encoding='utf-8'),
'cinco abas e nomenclatura revisada': '"Geral","Limpeza","Aparência","Bandeja","Avançado"' in settings,
'aplicar apenas com confirmacao': 'if(id==NS_APPLY){ns_commit();' in settings and 'atomic_settings_write(g_ns_draft)' in settings,
'cancelar sem persistir': 'case WM_CLOSE:DestroyWindow(hwnd);' in settings,
'controlar alteracoes pendentes': 'ns_is_dirty(void)' in settings and 'EnableWindow(GetDlgItem(g_ns_hwnd,NS_APPLY),ns_is_dirty())' in settings,
'rascunho de estado': 'g_ns_draft=next;ns_preview_refresh()' in settings,
'restaura padroes sem gravacao': 'free(g_ns_draft);g_ns_draft=doc' in settings,
'atalho por rascunho': 'ns_save("HotkeyModifiers",raw)' in settings,
'caches de memoria em nove regioes': 'region_keys[i]' in host and 'static const char *ns_region_details[]' in settings,
'sem checkbox de limpeza automatica': 'ns_add(NS_FIRST+80' not in settings and 'ns_add(NS_FIRST+82' not in settings,
'auto limpeza antiga bloqueada na leitura': 's->auto_threshold_enabled=0;' in host and 's->auto_interval_enabled=0;' in host,
'cor da interface #202020': 'RGB(32,32,32)' in draw,
'previsoes de icone claro/escuro': 'HICON icon_dark=render_icon' in settings and 'HICON icon_light=render_icon' in settings,
'abrir pasta dos logs': 'ShellExecuteW(hwnd,L"open",g_data_dir' in settings,
'30 idiomas incluidos': len(list((R/'Languages').glob('*.json')))==30,
'lancamento local sem GitHub': (R/'Executar-OrionMemory.bat').is_file(),
'script oficial Zig sem instalador': (R/'scripts/Build-Native.ps1').is_file(),
}
for p in (R/'Languages').glob('*.json'):
 try: json.loads(p.read_text(encoding='utf-8-sig'))
 except Exception: checks[f'idioma valido: {p.name}']=False
for ch in '(){}[]': pass
for k,v in checks.items():print(('OK' if v else 'FALHOU')+' - '+k)
print('Resultado:',sum(checks.values()),'de',len(checks),'verificacoes offline')
sys.exit(0 if all(checks.values()) else 1)