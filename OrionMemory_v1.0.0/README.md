# Orion Memory

Gerenciador de memória nativo para Windows, **versão 1.0.0**.

O Orion Memory monitora RAM, memória comprometida e cache do sistema, oferece limpeza manual configurável, indicador dinâmico na bandeja e interface compacta com 30 idiomas.

## Projeto

| Pasta ou arquivo | Conteúdo |
|---|---|
| `NativeHost/` | Código C/Win32 do aplicativo |
| `Assets/` | Ícone e recursos gráficos |
| `Languages/` | Arquivos dos 30 idiomas |
| `Languages.json` | Catálogo de idiomas |
| `app.manifest` | Manifesto de execução e DPI do Windows |
| `Tests/` | Testes automatizados e medição de RAM |
| `scripts/` | Implementação do comando de compilação |
| `.github/workflows/` | Compilação pelo GitHub Actions |

## Compilar

No Windows, execute **`Compilar.bat`**. Ele utiliza o script interno `scripts/Build-Native.ps1` e requer **Zig 0.15.2**. Se o compilador não estiver disponível, o script oferece a opção de baixar a versão portátil oficial e verificar seu SHA-256.

O resultado fica em `dist/win-x64/OrionMemory.exe` e o ZIP portátil em `dist/OrionMemory_v1.0.0_win-x64.zip`. O aplicativo compilado não requer Zig nem .NET para funcionar.

Alternativamente, no PowerShell:

```powershell
.\scripts\Build-Native.ps1 -InstallZig -Runtime win-x64
```

## Testes

```powershell
python .\Tests\Native\revisao_qualidade.py
```

A compilação executa o autoteste nativo antes de publicar o executável. Os testes estáticos não substituem a execução e a medição de memória em Windows real.

> **Observação:** liberar caches ou conjuntos de trabalho pode aumentar leituras de disco; não há garantia de aumento de desempenho ou de consumo fixo de RAM.

## Distribuição

O repositório contém **somente o aplicativo e seus testes**, sem instalador e sem binários pré-compilados. A identificação pública oficial é **v1.0.0**. Hashes SHA-256 verificam integridade, mas não substituem assinatura digital Authenticode.
