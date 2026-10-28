# Orion Memory

**Gerenciador de memória nativo para Windows — v1.0.0**

O Orion Memory monitora o uso de RAM, a memória comprometida e o cache do sistema. Oferece limpeza manual configurável e um indicador dinâmico de uso de memória na área de notificação. Desenvolvido em **C/Win32**, sem exigir .NET para executar.

## Download do Orion Memory

Acesse **[Releases — Orion Memory](https://github.com/Projeto-OrionOS/Orion-Memory/releases)** para baixar o pacote para Windows x64, que inclui o aplicativo e os arquivos de idiomas.

## Funcionalidades

- Monitoramento de memória física, memória comprometida e cache.
- Nove operações selecionáveis de liberação de memória.
- Indicador numérico de RAM na bandeja e menu contextual.
- Configurações de aparência, atalhos e notificações.
- Histórico de limpezas e opções de inicialização com o Windows.
- 30 idiomas e detecção do idioma do sistema na primeira execução.
- Aplicativo nativo para Windows x64.

> A liberação de caches e conjuntos de trabalho pode aumentar a atividade de disco ou prejudicar temporariamente o desempenho. O aplicativo não aumenta a memória RAM física.

## Compilação

**Requisitos:** Windows x64 e PowerShell. O script utiliza o compilador **Zig 0.15.2**, que pode ser localizado no computador ou baixado em versão portátil mediante autorização.

Para compilar, execute `Compilar.bat`. Alternativamente, utilize:

```powershell
.\scripts\Build-Native.ps1 -InstallZig -Runtime win-x64
```

O script compila o aplicativo, executa o autoteste nativo e, se tudo passar, gera:

- `dist/win-x64/OrionMemory.exe`
- `dist/OrionMemory_v1.0.0_win-x64.zip`

Para executar a verificação estática do projeto:

```powershell
python Tests/Native/revisao_qualidade.py
```

Os testes de compilação e de comportamento devem ser confirmados em Windows real. O consumo de RAM pode variar de acordo com o sistema e as janelas abertas.

## Estrutura

| Caminho | Conteúdo |
|---|---|
| `NativeHost/` | Código-fonte C e recursos Win32 |
| `Assets/` | Ícones e imagens |
| `Languages/` | Traduções do aplicativo |
| `Languages.json` | Catálogo de idiomas |
| `Tests/` | Testes e diagnósticos |
| `scripts/` | Script nativo de compilação |
| `Compilar.bat` | Entrada única para compilar no Windows |
| `.github/workflows/` | Compilação automática Windows x64 |

## Publicação

A aba **Actions** permite executar o workflow `Orion Memory | Build Windows x64`. Após uma compilação bem-sucedida, o ZIP será disponibilizado como **artefato do workflow**. Artefatos de teste não são automaticamente uma Release oficial.

A versão pública do Orion Memory é **v1.0.0**. O código do instalador é mantido separadamente.

## Segurança

Algumas operações avançadas exigem privilégios do Windows. Verifique a origem dos arquivos e seus hashes SHA-256. Um hash não substitui uma assinatura digital Authenticode.

## Licença

Ainda não há licença de redistribuição definida. A disponibilização pública do código não concede automaticamente permissão de uso, modificação ou redistribuição além do que a lei permite.
