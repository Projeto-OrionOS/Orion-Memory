# Orion Memory

**Gerenciamento de memória para Windows, leve e nativo.**

Orion Memory é um utilitário para monitorar o uso de memória RAM e executar operações de liberação de memória selecionadas pelo usuário. Desenvolvido em **C com a API Win32**, sem depender do .NET para executar.

**Versão oficial:** `v1.0.0` · **Plataforma:** Windows x64

## Funcionalidades

- Monitoramento da memória física, memória comprometida e cache do sistema.
- Nove operações selecionáveis de limpeza de memória.
- Indicador de RAM na área de notificação, com menu contextual compacto.
- Configurações de aparência, ícone da bandeja, atalhos e notificações.
- Histórico de limpezas e opções de inicialização com o Windows.
- Até 30 idiomas, com detecção do idioma do Windows na primeira execução.
- Executável nativo, sem necessidade de instalar .NET.

> **Atenção:** liberar caches e conjuntos de trabalho pode aumentar leituras de disco ou prejudicar temporariamente o desempenho. O Orion Memory não aumenta a RAM física. Algumas operações exigem privilégios de administrador. Use opções avançadas com critério.

## Instalação

Após a publicação da versão oficial, baixe o pacote Windows x64 na seção **Releases**. Para a versão portátil, mantenha `Languages/` e `Languages.json` junto ao `OrionMemory.exe`.

A versão oficial do projeto é **v1.0.0**. A ausência de uma versão em Releases significa que o pacote de distribuição ainda não foi publicado ali.

## Compilar a partir do código-fonte

Requisitos: Windows x64, PowerShell e **Zig 0.15.2**. O script de compilação pode localizar uma instalação local do Zig ou baixar a distribuição oficial mediante autorização.

```powershell
.\Build-Native.ps1 -InstallZig -Runtime win-x64
```

O resultado esperado é `dist/win-x64/OrionMemory.exe` e, após validação, `dist/OrionMemory_v1.0.0_win-x64.zip`.

Para os testes:

```powershell
python Tests/Native/revisao_qualidade.py
.\Testar-Windows.ps1
```

A aba **Actions** contém um workflow de compilação Windows x64. Após enviar o código-fonte, ele pode produzir um artefato ZIP de teste para download. Um artefato de CI não equivale automaticamente a uma versão final assinada.

## Organização do projeto

| Caminho | Conteúdo |
|---|---|
| `NativeHost/` | Aplicativo nativo C / Win32 |
| `Assets/` | Ícones e recursos visuais |
| `Languages/` e `Languages.json` | Localização do aplicativo |
| `Tests/Native/` | Testes e verificações |
| `Build-Native.ps1` | Compilação e empacotamento |
| `.github/workflows/` | Compilação automatizada |

O instalador é desenvolvido e distribuído separadamente; este repositório se destina ao código-fonte do **aplicativo Orion Memory**.

## Segurança e privacidade

- Não execute arquivos obtidos de fontes não oficiais.
- Verifique os hashes de distribuição quando forem publicados.
- O aplicativo pode solicitar elevação apenas para operações que dependem dela.
- Configurações e histórico ficam no perfil do usuário.
- **Assinatura digital:** não presuma que um binário está assinado; confira nas propriedades do arquivo antes de instalar.

## Licença

O repositório ainda não declara uma licença de software. Tornar o código público não concede automaticamente permissão para redistribuí-lo ou modificá-lo. A licença será definida pelo mantenedor do projeto.
