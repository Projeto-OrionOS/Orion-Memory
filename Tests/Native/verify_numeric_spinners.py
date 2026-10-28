#!/usr/bin/env python3
"""Static regression checks for native Win32 unified numeric controls.
These checks do not claim that the Windows binary has compiled or executed.
"""
from pathlib import Path
import json
import re
import sys

root = Path(__file__).resolve().parents[2]
code = (root / 'NativeHost/native_settings.inc').read_text(encoding='utf-8')
original = (root / 'NativeHost/native_actions.inc').read_text(encoding='utf-8')
checks = []
def check(condition, label):
    checks.append((bool(condition),label))

check('#define NS_SPINNER_CLASS L"OrionMemory.NumericSpinner"' in code, 'Classe Win32 específica para controles numéricos')
check('static void ns_spinner_paint(HWND hwnd,HDC dc)' in code, 'Renderização integrada do fundo e borda')
check('ui_round_border(dc,r,ui_background()' in code, 'Fundo único com cantos arredondados')
check('ui_focus_outline()' in code[code.index('static void ns_spinner_paint'):code.index('static LRESULT CALLBACK ns_spinner_proc')], 'Borda de foco')
check('LineTo(dc,right,r.bottom-ui_scale(hwnd,7))' in code, 'Divisor visual das setas')
check('for(int direction=0;direction<2;++direction)' in code, 'Duas setas dentro do mesmo campo')
check('WM_MOUSEMOVE' in code and 'TME_LEAVE' in code, 'Hover com rastreamento de saída do mouse')
check('WM_CTLCOLOREDIT:case WM_CTLCOLORSTATIC:' in code and 'g_ns_spinner_brush;' in code, 'Campo de texto com o mesmo fundo do controle')
check('WS_CHILD|WS_CLIPCHILDREN' in code and 'SetParent(field->hwnd,container)' in code, 'EDIT nativo preservado para digitação e acessibilidade')
check('case WM_COMMAND:return SendMessageW(g_ns_hwnd,WM_COMMAND,wp,lp);' in code, 'Notificações do EDIT encaminhadas para a janela de configurações')
check('SendMessageW(g_ns_hwnd,WM_COMMAND,MAKEWPARAM(command,BN_CLICKED),0);' in code, 'Clique nas setas mantém validação e gravação já existentes')
check('ns_move(spinner,x-8,y-2,w+40,30)' in code, 'Posicionamento compacto com separação entre campos')
check('ns_show(spinner,f->page==g_ns_page)' in code, 'Controles aparecem apenas na aba correspondente')
check('if(f->kind==NS_NUMBER&&g_ns_spinners[i])' in code, 'Fallback do campo original se wrapper não for criado')
check('ns_create_fields();ns_create_spinners();ns_subclass_edits();' in code, 'Ordem correta de inicialização e subclassing')
check('cls.lpfnWndProc=ns_spinner_proc;cls.lpszClassName=NS_SPINNER_CLASS;RegisterClassExW(&cls);' in code, 'Classe registrada antes de criar a janela')
check('if(g_ns_spinner_brush)DeleteObject(g_ns_spinner_brush)' in code, 'Brush da interface liberado')
check('if(id==NS_AUTHORIZE){' in code and 'ui_fill(dc,r,ui_surface());' in code, 'Botão Autorizar com fundo consistente')
check('if(f->kind==NS_NUMBER&&notification==EN_KILLFOCUS){ns_save_number(f)' in code, 'Entrada numérica persiste ao sair do foco')
check('if(field->kind==NS_NUMBER&&(wp==VK_UP||wp==VK_DOWN))' in code, 'Setas do teclado preservadas')
check('case WM_KEYDOWN' in code and 'if(wp==VK_RETURN)' in code, 'Enter mantém confirmação do campo')
check('if(value>=g_settings.warning)' not in code, 'Sem alterações indevidas nas regras de valores')
check('static void a_real_operation(' in original, 'Motor original de limpeza preservado')
langs=list((root/'Languages').glob('*.json'))
check(len(langs)==30 and all(isinstance(json.loads(p.read_text(encoding='utf-8')),dict) for p in langs), '30 arquivos de idioma íntegros')
check((root/'Assets/OrionMemory.ico').stat().st_size>10_000, 'Ícone RAM oficial presente')

for ok, description in checks:
    print(('OK  ' if ok else 'ERRO'),description)
errors = sum(not ok for ok,_ in checks)
print(f'\n{len(checks)-errors}/{len(checks)} verificações estáticas passaram. Compilação e execução Windows: pendentes.')
sys.exit(1 if errors else 0)
