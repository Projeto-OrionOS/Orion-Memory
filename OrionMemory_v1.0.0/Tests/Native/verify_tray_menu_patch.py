"""Offline regression checks for the Orion Memory tray-only UI adjustment.

Run from project root: python Tests/Native/verify_tray_menu_patch.py
Does not replace compilation or GUI tests on Windows.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
popup = (ROOT / 'NativeHost' / 'native_popup.inc').read_text(encoding='utf-8')
host = (ROOT / 'NativeHost' / 'host.c').read_text(encoding='utf-8')
settings = (ROOT / 'NativeHost' / 'native_settings.inc').read_text(encoding='utf-8')

checks = {
    'popup somente para bandeja': 'if (p->kind == 4)' in popup and 'if (p->kind == 0)' in popup,
    'nomes curtos somente em portugues': 'popup_portuguese()' in popup and 'tray_region_short_labels[9]' in popup,
    'as nove regioes preservadas': popup.count('L"') >= 9 and 'for (int i = 0; i < 9; ++i)' in popup,
    'menu lateral': 'popup_open_sub' in popup and 'popup_create(parent->owner, 4' in popup,
    'abrir para esquerda em borda de tela': 'room_right < estimate' in popup and 'point.x -= p->width' in popup,
    'clique direto em regiao': 'native_execute_command(command);' in popup and 'popup_build(p);' in popup,
    'menu permanece aberto apos marcar': 'if (p->kind == 4 && command >= CMD_REGION' in popup,
    'preserva protecao ultima regiao': 'count <= 1' in popup and 'g_settings.regions[i]' in popup,
    'operacoes com nomes completos intactas': 'static const char *region_labels[]' in host and 'Cache de arquivos do sistema' in host,
    'configuracoes nativas intactas': 'static void ns_save_hotkey' in settings,
    'sem novo dialogo de aplicar': 'Aplicar cor' not in popup and 'ConfirmWindow' not in popup,
    'teclado enter/espaco/esc': 'wp == VK_RETURN || wp == VK_SPACE' in popup and 'wp == VK_ESCAPE' in popup,
}
for name, passed in checks.items():
    print(('OK  ' if passed else 'FAIL'), name)
if not all(checks.values()):
    sys.exit(1)
print(f"{len(checks)} verificacoes estaticas aprovadas.")
