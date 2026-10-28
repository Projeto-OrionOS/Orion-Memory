"""Verificacoes estaticas seguras. A compilacao e os testes Win32 sao obrigatorios no Windows.
Execute: python Tests/Native/revisao_qualidade.py
"""
from pathlib import Path
import json
import re
import struct
import sys

root = Path(__file__).resolve().parents[2]
host = (root / 'NativeHost' / 'host.c').read_text(encoding='utf-8')
settings = (root / 'NativeHost' / 'native_settings.inc').read_text(encoding='utf-8')
spinner = (root / 'Tests' / 'Native' / 'verify_numeric_spinners.py').read_text(encoding='utf-8')
build = (root / 'scripts/Build-Native.ps1').read_text(encoding='utf-8-sig')
bench = (root / 'Tests/Native/Medir-RAM-Windows.ps1').read_text(encoding='utf-8-sig')
index = json.loads((root / 'Languages.json').read_text(encoding='utf-8-sig'))
checks = []
def check(label, condition):
    checks.append((label, bool(condition)))

langs = list((root / 'Languages').glob('*.json'))
check('30 idiomas mantidos', len(langs) == 30)
for path in langs:
    try:
        json.loads(path.read_text(encoding='utf-8-sig'))
        ok = True
    except (json.JSONDecodeError, UnicodeDecodeError):
        ok = False
    check('JSON valido: ' + path.name, ok)
check('Catalogo de idiomas JSON valido', isinstance(index, list) and len(index) == 30)
check('icone RAM existente', (root / 'Assets' / 'OrionMemory.ico').is_file())
check('nao cria backups .bak nas preferencias', 'ReplaceFileW(g_settings_path,temporary,NULL' in host)
check('escrita atomica tem arquivo temporario', 'FlushFileBuffers(h)' in host and 'DeleteFileW(temporary)' in host)
check('auto limpeza permanece configuravel', 'native_automation_tick();' in host)
check('limpeza continua bloqueada em modo de teste', 'g_test_mode' in host)
check('limpeza tem verificacao de autorizacao', 'native_authorization_busy()' in host)
check('icone numerico permanece dinamico', 'render_icon(percent,g_icon_size)' in host)
check('nao consulta desempenho completo sem janela principal', 'if(g_main_hwnd&&GetPerformanceInfo' in host)
check('notificacoes da bandeja controladas', 'g_last_tooltip_tick>=5000ull' in host)
check('preferencia de transparencia preservada', 'json_bool(j,"WindowBlur",0)' in host and 'json_replace(revision,"WindowBlur","false")' not in host)
check('spinner numerico nativo mantido', 'NS_SPINNER_CLASS' in settings and 'WM_CTLCOLOREDIT' in settings)
check('testes numericos mantidos', 'ns_spinner' in spinner and 'WM_' in spinner)
check('compilacao gera exe nativo', '& $zig cc ' in build and '-target $target' in build)
check('compilacao testa antes de publicar', build.index('Start-Process -FilePath $temporaryExe') < build.index('Move-Item -LiteralPath $temporaryExe -Destination $targetExe'))
check('download oficial com SHA256', 'ziglang.org/download/index.json' in build and 'Get-FileHash' in build and 'SHA-256 invalido' in build)
check('compilacao usa Zig 0.15.2', "$zigVersion = '0.15.2'" in build)
check('arquivo ZIP portatil automatico', 'Compress-Archive' in build and 'OrionMemory_v1.0.0_$Runtime.zip' in build)
check('saida inclui 30 idiomas', '$localeCount -ne 30' in build)
check('medicao oficial privada residente', 'K32GetProcessMemoryInfo' in bench and 'PrivateWorkingSetSize' in bench)
check('medicao diagnostica sem limpeza real', '--native-test-dir' in bench)
check('workflow Windows gerado', (root / '.github/workflows/build-windows.yml').is_file())
# Check ambiguous parser forms $variable: (but allow ${name}: and drive scopes like $env:)
for name, script in [('scripts/Build-Native.ps1',build),('Tests/Native/Medir-RAM-Windows.ps1',bench)]:
    bad = re.findall(r'\$([A-Za-z_][A-Za-z_0-9]*):(?=\s|\$|["\'])', script)
    check('PowerShell sem variaveis ambiguas: '+name, not bad)
for label, passed in checks:
    print(('OK  ' if passed else 'FAIL') + label)
print('\nVerificacoes:',sum(v for _,v in checks),'/',len(checks))
sys.exit(0 if all(v for _,v in checks) else 1)
