# Orion Memory v1.0.0 — Interface clean nativa

Orion Memory é um gerenciador de memória nativo para Windows 10/11 x64,
implementado em C/Win32 e desenhado principalmente com GDI.

## Revisão visual

Cinco abas reformuladas no estilo das prévias aprovadas, sem ícones decorativos
na navegação e nos cartões. Mantém o tema escuro `#202020`, as prévias reais do
indicador em barras de tarefas clara/escura, prévia de notificação, controles
nativos de configuração e as nove operações manuais.

As operações de limpeza são classificadas como **Recomendado (impacto relativo
menor)**, **Cautela** e **Pode travar**. Estas classificações não são garantia
de melhoria de desempenho. A limpeza automática permanece desativada.

## Como usar

O executável compilado é distribuído em um ZIP portátil separado, sem
instalador. Para compilar o código-fonte no Windows x64, execute
`Executar-OrionMemory.bat` (Zig 0.15.2).

Não é necessário instalar DLLs adicionais. Para manter todos os idiomas,
acompanhe o executável com `Languages.json` e a pasta `Languages/`.

## Idiomas

Esta revisão amplia a seleção para **44 variantes de idioma**: 30 existentes e
14 novas opções regionais/idiomas, incluindo inglês britânico, espanhol do México,
francês do Canadá, grego e hindi. Mudar o idioma e clicar em **Aplicar** atualiza
a navegação, os cartões, botões, alertas e mensagens localizados. Português do
Brasil é a linguagem-fonte; para textos ainda não revistos em certos idiomas,
é usado o inglês, evitando misturar a interface com português. Os idiomas regionais
aproveitam suas traduções completas da região original.

## Validação

Testes estáticos em `Tests/Native/verify_clean_mockup_ui.py` e demais suítes
existentes; testes interativos no Windows continuam necessários.

Identificação pública preservada em **v1.0.0**. O instalador está fora do escopo.
