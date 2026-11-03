"""Verify that each selectable Orion Memory locale has a valid, complete dictionary.
This test does not assert language fluency and does not replace Windows UI tests.
"""
from pathlib import Path
import json, sys, re
root=Path(__file__).resolve().parents[2]
manifest=json.loads((root/'Languages.json').read_text(encoding='utf-8-sig'))
pt=json.loads((root/'Languages/pt-BR.json').read_text(encoding='utf-8-sig'))
keys=set(pt)
required={
    'Geral','Limpeza','Bandeja','Inicialização','Janela e aparência','Segurança',
    'Perfil de limpeza','Áreas incluídas na limpeza','Informações sobre a limpeza',
    'Notificações','Diagnóstico e segurança','Restaurar padrões','Cancelar','Aplicar',
    'Nenhuma alteração pendente','Autorização:','Configurada','Pendente',
    'Leve','Equilibrado','Personalizado','Recomendado','Cautela','Pode travar',
    'Testar notificação','Notificação de teste. Nenhuma limpeza foi realizada.'
}
errors=[]
codes=[x['Code'] for x in manifest]
if len(codes)!=44 or len(set(codes))!=len(codes):errors.append('Expected 44 distinct locales')
if len(list((root/'Languages').glob('*.json')))!=len(codes):errors.append('File count != manifest count')
for entry in manifest:
    code=entry['Code'];path=root/'Languages'/f'{code}.json'
    if not re.fullmatch(r'[a-z]{2,3}-[A-Za-z0-9-]{2,20}',code):errors.append(f'{code}: invalid BCP-47 code')
    if not path.exists():errors.append(f'{code}: file missing');continue
    try:d=json.loads(path.read_text(encoding='utf-8-sig'))
    except Exception as e:errors.append(f'{code}: invalid JSON: {e}');continue
    for key in keys|required:
        value=d.get(key)
        if not isinstance(value,str) or not value.strip():errors.append(f'{code}: missing/empty {key}')
    if code.split('-')[0]!='pt' and d['Geral']=='Geral':errors.append(f'{code}: main tab not localized')
    if code!='pt-BR' and d['Aplicar']=='Aplicar' and code.split('-')[0] not in {'es','pt'}:
        errors.append(f'{code}: Apply button appears untranslated')
    # Validate placeholders to avoid runtime unsafe swprintf when translated.
    s=d.get('Memória residente: %.1f MB','Memória residente: %.1f MB')
    if '%.' in s and '%.1f' not in s:errors.append(f'{code}: invalid resident-memory format')
settings=(root/'NativeHost/native_settings.inc').read_text('utf-8')
host=(root/'NativeHost/host.c').read_text('utf-8')
checks={
 'language catalogue capacity': 'g_ns_languages[64]' in settings and 'g_ns_language_count<64' in settings,
 'native navigation localized': 'translate(ns_pages[page],out,capacity)' in settings,
 'risk badges localized': 'translate(risk==0?' in settings,
 'language selection via staged Apply': 'ns_save("Language",raw)' in settings and 'if(id==NS_APPLY){ns_commit();' in settings,
 'dynamic buttons relocalized': 'ns_set_text(GetDlgItem(g_ns_hwnd,NS_TEST_NOTIFICATION)' in settings,
 'native locale autodetection': 'L"hi-IN"' in host and 'L"el-GR"' in host,
 'localized safety warning': 'translate("Orion Memory — operação avançada",caption,128)' in settings,
}
errors.extend(key for key,ok in checks.items() if not ok)
for key,ok in checks.items():print(('OK' if ok else 'FAIL'),key)
print('Valid locales:',len(codes),' | UI translation keys:',len(keys|required),' | Errors:',len(errors))
for err in errors[:25]:print('FAIL',err)
sys.exit(1 if errors else 0)
