# Manual do Usuário do ChooChooTracker
> Esta é uma tradução automática do arquivo `docs/USER_MANUAL.md` do manual atual em inglês. Caso haja divergências, o texto em inglês prevalece.
>
> **Convenções.** A interface do aplicativo está em inglês. Mantenha nomes de telas, menus, campos e botões, códigos FX, nomes de módulos e engines, e extensões de arquivo em inglês.


ChooChooTracker é um tracker musical de 8 canais para consoles portáteis.

Segue o fluxo clássico e sistema de navegação do LSDj, com várias opções de design de som:

- Emulação vintage de chips e consoles
- Soundfonts
- Osciladores Mutable Instruments Braids, Plaits e Plaits-Alt, com filtro multimodo
- Samples PCM, waveforms de ciclo único e osciladores wavetable, com filtro multimodo
- aChChid, um sintetizador acid versátil baseado no Open303
- Bogie, sintetizador de bateria nativo com 12 modelos e filtro multimodo
- MonkSynth, sintetizador vocal com uníssono de dez vozes e delay estéreo
- MME, sintetizador multamodulação de dois osciladores com filtro multimodo
- Sintered, sintetizador digital experimental nativo com 6 modelos para bateria

## 1. Instalação e arquivos

### ArkOS / PortMaster

Instale `choochootracker.zip` através do PortMaster, ou copie o pacote extraído para o diretório `ports` do console. Inicie o **ChooChooTracker** pelo menu Ports.

### Miyoo Mini / Mini Plus (Onion OS)

Extraia o arquivo MiyooPorts na raiz do cartão SD para que as pastas `Roms/PORTS/` se mesclem com as já existentes do Onion OS. Inicie o **ChooChooTracker** pelo menu Ports após atualizar a lista. Faça backup da pasta `projects/` do app antes de substituir uma instalação existente.

O tempo de carregamento de um projeto pode variar conforme os tipos de instrumento usados na música. Se a inicialização ou o carregamento de um projeto parecer anormalmente lento, informe quanto cada um leva, junto com o modelo do console e a versão do Onion OS.

### Raspberry Pi OS 64 bits

Extraia o arquivo Raspberry Pi AArch64, instale o runtime do SDL2 (`sudo apt install libsdl2-2.0-0`) e execute `./chipnomad.sh` a partir do diretório extraído. Mantenha os instrumentos incluídos e demais recursos junto ao executável.

### Windows

Execute `choochootracker.exe` com `SDL2.dll` e `libwinpthread-1.dll` na mesma pasta.

No Windows, a entrada funciona com teclado (veja a seção Jazz mode para detalhes) ou Gamepad.

### Web

Você pode usar o Choochootracker online em https://choochootracker.vercel.app/ .

Controle esta versão com teclado, controles na tela em dispositivos móveis ou seu gamepad.

Configurações, samples, temas, projetos etc. são salvos no armazenamento do navegador.

Nota: devido a restrições do iOS, usar midi na versão web em iPhone pode ser impossível.

### Configuração de diretórios

| Pasta | Conteúdo |
|---|---|
| `AY_Wavetables` | Wavetables `.aywave` para chip AY |
| `fonts` | Fontes `.cnfont` do ChipNomad |
| `instruments` | Presets de instrumento `.cni`, bancos FM e Soundfonts |
| `pitch-tables` | Afinações alternativas em `.csv` |
| `projects` | Arquivos de música e projeto `.cct` |
| `samples` | Arquivos `.wav` para o engine Sampler |
| `SR_wavetables` | Wavetables em formato Serum para o engine BYOWTBL |
| `themes` | Temas `.cth` do ChipNomad |
| `title` | Recursos da tela de abertura |
| `waveforms` | Waveforms de ciclo único `.wav` para o engine 2xSCWF |

## 2. Controles

ChooChooTracker funciona melhor com um gamepad com D-pad, 2 sticks analógicos e 7 botões.

| Controle lógico | Padrão handheld | Padrão Windows |
|---|---|---|
| **[ESQUERDA/DIREITA/CIMA/BAIXO]** | D-pad | Teclas de seta |
| **EDIT** | A | X |
| **OPT** | B | Z |
| **PLAY** | Start | Espaço |
| **SELECT** | Select | Shift |
| Eixos de modulação | Sticks esquerdo e direito | Sticks do gamecontroller |
| **STICK LIVE** | L1 | Q |
| **MOTION RECORD** | L2 | W |
| **MOTION ERASE** | R2 | E |

Mapeamentos podem ser alterados em **Settings > Controls > Key mapping**, inclusive no Android com um gamepad externo. No PortMaster, salve o remapeamento com **Done**, solte o botão e continue nas Settings.

### Navegação

- Use **[CIMA/BAIXO/ESQUERDA/DIREITA]** para mover o cursor.
- Segure **SELECT + [DIREÇÃO]** para mudar de tela.
- Segure **OPT + [DIREÇÃO]** para navegação específica da tela.
- Sobre um valor, use **EDIT + [ESQUERDA/DIREITA]** para ajuste fino, ou **EDIT + [CIMA/BAIXO]** para ajuste grosso.
- Toque em **EDIT** para entrar um valor ou ativar um comando.

### Seleção e área de transferência

- **SELECT + OPT** ativa o modo de seleção e muda entre faixas úteis.
- Use **[DIREÇÃO]** para ampliar a seleção.
- Toque e solte **OPT** para copiar e sair do modo seleção.
- **OPT + EDIT** recorta a seleção.
- Fora do modo seleção, segure **SELECT** e pressione **EDIT** para colar.
- **EDIT + [DIREÇÃO]** edita as células selecionadas juntas.

### Reprodução

- **PLAY** começa a tocar a partir do cursor. Pressione novamente para parar.
- Você pode editar notas, instrumentos e chains durante a reprodução; mudanças soam no próximo tick do sequenciador.
- **SELECT + PLAY** inicia todas as tracks quando usado fora da tela Song.
- **EDIT + PLAY** pré-escuta um instrumento do Instrument Pool.

### Bounce Global

Selecione uma faixa de linhas e colunas nas telas Song, Chain ou Phrase, e dê dois toques rápidos em **EDIT** para abrir a tela BOUNCE TO SAMPLE. O bounce inclui todas as tracks selecionadas: tracks cuja primeira chain começa depois aguardam em silêncio e entram na primeira chain, garantindo que o arquivo tenha todas as chains das tracks selecionadas.

A tela oferece:

- **File name**: edite com o teclado de caracteres. Ao abrir, o próximo número sequencial livre (`001`, `002`...) é sugerido — o primeiro número cujo arquivo `.wav` ainda não exista na pasta de exportação. Edite livremente — o nome é usado tal qual. Se um arquivo com o nome já existir, um sufixo `_001`, `_002`... é adicionado para evitar substituições. O contador reseta ao carregar ou criar um projeto.
- **Sample rate** e **Bit depth**: mesmas opções da tela de exportação
- **Include in a sample name**: três caixas de seleção que adicionam tags ao nome do arquivo quando você pressiona **Start** (todas desmarcadas por padrão):
  - **[BPM]** — tempo atual do projeto, ex. `[120]`
  - **[Key]** — nota raiz e escala do projeto, ex. `[Cmaj]`, `[F#min Pent]`
  - **[Bars:Beats:16ths]** — duração renderizada do bounce: uma frase equivale a um compasso (16 semicolcheias), um beat a 4 semicolcheias. Frases completas mostram só compassos, ex. `[2]`; frases parciais mostram `[0:2]` (dois beats) ou `[0:1:3]` (um beat mais três semicolcheias). Para bounces multitrack, a maior track define. Uma seleção terminando no meio do beat conta uma semicolcheia a menos: a última linha selecionada marca o ponto de corte, que coloca o note-off no fim da semicolcheia anterior.
  - As tags são aplicadas na ordem `[BPM][Key][Length]` antes do nome do arquivo.
- **Start**: renderiza a seleção para arquivo WAV na pasta de exportação (veja *Export location* na tela Project)
- **Cancel**: volta sem renderizar

Enquanto renderiza, pressione **OPT** para cancelar. Após o bounce finalizar ou ser cancelado, retorna à tela original.

## 3. Estrutura de Song e mapa de telas

Segure **SELECT + [DIREÇÃO]** para navegar entre telas.

A linha principal de navegação é:

<pre>
        I
R P   G M
<strong>M S C P I T</strong>
D S     P W
</pre>

Linha central:

- **M**: Mixer (ramifica para Reverb e Delay)
- **S**: Song (ramifica para Project e Settings)
- **C**: Chain
- **P**: Phrase (ramifica para Groove)
- **I**: Instrument (ramifica para Modulation e Instrument Pool)
- **T**: Table (ramifica para o editor de AY Wavetable)

Linha superior:

- **R**: Reverb
- **P**: Project
- **G**: Groove
- **M**: Modulation
- **I**: Insert FX

Linha inferior:

- **D**: Delay
- **S**: Settings
- **P**: Instrument Pool
- **W**: Editor AY Wavetable

Uma música do ChooChooTracker segue esta hierarquia:

```text
Song -> Chain -> Phrase row -> Note + Instrument + FX
```

Uma música tem 8 tracks. Cada track contém uma sequência de chains, e cada chain contém uma sequência de phrases. Phrases são sequências de 16 passos de notas. Uma nota tem altura, instrumento, velocity e até 3 FX por track. O instrumento é o que gera o som.


## 4. Song, Chain e Phrase

### Song

A tela Song tem 8 colunas, 1 por track de áudio. Cada célula referencia uma chain. Uma música pode ter até 256 linhas (`00-FF`).

Tracks correm da esquerda para a direita, suas chains correm verticalmente. Cada track é independente; chains podem ter de 1 a 16 phrases, então tracks podem se perder em sincronia. Projete suas chains para permanecerem juntas, ou deixe-as flutuarem se o track pedir isso.

É prática comum na comunidade tracker reservar 1 chain para phrases vazias, geralmente `00` ou `FE`.

Você pode manter vários sub-songs num projeto, criando "ilhas": seções separadas por linhas vazias.

#### Live Mode

Dê dois toques rápidos em **OPT** para entrar ou sair do Live Mode; aparece `LIVE` no lugar do título da música. No Live Mode, **PLAY** inicia a chain selecionada na sua track quando essa está parada. Se a track já tocar, seleciona-se a chain para a próxima borda de chain; aperte **PLAY** duas vezes para troca imediata na próxima borda de phrase. Uma célula Song vazia selecionada ordena uma parada quantizada. `+` marca uma chain enfileirada, `!` chain urgente e `-` parada enfileirada.

Selecione uma linha da Song entre várias colunas para enfileirar ou parar essas tracks juntas. Seleções em múltiplas linhas são propositalmente rejeitadas. Fila Live é limpada durante faixa de Loop na Song. Live é modo de performance e não é salvo no projeto.

#### Controles

- **OPT + [CIMA/BAIXO]**: pula 16 posições para cima ou para baixo
- Selecione uma faixa, depois use **SHIFT + EDIT**: clone raso das chains mantendo as phrases originais
- Selecione uma faixa, depois dê dois toques em **SHIFT + EDIT**: clone profundo de chains e suas phrases
- Selecione uma faixa, depois use **EDIT + [CIMA/BAIXO]**: move seleção para cima ou para baixo
- Toque **OPT** 3 vezes: adiciona ou remove destaque para organização visual
- **OPT + SHIFT**: silencia track atual ou colunas selecionadas (solte **OPT** primeiro para manter silenciamento)
- **OPT + PLAY**: solo na track atual ou colunas selecionadas (solte **OPT** primeiro para manter solo)
- **OPT + [ESQUERDA/DIREITA]**: solo todas as tracks à esquerda ou direita da atual
- Selecione uma faixa, dê dois toques em **EDIT**: abre tela BOUNCE TO SAMPLE (veja Bounce acima)

Parar a reprodução e alterar mute ou solo usam uma breve correção de saída de 3 ms para reduzir cliques. Isso inclui também reativar o som e remover solos durante a reprodução.

### Chain

Chain é uma lista ordenada de phrases de 16 passos, com transposição opcional. Pode ter até 16 phrases; uma mesma phrase pode aparecer várias vezes. A segunda coluna define a transposição em semitons.

Um asterisco (`*`) aparece ao lado de chains reutilizadas no projeto. Você pode clonar chains da tela Song.

#### Controles

- **OPT + [ESQUERDA/DIREITA]**: muda entre tracks
- **OPT + [CIMA/BAIXO]**: muda entre chains na track atual
- Selecione uma faixa, use **SHIFT + EDIT**: clona phrases
- Selecione uma faixa, dê dois toques em **EDIT**: abre BOUNCE TO SAMPLE para as linhas selecionadas da chain na track atual (veja Bounce em Song)

### Phrase

Phrase é o padrão da track num sequenciador por passos tradicional.

Phrase tem 16 passos, 1 linha por passo. Cada linha contém nota, instrumento, volume de `00` a `7F` e colunas de FX. Notas usam notação tracker como `C-4` (nota e oitava).

Para parar uma nota, insira `NOTE OFF`, use o FX kill-note ou toque outra nota sem instrumento.

Quando a chain tem várias phrases, a tela Phrase funciona como editor contínuo. Mover abaixo da linha `F` ou acima da linha `0` abre a próxima ou anterior phrase da chain.

Um asterisco (`*`) aparece ao lado de phrases reutilizadas no projeto. Você pode clonar phrases da tela Chain.

O seletor de FX mostra comandos comuns e os suportados pelo instrumento daquela linha, evitando lista longa global de comandos específicos da engine. ChooChooTracker suporta mais FX que ChipNomad.

### Controles- **OPT + EDIT** sobre uma nota vazia: insere `NOTE OFF`
- **EDIT + [UP/DOWN]** na coluna de nome do FX: abre a tela de seleção de FX
- **OPT + [LEFT/RIGHT]**: move entre as tracks
- **OPT + [UP/DOWN]**: move entre frases na chain atual
- Selecione uma faixa na coluna de instrumentos, depois use **SHIFT + EDIT**: clona instrumentos
- Selecione uma faixa, depois use **EDIT + [UP/DOWN]**: rotaciona as linhas da frase
- Selecione uma faixa, depois dê dois toques rápidos em **EDIT**: abre a tela BOUNCE TO SAMPLE para as linhas da frase selecionadas na track atual (veja Bounce em Song)

Nesta tela, com uma seleção ativa, a coluna de notas pode ser preenchida
com padrões rítmicos:

- **OPT + LEFT** preenche as células das notas da seleção com um padrão fixo de 16 passos
  e avança para o próximo padrão a cada pressionar. Depois do último padrão, as
  notas são apagadas e o ciclo recomeça. Os padrões alinham-se às linhas absolutas
  da frase, então uma seleção parcial só recebe os passos que caem dentro dela.
- **OPT + RIGHT** preenche as células das notas da seleção com um padrão randomizado
  de 3-8 notas (7 e 8 são menos prováveis), ciclando indefinidamente enquanto **OPT**
  for segurado.
- **OPT + UP** muta as notas já colocadas na seleção: a cada pressionar ele escolhe
  1-3 delas e desloca cada uma por um valor aleatório dentro de uma oitava
  (deslocamentos pequenos são mais prováveis que os extremos). No 8º pressionar,
  as notas originais da seleção são restauradas, e o ciclo reinicia.
- **OPT + UP** em uma seleção que cobre somente a coluna de volume (sem células de
  nota, instrumento ou FX) randomiza as velocidades em vez de mutar: o primeiro
  pressionar aplica o padrão Minimal (cada volume varia até +-20), o segundo
  Medium (até +-50), o terceiro Random (valores totalmente aleatórios), e o quarto
  restaura os volumes originais da seleção quando o ciclo começou. Soltar **OPT**
  reinicia o ciclo para Minimal. Linhas sem volume permanecem intactas.
- **OPT + DOWN** espalha as notas posicionadas de instrumentos Sampler fatiados na
  seleção para um andamento cromático: a primeira nota de cada instrumento (mais
  alta no sequenciador) mantém seu valor, e cada nota seguinte desse instrumento
  sobe um semitom em relação à anterior, transformando uma pilha de notas idênticas
  em triggers consecutivos de slice. Vários instrumentos fatiados se espalham
  independentemente a partir de suas primeiras notas, notas em instrumentos sem
  slicing permanecem intactas, e o resultado nunca é ajustado à escala do projeto.
  A última nota escrita pelo espalhamento vira a última nota usada na entrada,
  então continuar o andamento manualmente seleciona o próximo slice. Uma seleção
  contendo apenas instrumentos fatiados espalha a cada pressionar; quando há notas
  não fatiadas também, o espalhamento roda na primeira vez, e o arp aleatório toma
  conta dessas notas a partir da segunda vez.
- **OPT + DOWN** numa seleção sem notas de instrumento fatiado é um arp aleatório
  para as notas já colocadas ali. Um acorde é escolhido da paleta CRD — acordes
  de 3 notas para seleções de 1-2 notas, de 3 ou mais notas para seleções de 3,
  qualquer acorde para 4 ou mais — com a raiz numa nota aleatória da escala do projeto
  dentro da oitava da seleção (qualquer nota cromática quando a escala estiver
  desativada ou a track for excluída), e seus tons são quantizados na escala como
  a reprodução faz para CRD. Cada pressionar redistribui o acorde pelas notas
  posicionadas: aleatório, do menor para o maior, do maior para o menor, depois
  sobe e desce; no 5º pressionar as notas originais são restauradas e o próximo
  pressionar escolhe um acorde novo. O nome do acorde aparece na barra de status
  enquanto o arp está ativo, não são adicionadas nem removidas notas, e soltar
  **OPT** reinicia todo o processo.
- Cada pressionar de preenchimento apaga as notas da seleção antes de escrever,
  e as notas colocadas assumem o último instrumento usado; soltar **OPT** reinicia
  os ciclos para a primeira etapa.

### Key jazz (desktop apenas)

Pressione **Esc** na tela Phrase para alternar o key jazz, um modo de entrada de
notas estilo piano QWERTY (como [m8c](https://github.com/laamaa/m8c)):

- `Z S X D C V G B H N J M` e `Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P` digitam notas
  cromaticamente por mais de duas oitavas, usando a posição física de cada tecla
  para que o layout seja o mesmo em teclados AZERTY, QWERTY e QWERTZ.
- Cada tecla pressionada escreve a nota na linha atual e avança para a próxima.
- `[` / `]` mudam a oitava.
- **Shift + setas** seleciona uma faixa de linhas e colunas; uma seta simples
  depois limpa a seleção.
- **Ctrl+C** / **Ctrl+X** copia ou recorta as colunas selecionadas, ou a nota/instrumento/volume
  da linha atual se nada estiver selecionado; **Ctrl+V** cola no cursor.
- **Delete** remove toda(s) linha(s) (todas as colunas) e puxa as demais linhas para
  cima para preencher o espaço; o cursor permanece na mesma linha.
- **Backspace** remove o elemento sob o cursor na coluna atual somente (ou nas colunas
  da seleção, se houver), puxa para cima o que está abaixo para preencher o espaço,
  e retrocede o cursor uma linha, como um editor de texto.
- **Insert** insere uma linha em branco, empurrando o restante para baixo.
- **Ctrl+S** salva o projeto. Fora do key jazz, Ctrl+S não faz nada.
- Pressione Esc novamente para desligar o key jazz.

Enquanto o key jazz está ativo, ele assume completamente as teclas de notas na tela
Phrase, incluindo atalhos Edit/Opt/Motion que compartilham essas teclas — desligue
com Esc para usá-los de novo.

O key jazz também funciona, ligado de forma independente com Esc, em:

- **Song** e **Chain**: digite um índice hexadecimal da chain ou frase diretamente
  (`0-9`, `A-F`) em vez de incrementar com Up/Down. O primeiro dígito digitado na
  célula substitui seu valor; dígitos subsequentes sem mover o cursor se inserem
  nela (digitar "3" depois "F" define 3F). No Song, **Shift+setas** seleciona uma
  faixa, **Delete** remove linhas inteiras e puxa o resto para cima, **Backspace**
  remove a(s) coluna(s) atual(is)/selecionada(s) e puxa o que está abaixo para cima,
  **Insert** insere uma linha em branco, e **Ctrl+C**/**Ctrl+X**/**Ctrl+V** copiam/recortam/colam
  as linhas selecionadas (ou a linha atual se nada estiver selecionado) — os mesmos
  atalhos de edição estrutural da tela Phrase. Isso assume o Shift, então Shift+Right/Up
  não pula mais para Chain/Project enquanto o key jazz está ativo aqui — Esc para recuperar
  esses atalhos.
- **Qualquer tela com campo de texto**: digite diretamente no teclado em vez do teclado
  virtual na tela. Isso inclui nome do arquivo do projeto, título e autor, nome do instrumento,
  nome do tema de cor, nome da tabela de pitch, nome do arquivo bounce e caixas de diálogo
  para nomear (digitar nome, criar pasta). Backspace funciona como num campo de texto normal;
  Shift digita letras maiúsculas. Esc ativa/desativa em todas essas telas ao mesmo tempo, e
  o teclado virtual ainda abre normalmente quando o key jazz está desligado.
- **Instrument**: com o key jazz ligado, as teclas de nota tocam o instrumento na track atual
  para audição, sem inserir nota (mesmo layout de piano da tela Phrase; a última tecla pressionada
  soa, e o som para ao soltar as teclas). As teclas de oitava mudam a oitava, começando pela oitava
  da primeira nota do instrumento. No campo **Name**, as teclas digitam texto em vez disso.
  Nada toca enquanto a música estiver rodando nessa track.

## 5. Instrumentos

Todos os instrumentos têm alguns parâmetros comuns:

- Tipo de instrumento
- Nome, até 15 caracteres
- Velocidade padrão da tabela, em ticks por linha da tabela
- Volume
- Pan
- Transpose ligado ou desligado

Cada instrumento tem uma tabela padrão com o mesmo número na faixa `00-7F`. Você pode usar
essas como tabelas auxiliares, mas manter tabelas auxiliares na faixa `80-FE` evita conflitos
e confusões. Tabelas são uma das principais ferramentas de design sonoro no ChooChooTracker.

### Controles

- **OPT + [LEFT/RIGHT]**: move entre instrumentos
- **EDIT + PLAY**: pré-escuta o instrumento
- **SHIFT + OPT**: copia o instrumento
- **SHIFT + EDIT**: cola o instrumento

### SOUNDFONT (experimental)

Copie bancos originais SF2/SF3, instrumentos SFZ/EXS com seus samples, ou arquivos ZIP
para `instruments/USER/soundfont`. Selecione **SOUNDFONT** no Instrument. A linha de escaneamento
mostra **Not scanned**, **Scanning...** ou **Scanned**, com a ação **Rescan**.
O escaneamento lê metadados; não carrega todas as amostras de todos os presets na memória.

A linha **Preset** mostra o preset ativo. Toque e solte EDIT para abrir o navegador nesse preset:
coleções aparecem à esquerda e seus presets à direita. Cada coleção apresenta uma lista plana,
incluindo presets dentro de arquivos bank e arquivos ZIP. Arquivos WAV/AIFF de suporte ficam
com seus instrumentos mapeados; coleções que contêm apenas samples individuais listam esses samples
como presets. IDs de download e hashes de arquivo são removidos dos rótulos; número de nota musical,
BPM e take são mantidos. EDIT seleciona um preset; EDIT+PLAY pré-escuta a seleção carregada.

Segure EDIT e pressione Left/Right em Preset para navegar sequencialmente na mesma ordem do navegador,
continuando na próxima coleção e circulando nas extremidades da biblioteca. Ciclagem rápida prepara
a última seleção pedida. O preset atual permanece audível enquanto substituições são preparadas.
Se o carregamento falhar, o preset pedido fica selecionado e o instrumento fica silencioso. **Error:**
e a razão aparecem abaixo do Instrument; a ciclagem continua a partir daquele preset em qualquer direção.
Rescan após importar ou alterar arquivos da biblioteca. O status do preset aparece logo abaixo de Preset,
com cor fixa. O carregamento não exibe mensagens em SONG ou outras páginas.

**Native keys** mostra o alcance original de notas-on do preset selecionado, usando C4 para a nota MIDI 60.
**(gaps)** indica que algumas notas dentro desses limites não têm zona mapeada. É a união das camadas
de velocidade; nem toda velocidade ou articulação precisa soar em toda tecla mostrada. Samples release-only (só liberados)
não ampliam o alcance. Afinação e pitch FX não alteram essa exibição nem ampliam o mapeamento original.

**Edit**, à direita de Preset, abre o editor experimental de SoundFont:
afinação (−2400 a +2400 cents), cutoff (20–20000 Hz), ressonância (0–40 dB), envelope
delay/attack/hold/decay/release (0–20000 ms), sustain (0–100%), volume, pan e
**Bend step** (1–100 cents por unidade PBN). Instrumentos novos usam passo de bend de 1 cent.
Arquivos antigos mostram **Stock**, mantendo a escala PBN anterior (25 cents no modo pitch linear,
1 no modo period); ajuste Bend step para 1 para bends mais finos. Clear restaura passo de bend a 1 cent.
Isso afeta apenas SoundFont PBN; FPT mantém afinação fixa. Volume e pan são os mesmos valores
do instrumento mostrados na página inicial. Campos de tom e envelope padrão são **Bank**, mantendo
o valor original de cada camada. Um valor explícito sobrescreve o parâmetro nas camadas do preset.
Clear restaura **Bank**, zero cents para Tune, volume total ou pan centralizado. EDIT+Left/Right faz
alterações finas; EDIT+Up/Down faz alterações maiores. OPT retorna para Instrument; EDIT+PLAY audiciona
o instrumento na nota da música ou na nota padrão. Isso edita controles de reprodução do instrumento,
não o mapeamento por sample do banco, zonas de tecla/velocidade ou pontos de loop.

Afinação, cutoff e ressonância aplicam-se ao vivo. Attack, delay e hold são definidos no note-on.
Decay, sustain e release podem atualizar uma voz tocando; release começa com um note OFF, não só na linha vazia.
Decay é inaudível com sustain 100%. Um tempo de release não pode estender um sample além do áudio disponível.
Um override explícito de release também faz um sample one-shot responder a note OFF. Com configurações Bank,
o comportamento one-shot original permanece. Os alcances chave e velocidade do preset são autoritativos;
notas fora deles são silenciosas. FPT pode mudar o pitch do sample selecionado além desse alcance sem mudar
seu mapeamento. Alterações salvam em arquivos de instrumento e projeto sem modificar o banco original;
músicas antigas sem overrides mantêm suas configurações originais do banco.Um WAV com um tipo de loop específico do sampler desconhecido pode tocar seu áudio sem esse loop. O aviso do Instrument nomeia o tipo de loop ignorado; o arquivo fonte permanece inalterado. Um loop WAV direto terminando exatamente um sample além do áudio é aceito como um fim exclusivo na borda do sample, com aviso; mapeamentos PCM e explícitos SFZ/EXS permanecem inalterados. Outros limites inválidos de loop e notas raiz geram erros específicos. Um único byte zero de alinhamento RIFF contado por engano em um layout de quadro PCM com tamanho ímpar é ignorado com aviso; outros quadros PCM parciais continuam gerando erros. Loops WAV padrão alternados/para trás ainda não são suportados.

**SoundFont FX** expõe oito controles no seletor contextual Lane FX. Todos os valores abaixo usam notação hexadecimal de tracker. Os FX afetam apenas sua faixa e deixam os controles salvos do instrumento inalterados. Cada nova nota começa a partir das configurações do instrumento, depois aplica os comandos na sua linha e nas linhas subsequentes.

| FX | Controle | Valores |
| --- | --- | --- |
| FPT | Pitch | `00` = −24 semitons, `18` = zero, `30` = +24; passos precisos de semitom |
| FCF | Cutoff | `00`–`FE`: 20 Hz–20 kHz, logarítmico |
| FRS | Resonância | `00`–`FE`: 0–40 dB |
| FAT | Attack | `00`–`FE`: 0–20 segundos, espaçamento mais fino em tempos curtos |
| FHD | Hold | `00`–`FE`: 0–20 segundos antes do decay, definido na linha da nota |
| FSU | Sustain | `00`–`FE`: nível mantido de 0–100% |
| FDC | Decay | `00`–`FE`: 0–20 segundos, espaçamento mais fino em tempos curtos |
| FRL | Release | `00`–`FE`: 0–20 segundos, espaçamento mais fino em tempos curtos |

Para FCF/FRS/FAT/FHD/FDC/FSU/FRL, `FF` retorna à configuração do Edit (incluindo **Bank**). Coloque Attack/Hold na linha da nota. Decay/Sustain/Release podem alterar a nota ativa; use OFF para ouvir o release. Volume e pan usam os controles comuns; comandos legados FVL/FPN ainda carregam e tocam mas não são mais oferecidos. Pitch FX alteram a afinação sem mudar a zona de teclas selecionada do bank. Comandos de pitch/fine-pitch do sequenciador, MODs e efeitos da faixa permanecem disponíveis. Projetos salvam referências estáveis de bank/preset e comandos FX; bibliotecas de samples permanecem externas.

Todos os banks são bancos USER. Instrumentos grandes podem exceder o cache PCM padrão de 64 MiB, e parâmetros de fonte não suportados geram avisos. Veja o [guia SoundBanks](soundbanks/README.md) para caminhos nas plataformas, importação pelo navegador, configuração do cache, cobertura de formatos e limites atuais de validação.

#### Compatibilidade SoundFont: bancos, presets e modulação

Um arquivo **bank SF2 ou SF3** pode conter muitos **presets**, junto com seus samples. Uma **coleção** da biblioteca é a pasta usada para agrupar esses arquivos no navegador. São níveis diferentes: um preset pode ter comportamento não suportado mesmo quando outros presets no mesmo arquivo funcionam bem.

**Modulação de preset** significa ligações programadas de entradas como velocity, tecla, mod wheel ou pressão para um parâmetro sonoro. Essas ligações podem pertencer a um preset ou a uma camada de instrumento subjacente compartilhada por vários presets. Não são necessariamente uma configuração que afeta todos os sons do bank.

| Recurso do preset | Tradução atual |
| --- | --- |
| Mapeamento sample/tecla/velocity, pitch raiz e afinação | Implementado; faixas autorais são mantidas. Zones de velocity e velocity comuns de tracker ainda funcionam. |
| Offsets de samples, loops suportados, gain/pan e grupos exclusivos | Implementado no parser e limites de vozes. |
| Cutoff/resonância low-pass estáticos e envelope de volume | Implementado via renderizador do tracker; não garante resposta idêntica a outro player SoundFont. |
| Rotas de modulação customizadas de preset/instrumento (`pmod` / `imod`) | Não implementado. Mapeamentos autorais de controle para parâmetro, quantidades e curvas de resposta são ignorados. |
| LFOs de modulação/vibrato SoundFont e roteamento de envelope de modulação | Não implementado; movimento programado de pitch, filtro ou volume pode estar ausente. |
| Hold/decay de envelope de volume escalado por tecla, geradores fixed-key/fixed-velocity | Não implementado. |
| Envio para chorus/reverb SoundFont | Não implementado. Efeitos de tracker podem ser ajustados manualmente mas não reproduzem as configurações do preset automaticamente. |

A mensagem antiga **“Custom bank modulation ignored”** descreve uma verificação a nível de arquivo: o parser encontrou registros de modulação customizada em algum lugar do bank. Isto **não** estabelece que o preset selecionado os usa, ou que o banco todo é inutilizável. Esta limitação geral está documentada aqui em vez de repetir essa mensagem sob cada preset. Diagnósticos detalhados do parser mantêm os registros; outros erros de carga e avisos de compatibilidade ainda aparecem quando aplicáveis. O engine também não reivindica comportamento completo de moduladores padrão do SoundFont. PBN do tracker, vibrato, Edit e Lane FX são controles independentes, não traduções automáticas da programação omitida do preset.

Ao escolher uma biblioteca, comece com presets baseados principalmente no mapeamento de samples, loops e envelopes estáticos. Sons cujo caráter depende de articulações por controle, filtros animados ou LFOs programados precisam de comparação mais próxima em um player SoundFont completo. **Carrega e toca** não significa **reproduz totalmente o preset**. Compare notas e velocities úteis, assim como notas mantidas e nota OFF. Mudar SF2 para SF3 (ou vice-versa) não elimina por si só essas necessidades de modulação. Uma conversão deve traduzir os mapeamentos e comportamentos para recursos suportados por este engine; renderizar um efeito em sample preserva apenas esse resultado renderizado, não sua resposta interativa a controladores. Veja a [cobertura de formato e limites](soundbanks/README.md#format-coverage) e as [notas de validação](soundbanks/validation.md) antes de escolher ou converter um bank. Esses limites descrevem a implementação experimental atual, não restrições inerentes a todos os players SoundFont.

#### Explorar coleções SoundFont

Em SAMPLE → SOUNDFONT, **Preset** abre suas coleções. **Right** ou **EDIT/A** entra diretamente em uma pasta. Em uma coleção folha, Right passa aos seus presets; outro Right avança 15 entradas sem carregar. EDIT/A carrega o preset destacado. Left volta às coleções e depois à pasta superior; OPT também volta. PLAY inicia/para a música durante a navegação.

Ícones de pasta e de som identificam as entradas. As coleções usam toda a altura do painel. O preset selecionado permanece sob as oito prévias de faixa; nomes longos rolam após um segundo de inatividade na seleção e reiniciam ao se mover. EDIT+Left/Right na página de instrumento segue a mesma ordem de pastas aninhadas, mantendo juntos os sons de cada coleção. EDIT+PLAY ouve.

A terceira linha informa Reading preset, Finding samples, Unzipping, Checking samples ou Loading samples. Instrumentos EXS com nomes embutidos vazios ou `(null)` usam seus nomes de arquivo. Os bancos de origem e o áudio não mudam. Use **Rescan** após modificar os arquivos da sua biblioteca.

Consulte [cobertura de formatos e limites](soundbanks/README.md#format-coverage) para o comportamento suportado do sampler e os formatos de sample.

### AY Classic, AY Plus e AY Sample

Estas são as engines originais ChipNomad AY/YM. AY Classic expõe controles de tom, ruído e envelope no estilo hardware. AY Plus adiciona osciladores de software e modulação mais rica. AY Sample reproduz um sample via volumes estilo AY e é distinto da engine Sampler.

#### AY Classic

Tipo de instrumento legado do ChipNomad `v1.0.0`. O instrumento AY Plus mais novo oferece mais recursos.

Instrumentos AY Classic têm esses parâmetros:

- Mixer: tom ligado ou desligado, ruído ligado ou desligado, forma de envelope (`0-F`)
- Volume: envelope ADSR gerado por software
- Período automático de envelope: ligado ou desligado, com uma taxa de `1:1` a `F:F`

**AY Quality** em Settings afeta só renderização AY/YM. Não altera qualidade de Braids, Plaits ou Sampler. **Sample dithering** se aplica à quantização de AY Sample.

Veja a [documentação ChipNomad AY-3-8910](https://chipnomad.org/chips/ay-3-8910/) para detalhes do chip.

#### AY Plus

AY Plus aplica conceitos clássicos de sintetizador ao design sonoro AY-3-8910/YM2149F. Pode-se pensar no AY como um sintetizador com 5 osciladores:

- 3 osciladores de tom onda quadrada
- 1 oscilador de ruído
- 1 gerador de envelope de amplitude que pode ser usado como oscilador

Neste modelo, cada instrumento tem 3 osciladores hardware: tom, ruído e envelope. AY Plus adiciona um 4º oscilador, de software. Atualizar registros AY mais rápido que a taxa de tick do projeto abre mais opções de design sonoro. Pelo funcionamento do AY, os osciladores são mixados usando modulação em anel. Experimente diferentes offsets de pitch, desafinação e combinações de osciladores para descobrir sons.

Cada oscilador tonal pode ser tunado independentemente via offset de pitch e fine tune.

Tipos e parâmetros dos osciladores de software:

- Pulse - onda quadrada com largura de pulso controlável
  - FM Depth - profundidade simples de FM
  - Pulse width - o intervalo completo é `00-FF`, enquanto os passos são definidos na tela do Project
  - Pulse low - controla o valor baixo em um par repetido cujo valor alto é sempre `15`
- Sync Tone - efeito hard-sync para o oscilador de tom
  - FM Depth - profundidade simples de FM
- Sync Env - efeito hard-sync para o oscilador de envelope
  - FM Depth - profundidade simples de FM
  - Pulse width - razão de duração entre as 2 formas no par de envelope
  - Envelope Pair - 2 formas de envelope para alternar
- Wavetable - wavetables de 32 passos, editadas na tela Wavetable
  - FM Depth - profundidade simples de FM
  - Wavetable index
- Tone FM - efeito simples tipo FM para o oscilador de tom
  - FM Depth - profundidade FM do tom
- Env FM - efeito simples tipo FM para o oscilador de envelope
  - FM Depth - profundidade FM do envelope

Osciladores de software Pulse e Wavetable não podem ser usados com o oscilador Envelope. O oscilador Envelope desliga automaticamente.

FM simples alterna rapidamente entre 2 valores de período, 1 menor e 1 maior. Isto é similar a usar uma onda quadrada como modulador FM. Pulse, Sync Tone, Sync Env e Wavetable usam razão `1:1` entre portadora e moduladora FM. Tone FM e Env FM podem imitar outras razões via offsets de pitch do oscilador de software.

#### AY Sample

Pode-se carregar arquivos WAV mono ou stereo em `8-bit`, `16-bit`, `24-bit` ou `32-bit`. São convertidos para `8-bit` e truncados para `16 KB`. Este limite corresponde ao tamanho da página RAM do ZX Spectrum.

AY toca samples através de escritas rápidas no registrador de volume. Sua saída é unipolar, então samples precisam ser “elevados a zero” para reprodução mais limpa. Isso remove cliques no fim do sample e melhora caudas silenciosas.

AY tem resolução DAC de `4-bit` com escala de volume não linear, então samples `8-bit` são convertidos para `4-bit` durante a reprodução. A opção Sample dithering em Settings pode melhorar a profundidade percebida. Dithering não é prático em plataformas retrô como ZX Spectrum e Atari ST, então mantenha desligado ao escrever para hardware real ou para buscar um som lo-fi granulado.

Samples se comportam como outro oscilador de software AY Plus, mas seus parâmetros extras facilitam o uso como tipo separado de instrumento.

Parâmetros do sample:

- Sample rate - tocar um sample em sua frequência nativa produz um `C-4`
- Sample start
- Sample length
- Sample loop start
- Sample pitch offset
- Sample fine pitch offset

Seções Tone e Noise são iguais às do AY Plus. Devido ao funcionamento do AY, todos os osciladores são mixados usando modulação em anel.

### Braids

Braids oferece 47 modelos de síntese. Seus controles principais são:

- **Model**: oscilador ou algoritmo de síntese
- **Timbre** e **Color**: macroparâmetros dependentes do modelo
- **Filter**: filtro LP/BP/HP adicional, inclinação de `12` ou `24 dB`, cutoff e ressonância
- **ADSR**: attack, decay, sustain e release

Toque em **Model** para escolher em listas categorizadas de modelos.

| Categoria | Modelos |
|---|---|
| Analog | `00 CSAW`, `01 MORPH`, `02 SAW-SQUARE`, `03 SINE-TRI`, `04 BUZZ`, `05 SQUARE-SUB`, `06 SAW-SUB`, `07 SQUARE-SYNC`, `08 SAW-SYNC` |
| Multi Osc | `09 TRIPLE-SAW`, `10 TRIPLE-SQR`, `11 TRIPLE-TRI`, `12 TRIPLE-SINE`, `13 TRIPLE-RING`, `14 SAW-SWARM`, `15 SAW-COMB`, `16 TOY` |
| Filter / Voice | `17 FILTER-LP`, `18 FILTER-PEAK`, `19 FILTER-BP`, `20 FILTER-HP`, `21 VOSIM`, `22 VOWEL`, `23 VOWEL-FOF`, `24 HARMONICS` |
| FM / Chaos | `25 FM`, `26 FEEDBACK-FM`, `27 CHAOTIC-FM` |
| Physical | `28 PLUCKED`, `29 BOWED`, `30 BLOWN`, `31 FLUTED`, `32 STRUCK-BELL`, `33 STRUCK-DRUM` |
| Drums | `34 KICK`, `35 CYMBAL`, `36 SNARE` |
| Wavetables | `37 WAVETABLES`, `38 WAVE-MAP`, `39 WAVE-LINE`, `40 WAVE-PARA` |
| Noise / Granular | `41 FILTER-NOISE`, `42 TWIN-PEAKS`, `43 CLOCK-NOISE`, `44 GRAN-CLOUD`, `45 PARTICLE`, `46 DIGI-MOD` |

#### Parâmetros

Timbre e Color dependem do modelo selecionado. Consulte o manual original do Braids da Mutable Instruments para a lista completa de modelos e detalhes, ou simplesmente toque de ouvido.

#### VCF e VCA

Braids alimenta o filtro multimodo padrão do ChooChooTracker e o envelope ADSR.

#### Configurações

Configurações globais do Braids estão disponíveis em Settings do app:

- **BITS**: reduz a resolução de saída
- **DRFT**: adiciona instabilidade de pitch do oscilador
- **SIGN**: reproduz o algoritmo original de imperfeição de forma de onda do Braids

### aChChid**aChChid** é um motor de baixo acid monofônico baseado no Open303. `Square` e `Saw` usam seu oscilador nativo TB-303, filtro, envelope e comportamento de acento. `Braids` substitui apenas o oscilador, continuando pelo mesmo filtro 303 e caminho do amplificador. Ele expõe Model, Timbre, Color e um waveshaper extra. O shaper evolui de saturação suave até wavefolding.

Cutoff e Decay usam edição logarítmica: **EDIT + LEFT/RIGHT** move um passo musical, enquanto **EDIT + UP/DOWN** move dezesseis passos. Cutoff cobre de 200 Hz a 20 kHz; Decay cobre de 200 ms a 2 s.

Um `F` na coluna de volume da nota dispara um acento. `ASL` desliza para essa nota a partir do tom anterior sem reativar o envelope 303; mantém automaticamente o gate anterior, e `ASL 00` resulta em um glide de `60 ms`. `ATY` mantém a nota aberta durante todo o passo atual. Notas sem `ASL` sempre reativam o envelope.

Destinos de modulação incluem Decay e Accent, além de Timbre e Color no modo de onda Braids.

### Bogie

**Bogie** é um sintetizador nativo de bateria one-shot. Notas do tracker definem o pitch e o volume do tracker/instrumento define o nível; cada disparo tem seu próprio decay interno, portanto não há página ADSR. Possui 12 modelos: `Kick`, `Snare`, `Hat`, `Clap`, `Tom`, `Rim`, `FM`, `Noise`, `Cowbell`, `Cymbal`, `Shaker` e `Clave`.

Seis macros: **Decay**, **Tone**, **Sweep**, **Noise**, **FM** e **Drive**. Seus nomes se adaptam conforme o modelo (por exemplo, Noise no Kick é `Click`, no Cowbell o FM é modulação cruzada metálica). Cada macro está ativa em todos os modelos, mas seu papel musical varia: Sweep altera pitch, espaçamento dos bursts ou difusão metálica; FM adiciona modulação cruzada, camada metálica ou harmônico ring-like. Metade inferior do alcance é para sons convencionais; metade superior abre morphs timbrais amplos e resultados mais sintéticos.

Cada modelo pode usar o filtro multimodo LP/BP/HP compartilhado, com caráter Clean, Classic, Aggro ou Acid, inclinação `12`/`24 dB`, cutoff e ressonância. Bogie suporta todos os seus macros ativos mais Volume, Pitch, Cutoff e Resonance como destinos de modulação e gravação de movimento. Use Decay curto no Cowbell para ataques phonk agressivos; aumente FM para um toque metálico mais forte.

Na tela de Modulação, pressione **EDIT** em uma fonte para escolher uma família (Envelopes, LFO ou Sticks), depois sua fonte. **Left** e **Right** ciclam as fontes diretamente.

### Sintered

**Sintered** é um sintetizador nativo de percussão digital one-shot. Notas definem sua pitch fundamental; seus seis modelos são `Knot`, `Shard`, `Burst`, `Comb`, `Logic` e `Melt`. Cada modelo tem **Decay**, **Mod** e **Motion**, mais três controles específicos: Knot usa Ratio/Spread/Fold, Shard Ratio/Feedback/Bite, Burst Noise/Color/Feedback, Comb Time/Damping/Regen, Logic Rate/Pattern/Crush e Melt Ratio/Chaos/Drive. **Mod** é sempre a profundidade interna do acoplamento.

Motion é um macro envelope interno: valores abaixo de `80` usam forma AD, `80` desativa, e valores acima usam formato só de decay. Quanto mais longe do centro, mais rápido o envelope. Seus destinos são fixos por modelo, mantendo Sintered previsível em padrões tracker: Knot anima Mod/Fold; Shard Feedback/Bite; Burst Noise/Color/Feedback; Comb Damping/Regen; Logic Rate/Crush; Melt Chaos/Drive. Todos resetam o estado ao disparar nota e limitam o feedback interno para hits repetíveis.

### MonkSynth

MonkSynth é um sintetizador vocal. Modela vogal, caráter da voz, sopro, glide, vibrato, uníssono e delay a partir da página Instrument. EDIT abre todos os controles e o nome do patch.

Escolha um banco e um preset nos controles VOICE. ALL inclui FACTORY e USER; EDIT+PLAY ouve o preset destacado, enquanto EDIT o seleciona. Salve seus próprios sons em `instruments/USER/monk/`. Instrumentos e músicas salvos mantêm as configurações do MonkSynth.

Os valores são hexadecimais `00..FF`, exceto Voices (`01..0A`, de uma a dez). Attack, decay e release cobrem 0..5 segundos; o detune 0..50 cents e a abertura das vozes 0..0,5. Volume, Pitch e os seis controles principais de voz estão disponíveis como destinos de modulação e de motion recording.

Os dezesseis controles aparecem juntos em **MonkSynth FX**: `MWV` (vogal), `MIN` (voz), `MAM` (sopro), `MFL` (glide), `MFB` (profundidade do vibrato), `MSH` (velocidade do vibrato), `MUV` (vozes), `MDT` (detune), `MVS` (abertura), `MDM` (mix do delay), `MDR` (velocidade do delay), `MLV` (nível) e `EAT`/`EDC`/`ESU`/`ERL` (ADSR). Esses significados valem quando MONK é o instrumento ativo; os significados existentes dos comandos do MME não mudam. O grupo Trigger FX compartilhado mantém TDC/TCL sem entradas de envelope duplicadas.

### MME

**MME** (Multi Modulation Engine) é um sintetizador de dois osciladores focado na interação entre eles. Seus modelos são `Ring`, `Fold`, `Cross`, `VPM`, `Sync`, `Logic` e `Vocode`. Ambos os osciladores são internos e seguem as notas do tracker; `Interval` define a relação entre eles.

Seis macros: **Waves**, **Interval**, **Amount**, **Flow**, **Feedback** e **Shaper**. Waves interpola pares musicais A/B de formas de onda. Amount controla a interação principal, Flow muda direção ou variante do algoritmo, e Feedback é limitado mas fica extremo no final do seu alcance. Shaper é um estágio final compartilhado que vai do limpo à saturação e wavefolding.

MME usa o filtro multimodo LP/BP/HP compartilhado e ADSR. Os seis macros, Cutoff e Resonance estão disponíveis para modulação e gravação de movimento. Segure **EDIT** no Model para abrir o popup de seleção.

### MIDI Out

**MIDI Out** (desktop, PortMaster e navegadores Web MIDI) controla um dispositivo MIDI externo, em vez de sintetizar áudio: disparar uma nota envia um Note On MIDI real no **Channel** do instrumento (`1-16`) e, ao terminar, o Note Off correspondente. Volume vira velocity, e faixas de acordes enviam Note On/Off para cada voz. Não há som no ChooChooTracker — selecione o dispositivo MIDI em [Settings](#13-settings) primeiro. Veja [MIDI](#15-midi) para panorama completo, incluindo prévia sonora via teclado MIDI.

**Program** e **Bank high/low** (CC0/CC32) são opcionais — mostrados como `--` quando desligados, `Clear` desativa o campo guardando o valor. Enviados apenas antes da próxima nota, se forem diferentes da última mensagem daquele canal (não antes de toda nota, para evitar retrigger do envelope/patch do dispositivo receptor).

**MC1 number**-**MC4 number** definem qual número CC (`00-7F`) cada um dos quatro FX MC1-MC4 envia — veja [MIDI Out FX](#midi-out-fx). Também opcionais e `--`; um slot não configurado torna seu FX inerte.

### Classic architecture engines

Os motores desta categoria têm arquitetura VCO para VCF para VCA.

Tipos de VCO disponíveis:

- **Braids**
- **Plaits**
- **Plaits-Alt**
- **Sampler**
- **2xSCWF**
- **BYOWTBL**

Cada instrumento alimenta seu VCO em um filtro multimodo.

Filtro com chave para inclinação `12 dB` ou `24 dB`, funcionando como low-pass (LP), band-pass (BP) ou high-pass (HP).

Caracteres de filtro disponíveis:

- **Clean**: digital e estéril
- **Classic**: inspirado em sintetizadores analógicos americanos clássicos
- **Aggro**: inspirado em sintetizadores analógicos japoneses agressivos
- **Acid**: dobro do Aggro em agressividade

Classic, Aggro e Acid podem ficar bastante ressonantes. Comece baixo e aumente devagar.

Filtro alimenta um VCA controlado por envelope ADSR. A forma do envelope pode variar entre exponencial, linear e logarítmica.

#### Plaits e Plaits-Alt

**Plaits** oferece 24 motores, agrupados assim na seleção:

| Categoria | Motores |
|---|---|
| Analog / Waves | `00 VA VCF`, `01 PHASE DIST`, `05 WAVE TERRAIN`, `06 STRING MACH`, `08 VIRTUAL ANALOG`, `09 WAVESHAPING`, `11 FORMANT`, `12 HARMONIC`, `13 WAVETABLE`, `14 CHORD` |
| FM | `02 6-OP FM 1`, `03 6-OP FM 2`, `04 6-OP FM 3`, `10 2-OP FM` |
| Digital | `07 CHIPTUNE`, `15 SPEECH` |
| Texture / Noise | `16 SWARM`, `17 NOISE`, `18 PARTICLE` |
| Physical | `19 STRING`, `20 MODAL` |
| Drums | `21 BASS DRUM`, `22 SNARE DRUM`, `23 HI-HAT` |

**Plaits-Alt** é coleção de modelos alternativos para o Plaits original. A maioria é mais estranha ou experimental que o VCO médio.

| Categoria | Motores |
|---|---|
| Granular / Micro | `GLISSON`, `PULSAR`, `GENDY`, `SCANNED`, `LOOPBACK` |
| Phase / Harmonic | `PHASE WEAVE`, `SIDEBAND BANK`, `UNDERTOW`, `ATTRACTOR`, `LOCKSTEP` |
| Acoustic / Physical | `REED PIPE`, `BRASS`, `SHAKERS`, `CLAPS`, `FRESHETS FORMANT` |
| Polyphony / Harmony | `DIATONIC CHORD`, `SCALE STACK`, `WT DIATONIC CHORD`, `WT SCALE STACK`, `HELIX` |
| Digital / Weird | `BYTEBEAT`, `RULEFIELD`, `SPECTRAL SPIRAL`, `PHASE FLOCK` |

Parâmetros seguem design da Mutable:

- **Harmonic** controla relações harmônicas, equilíbrio ou escolha de modelo dentro do motor.
- **Timbre** vai do espectro escuro/esparso ao claro/denso.
- **Morph** explora outra dimensão timbral.
- **Main/Aux** mistura saída principal com saída alternativa do motor.

Significado exato depende do motor. Por exemplo, motores de acordes usam para tipo, inversão e forma de onda. Modelos físicos para material, excitação e decay. Motores de bateria para tom, caráter e decay. Consulte o manual Plaits para detalhes.

Para `CHORD`, `DIATONIC CHORD` e `WT DIATONIC CHORD`, Harmonic mostra nome do acorde selecionado (`MIN`, `MAJ`, `MIN7`, `MAJ7`, etc.) em vez do valor cru. Braids `WAVE PARA` nomeia voicings Color igualmente.

Toque em **Engine** para escolher da lista categorizada.

**Env Mode** tem 2 rotas:

- `TRIG` reproduz o módulo com TRIG conectado e LEVEL sem patch
- `VCA` mantém LEVEL aberto aplicando ADSR do tracker após a voz

#### Sampler

Este motor de Sample limpo reproduz samples PCM mono ou stereo carregados na RAM.- Toque **Sample** para carregar um WAV PCM não comprimido em `8-bit`, `16-bit` ou `24-bit`. Pressione **PLAY** no navegador para ouvir o arquivo destacado.
- Quando um sample está carregado, **EDIT** aparece ao lado do nome. Toque nele para abrir a tela de edição do sample. **OPT** ou **SELECT + [LEFT]** retorna ao instrumento.
- A tela de edição do sample mostra o nome do arquivo (com um marcador `*` quando o sample na RAM difere do arquivo no disco), informações do formato (taxa de amostragem, canais), uma forma de onda alta e os campos **Region**, **Select**, **Slice**, **Process** e **File**.
- As edições ficam apenas na RAM até serem salvas: o projeto armazena o caminho do arquivo do sample e recarrega o WAV do disco na próxima vez que o projeto abrir, portanto sair do tracker sem **Save**/**Save As** descarta as operações de processamento (o marcador `*` alerta isso). Carregar outro sample no instrumento também descarta edições não salvas e reseta a Region para o sample inteiro (Start `00`, End `FF`).
- **Region** define os marcadores de início/fim da reprodução, também disponíveis na tela do instrumento Sampler. Eles definem os limites normalizados de reprodução (`00-FF`); se Start for depois de End, o sample toca ao contrário. **EDIT + [LEFT/RIGHT]** ajusta finamente o marcador em passos de um (`01`) e dá zoom na forma de onda em torno dele enquanto **EDIT** estiver pressionado; **EDIT + [UP/DOWN]** faz ajuste grosseiro (passo 16) e retorna a visualização ao sample completo. **EDIT + OPT** reseta o marcador para o padrão (Start `00`, End `FF`).
- A visualização da forma de onda dá zoom em uma janela fixa de um segundo de áudio ao redor do marcador editado e move o painel para mantê-lo visível; samples que cabem na janela permanecem na visualização completa 1:1. O zoom é temporário: dura enquanto **EDIT** estiver pressionado, e soltar **EDIT** retorna para a vista completa 1:1, assim como os ajustes grosseiros. Entrar na tela sempre reseta a visualização ao sample completo.
- **Select** define uma seleção para processamento em quadros absolutos, independente dos marcadores de reprodução Start/End. **EDIT + [LEFT/RIGHT]** move uma alça vinte quadros e dá zoom nela enquanto **EDIT** estiver pressionado; **EDIT + [UP/DOWN]** pula por `frameCount/64` (mínimo 16) e retorna à vista do sample completo. **EDIT (toque)** numa alça copia a posição do marcador Start ou End atual para ela. **EDIT + OPT** numa das alças limpa toda a seleção. Com a seleção vazia, ambas as alças mostram `-`; quando invertidas, elas se trocam automaticamente. A seleção é estado da sessão, não é salva com o projeto, e entrar na tela reinicia ela com o intervalo da Region de reprodução (o sample inteiro com os marcadores padrão).
- **Slice** é uma linha única com quatro células de valor - **Mode**, **Count**, **Slice** e **Frame** - e é salva com o instrumento. Off toca o intervalo Start/End. Notas da Phrase selecionam slices cromaticamente a partir de **C-0**, e notas além do último slice retornam ao início (nota N toca o slice `N % count`, então uma sequência passando do último slice volta ao primeiro). Notas com slice não transponhem tom nem usam quantização de escala, mas **CRD** mantém o slice selecionado e transpõe suas vozes como acorde. Inícios de slices são marcados na forma de onda com linhas curtas de dois pixels de largura (laranja escuro; a linha do slice atual é mais clara), e o slice sob o cursor recebe uma faixa preta de fundo na forma de onda (a tonalidade de seleção não é desenhada sobre a faixa, mantendo-a totalmente preta e claramente distinta do resto da pré-visualização).
  - **Mode** cicla entre `OFF` / `EQUAL` / `AUTO` / `LAZY` (EDIT + esquerda/direita ou toque; EDIT + OPT desliga o slicing). Trocar modos inicializa os pontos de slice: **EQUAL** divide o intervalo Start/End igualmente em **Count** partes; **AUTO** executa uma detecção de transientes no intervalo e posiciona um slice em cada ataque encontrado (até **Count**); **LAZY** começa com um único slice cobrindo o intervalo inteiro. Trocar para LAZY guarda a configuração atual EQUAL/AUTO (count e limites), e trocar de volta para EQUAL ou AUTO a restaura exatamente - sem confirmação e sem perda. Slice e Stretch são mutuamente exclusivos: ativar um desliga o outro, e as células Slice ficam esmaecidas enquanto Stretch controla a duração.
  - **Count** mostra o número total de slices (`01`..`64`). **EDIT + [LEFT/RIGHT]** aumenta ou diminui um (mínimo 1, máximo 64), **EDIT + [UP/DOWN]** cicla entre contagens potências de dois `2 / 4 / 8 / 16 / 32 / 64` com wrap-around. Toda mudança recalcula a divisão: **EQUAL** redistribui o intervalo igualmente, **AUTO** reexecuta a detecção com o novo alvo (sensibilidade da detecção depende do count — mais slices significa limite mais sensível). A caixa mostra `-` quando o modo está OFF. Em **LAZY** fica brilhante (conta os slices colocados manualmente) mas serve só para exibição: o cursor a pula — mover para direita a partir de **Mode** ou para esquerda a partir de **Slice** cai em **Slice**, descer da linha **Select** no marcador **END** também vai para cá, e tocar na caixa não faz nada (slices manuais não possuem divisão para recalcular). **EDIT + OPT** e toques simples em EDIT não fazem nada aqui: desligar slicing é feito em **Mode**, deletar um slice é em **Slice**.
  - **Slice** é o navegador: mostra o slice atual 1-indexado (`01`..count). **EDIT + [LEFT/RIGHT]** navega slice a slice, **EDIT + [UP/DOWN]** pula quatro slices (ambos limitados, sem wrap-around); o slice selecionado é indicado na forma de onda pela marca mais clara e pela faixa preta, e a vista dá zoom centralizando no início do slice. **EDIT + OPT** deleta o slice atual (ele se funde no anterior; o primeiro slice não pode ser deletado, e deletar o último slice ativa o modo OFF — os limites sobrevivem ao ciclo OFF e voltam ao escolher um modo novamente). **EDIT (toque)** pré-escuta o slice selecionado como playback one-shot (funciona em todos os modos de slice; a pré-escuta para quando a tecla é solta ou ao sair da tela). A caixa mostra `-` quando o modo está OFF.
  - **Frame** mostra o quadro inicial do slice atual em hexadecimal. **EDIT + [LEFT/RIGHT]** move em passos de dez quadros (com zoom no marcador enquanto **EDIT** estiver pressionado), **EDIT + [UP/DOWN]** move cem quadros e retorna para vista completa. O marcador fica dentro do próprio slice: não pode ultrapassar o início do slice anterior ou do próximo. **EDIT + OPT** deleta o slice.
  - **B** volta para a tela do instrumento a partir da linha Slice, como em todo canto da tela; **SHIFT + LEFT** sai de qualquer lugar.
  - **Fluxo LAZY**: com LAZY selecionado, toque **PLAY** para começar um playback one-shot do sample completo — toca uma vez do início e para sozinho na altura e velocidade originais (um marcador verde de dois pixels acompanha a posição na forma de onda); tocar **PLAY** de novo para antes. Enquanto toca, cada clique em **EDIT** solta um slice na posição atual, não importa onde o cursor esteja (slices a menos de 50 ms de um existente são rejeitados com `Too close to slice`). Tocar o sample de novo mantém os cortes existentes e adiciona novos a cada posição clicada, em ordem — a caixa **Count** atualiza com cada slice adicionado e o navegador **Slice** acompanha o novo slice. As mesmas regras de navegação e edição dos outros modos aplicam-se aos slices manuais: **EDIT + direção** no **Slice** navega entre eles, **Frame** ajusta o início selecionado, **EDIT + OPT** deleta o slice atual. A célula Frame fica esmaecida enquanto o playback-drop está armado. **SHIFT + PLAY** não é interceptado no LAZY: inicia playback da phrase como em todas as outras telas. Slices LAZY tocam no sequenciador igual a EQUAL e AUTO: notas da Phrase mapeiam cromaticamente a partir de **C-0** para os slices manuais, com wrap-around após o último.
  - **Dicas de combo**: enquanto o cursor estiver sobre uma célula da linha Slice e nenhuma outra mensagem aparecer, a barra de status mostra a dica dessa célula e a mantém enquanto o cursor ficar ali. Em **Mode**, a dica está ligada ao modo ativo e mostra só a descrição (o nome do modo já está na célula): `Divides sample in equal parts`, `Divides sample based on transients`, `Press Play and add slices with EDIT` — trocar modos troca o texto imediatamente, e OFF limpa a barra. Em **Count** mostra `Adjust the number of slices`, em **Slice** `Browse slices`, em **Frame** `Adjust slice start`. Qualquer mensagem de ação substitui a dica até expirar.
- **Process** seleciona uma operação de edição destrutiva: **Crop**, **Normalize**, **Delete**, **Silence**, **Fade In**, **Fade Out** ou **Reverse** (EDIT + esquerda/direita cicla, toque cicla para frente, EDIT + OPT desliga). **GO** (mesma linha) executa a operação selecionada na seleção atual, ou em todo o sample se a seleção estiver vazia. **Crop** mantém só a seleção; **Normalize** escala a seleção para seu pico atingir o máximo (os dois canais compartilham um ganho para preservar a imagem estéreo); **Delete** remove a seleção e junta as pontas (deletar o sample inteiro é rejeitado); **Silence** zera os dados; **Fade In**/**Fade Out** fazem rampas lineares da seleção para/desde silêncio; **Reverse** toca a seleção ao contrário (quadros são invertidos no lugar, os dois canais de um quadro movem juntos, comprimento e marcadores não mudam). Crop e Delete requerem uma seleção — com seleção vazia mostram `Select region first`. Cada operação pausa o áudio brevemente, mantém um undo de um nível, e marca o sample como modificado na RAM: o arquivo no disco não é alterado até os fluxos de salvar (veja abaixo), e sair da tela descarta a undo.
- **UNDO** (ao lado do GO) reverte o sample para o estado anterior à última operação. Fica esmaecido até uma operação ser executada, alterna entre estados pré e pós-op com pressões repetidas, e é reiniciado ao entrar na tela.
- **File** contém os fluxos de salvar. O instrumento mantém o caminho completo do WAV que foi carregado; esses fluxos escrevem esse arquivo ou apontam o instrumento para outro novo. Eles nunca tocam o nome do instrumento.
  - O campo File cicla entre **Save** e **Save As** (EDIT + esquerda/direita ou toque); **GO** (mesma linha) executa a ação mostrada.
  - **Save** sobrescreve o WAV que o sample foi carregado. Para um sample sem slices, pergunta `Overwrite <name>?` primeiro. Um sample com slices abre o diálogo **SAVE SLICES** perguntando onde salvar os pontos:
    - **SAVE TO SAMPLE** grava os pontos no WAV como chunks cue (portátil — DAWs mostram como marcadores) e mantém no projeto também,
    - **SAVE TO PROJECT** grava um WAV simples e mantém os pontos só no projeto,
    - **CANCEL** aborta.
    - Uma caixa `Don't ask again in this project` guarda a escolha no projeto e pula o diálogo depois (a escolha é salva como `- Sample save choice:` no .cct e só pode ser mudada editando o arquivo do projeto). Dialogo é pulado para samples sem slices. **Save** fica esmaecido (e ignorado na navegação) até carregar um sample com caminho. Uma falha na gravação mantém o marcador de alteração e mostra o erro.
  - **Save As** grava o sample atual em um arquivo novo: digite um nome (pré-preenchido com o nome atual sem extensão), depois escolha uma pasta no navegador. O sample é gravado como `<folder>/<name>.wav`, o instrumento aponta para o novo arquivo e a pasta é lembrada como pasta padrão de samples. Fica esmaecido (e ignorado na navegação) até carregar dados do sample, para que um sample recém-carregado possa receber caminho. Caminhos maiores que 255 caracteres são rejeitados.
  - O marcador `*` antes do nome do arquivo indica que o sample na RAM difere do arquivo no disco (qualquer operação de processamento ativa o marcador, Save e Save As limpam ele). O marcador é só da sessão: não é salvo com o projeto e reseta ao entrar na tela. Se vários instrumentos usam o mesmo arquivo WAV, salvar um sobrescreve o arquivo para todos eles.
- Na tela do instrumento Sample, use **EDIT + [LEFT/RIGHT]** para carregar o WAV anterior ou seguinte na mesma pasta.
- **Pitch** transpõe em semitons (`-48` a `+48`).
- **Loop** seleciona Off, Loop ou Ping-Pong.
- **Speed** controla time-stretch granular de `0%` a `500%` (`100%` é normal).Formatos WAV não suportados exibem um erro. Converta arquivos incomuns para WAV `PCM8`, `PCM16` ou `PCM24` antes de importá-los.

##### Entrada de áudio

Abra **Settings → Audio → Audio Input** antes de gravar de um microfone ou de uma interface de áudio USB. A lista mostra as entradas disponíveis naquele momento no dispositivo. Se uma interface armada for desconectada, a captura é interrompida e o gerenciador de áudio é reiniciado para que o app continue funcionando. Se você estava gravando, pressione **STOP** para encerrar a take interrompida. Após reconectar, selecione **[Refresh]** e confira se a entrada desejada está armada antes de gravar novamente; reconectar por si só não a rearma de forma confiável. **[List]** relê a lista de entradas atual sem reiniciar o áudio.

Marque a caixa **ARM** de uma entrada para disponibilizá-la ao gravador do Sampler; seu pequeno medidor mostra o nível de entrada. Arme apenas uma interface por vez: esta versão abre somente a primeira interface armada, mesmo que várias caixas estejam marcadas. Sua seleção é lembrada pelo nome do dispositivo, de modo que uma interface indisponível permanece desarmada em vez de ser substituída por outra entrada. Pressione **OPT** para voltar às configurações de Audio.

##### Gravação da entrada de áudio

Na tela de instrumento Sample, **REC** (ou **EDIT/REC** quando há um sample carregado) abre o editor de samples, que reúne gravação e edição em uma só tela. Arme primeiro as entradas na tela **Settings → Audio → Audio Input** (veja [Entrada de áudio](#entrada-de-áudio)). As linhas superiores do editor controlam a gravação:

- **Input** percorre a fonte de gravação: `T1`..`T8` gravam as oito faixas do tracker, seguidas do microfone ou da interface USB armados. Para gravar uma fonte externa, continue além de `T8` até aparecer o nome do dispositivo. `-` significa que nenhuma fonte foi escolhida explicitamente; **REC** usa então a primeira entrada armada e mostra seu nome. Segure **EDIT** e pressione **LEFT**/**RIGHT** para voltar/avançar, ou **EDIT+OPT** para voltar a `-`. Tocar em Input o avança diretamente.
- O medidor de nível ao lado de **Input** mostra o nível ao vivo da entrada armada (apenas fontes externas). **MON** ativa o monitoramento da entrada: a entrada armada é enviada à saída a -6 dB para reduzir o acoplamento. A configuração é salva no arquivo de configurações.
- **REC** inicia a gravação. Fontes externas gravam imediatamente; uma fonte de faixa mostra **ARMED** e a gravação começa quando a reprodução inicia (pressione **PLAY**). **STOP** encerra a gravação: o WAV é gravado na pasta de exportação do projeto (`recNNN.wav`), carregado no instrumento Sampler atual e a Region volta a ser o sample inteiro. Sair da tela com **OPT** durante uma gravação em andamento a encerra do mesmo modo.
- **PLAY** (exibido após uma gravação) ouve a take; vira **STOP** enquanto a prévia toca. **CLEAR** (ao lado) apaga o arquivo WAV e descarrega o sample do instrumento; após apagar, **REC** volta ao seu lugar.
- A forma de onda mostra a entrada ao vivo durante a gravação e a forma de onda completa depois de terminada. A linha de status mostra o nome do arquivo durante a gravação e o número de frames depois; uma take externa mostra também `drop 0` quando nenhum frame de áudio foi perdido. Um sample gravado é totalmente editável: região de loop, seleção, slicing, processamento e salvamento funcionam como em qualquer arquivo carregado.

As gravações são escritas na taxa de amostragem de áudio selecionada, como WAV estéreo de 16 bits. Fontes de faixa capturam o que os instrumentos da faixa renderizam (após os FX); fontes externas capturam diretamente a entrada do dispositivo.

#### 2xSCWF

2xSCWF é um oscilador de forma de onda ciclo único duplo. Carregue 1 WAV mono contendo exatamente 1 período em cada oscilador. Ambos os osciladores leem para frente e reiniciam ao final de seu ciclo.

Mix mistura crossfades A e B.

Detune é fino e exponencial de uníssono até `+200 ct`, depois avança em semitons de `+3 st` a `+24 st`.

2xSCWF trata cada arquivo como uma tabela de 1 ciclo e lê exatamente uma vez por ciclo de oscilador. Isso permite misturar e desafinar 2 ondas ciclo único.

A pasta factory `waveforms/AKWF/` contém formas de onda da biblioteca Adventure Kid. Muitas outras estão disponíveis online para expandir a coleção.

#### BYOWTBL

BYOWTBL é um oscilador wavetable duplo compatível com tabelas Serum. Cada oscilador carrega um WAV mono com frames consecutivos de ciclo único. `Pos A` e `Pos B` varrem suas tabelas independentemente de `00` a `FF`. O engine interpola linearmente dentro de cada frame e entre frames adjacentes. `Mix` crossfadeia os osciladores, e `Detune` desloca o oscilador B.

### Família de instrumentos FM

#### Instrumentos nativos OPLL e VRC7

O seletor Tipo de Instrumento tem um grupo FM com **OPLL / MSX** (YM2413) e **VRC7** (DS1001). Ambos agora oferecem **73 programas** em listas roláveis de Bank / Preset: os 15 tons originais, 40 tons adicionais distintos das tabelas YM2413/VRC7/YMF281B do emu2413, e 18 programas dois-operadores autorados por ChooChoo. Bytes de tons duplicados exatos são omitidos dentro de cada engine. As paletas adicionais usam o slot de tom programável do chip; não expandem a ROM física. O programa zero identifica um tom customizado. EDIT + esquerda/direita seleciona o preset anterior ou próximo dentro do filtro do banco. Fine ct ajusta o afinamento de -100 a +100 cents.

Na lista de programas, EDIT + PLAY audiciona o som selecionado; solte os botões para parar. EDIT confirma e OPT cancela. Navegar/audicionar não altera o instrumento, tabela ou música. O gesto usual de audição da página Instrumento continua funcionando após a seleção. Envelopes nativos do chip fornecem ataque/liberação; a Amp env opcional adiciona um ADSR em software (veja controles nativos do chip abaixo). Não há editor completo de operadores.

#### AdLib / OPL2 e OPL3

FM também inclui AdLib / OPL2 (YM3812) e OPL3 (YMF262). Bank filtra a lista factory; Preset abre grupos por categoria e vista All. EDIT + PLAY audiciona antes da seleção, EDIT seleciona, OPT cancela, e EDIT + esquerda/direita em Preset carrega a entrada anterior/próxima compatível. Fine ct ajusta o afinamento local. Mode mostra 2 operadores, 4 operadores ou voz Dual. OPL2 oculta patches incompatíveis OPL3; OPL3 pode tocar a coleção compartilhada de dois-operadores.

Os títulos dos popups Bank e Preset identificam o engine atual. Trocar engine ou slot do instrumento reseta o filtro do banco para **All banks**, incluindo ao alternar entre AdLib e OPL3 compatíveis. Voltando de um popup no mesmo slot mantém o filtro escolhido. Cancelar uma importação DX7 restaura o filtro anterior.

A confirmação do preset também publica edições feitas ao soltar botões para o próximo ciclo UI/áudio. Tocar músicas não requer outro clique ou reinício de transporte para aplicar o patch. A saída segue o buffer de áudio configurado e o envelope/retrigger do engine.

A coleção factory contém 697 entradas da fonte The Fat Man 2-op, The Fat Man 4-op e DMXOPL3, com 589 identidades de patch normalizadas únicas. Aliases mantêm nomes originais e atribuições. Níveis originais de operadores, flags de profundidade LFO, offsets de nota, tons fixos de percussão e afinações dual-voice são preservados. Volume tracker usa amplitude em software; não reproduz curvas originais de volume MIDI ou offsets por velocity. Estimativas de duração de release não cortam notas sustentadas. Alguns efeitos têm ataques lentos: segure Seashore em vez de esperar um clique curto para ouvi-lo.

Selecionar um preset mantém a tabela do slot e os inserts controlados pela track inalterados.

#### Instrumentos nativos Sega PSG e Game Boy

CHIP inclui Sega PSG, GB Pulse e GB Noise. No Sega PSG, **Bass range: Extended** mantém notas graves distintas. Escolha **Chip** para a faixa limitada original, onde notas mais baixas convergem. Ruído de taxa fixa mantém seu tom original; ruído derivado de tom segue a faixa de graves selecionada.
Os instrumentos GB usam registradores de pulse/ruído DMG; nenhum canal de onda do Game Boy é exposto. Suas listas de presets agora contêm **24, 24 e 28** presets respectivamente. As adições cobrem ruído Sega periódico/vinculado, varreduras de pitch GB, percussão curta, ruído metálico, drones e subidas. São programas originais, não sons extraídos de jogos.
A página oferece controles nativos de modo/dever/largura, envelope/varredura/ruído conforme aplicável, mais um ADSR de amplitude em software.

#### DX7 FM

Selecione **Type → FM → DX7 FM**; DX7 é a sétima opção, após Arcade / YM2151.
DX7 FM usa seis operadores. Sua página oferece Bank, Preset e Fine ct junto com configurações comuns de instrumento. A navegação usa listas de banco/categoria. Segure EDIT+PLAY para ouvir o som destacado; soltando para audição. EDIT confirma ao soltar; OPT cancela. Carregar um som copia seu patch completo sem alterar tabelas do tracker ou inserts da track.

Para biblioteca pessoal persistente, coloque arquivos `.syx` em **`instruments/banks/dx7/`** junto à biblioteca instrumentais existente. Subpastas são suportadas. Abra **DX7 → Bank**: cada arquivo aparece como banco nomeado, pronto para navegação em Preset. Reabra Bank ao adicionar/remover arquivos; não precisa converter ou carregar instrumento. Um banco DX7/TX7 original padrão contém **32 vozes**. Um arquivo com quatro mensagens de banco aparece como quatro bancos numerados (128 vozes no total). Arquivos de voz única também são aceitos. O patch selecionado é salvo com o instrumento; remover o banco fonte não altera a música.

**Load Instrument** também aceita vozes único e bancos 32-voz `.syx` originais DX7/TX7. Selecione um patch para usá-lo, ou cancele para manter o instrumento atual. Use a pasta Bank acima para biblioteca persistente; salve a música ou um `.cni` para manter patch editado. Formatos Yamaha não suportados são rejeitados.

DX7 tem limite de **16 notas ativas na música inteira**, incluindo vozes de acordes e releases. Ao atingir o limite, releases são silenciados primeiro, depois notas presas silenciosas, para manter ataques novos tocáveis. Audição de presets está indisponível durante a música.

#### Editor de operadores FM

Genesis FM, Arcade FM, OPL2, OPL3, OPLL e VRC7 têm botão **EDIT** à direita de Fine ct. Abre uma tabela dos parâmetros do patch:

- Genesis FM / Arcade FM
- OPL2 / OPL3: no OPL3 o modo (2op, 4op, dual) pode ser alterado aqui.
- OPLL / VRC7: editar um preset ROM o transforma em “Custom”.

Esquerda/Direita e as teclas usuais alteram valores; o instrumento continua tocando para ouvir as mudanças. A barra de status descreve o parâmetro selecionado. OPT ou Left+Shift volta à tela do instrumento. DX7 e SID não têm editor de operadores.

Genesis FM (YM2612) e Arcade FM (YM2151) usam o mesmo navegador Bank/Preset, audição e controles de fine tune que os outros engines FM. Seus envelopes nativos controlam o release; alterar volume ou pitch do tracker não os retriga.

#### Presets factory e USER FM

Instrumentos nativos usam **Bank:** para escolher ALL, uma coleção inclusa ou USER. **Preset:** escolhe um som dentro dessa seleção. Segure EDIT e pressione Esquerda ou Direita em qualquer linha para ciclar; um toque rápido em EDIT abre o seletor.

Coleções inclusas são pacotes ZIP sob `instruments/FACTORY/`. Arquivos USER vão em `instruments/USER/<engine>/`: `dx7`, `opll`, `vrc7`, `opl2`, `opl3`, `genesis`, `arcade`, `sid`, `sega`, `gb-pulse` ou `gb-noise`.
Navegue pastas diretamente ou coloque ZIPs de presets compatíveis lá. ZIPs permanecem intactos no disco. Todas essas pastas aceitam arquivos `.cni` compatíveis; CNI é formato portátil de instrumentos ChooChoo/ChipNomad, não padrão de fabricante de chip.
O navegador USER também lê os seguintes formatos fonte, inclusive dentro de ZIPs:

| Pasta USER | Formatos adicionais |
| --- | --- |
| `dx7` | vozes únicas e bancos 32-voz Yamaha DX7/TX7 `.syx` |
| `genesis` | `.tfi`, DefleMask `.dmp`, Furnace `.fui` |
| `arcade` | bancos VOPM `.opm`, DefleMask `.dmp`, Furnace `.fui` |
| `opl2`, `opl3` | `.wopl`, `.opli`, `.woplx`, `.sbi`, Furnace `.fui` |
| `opll`, `vrc7` | patches customizados Furnace `.fui` e seleções fixas de ROM |
| `sega`, `gb-pulse`, `gb-noise` | Furnace `.fui`, DefleMask `.dmp` |
| `sid` | GoatTracker 2 `.ins` (GTI5), Furnace `.fui` |

Um banco abre vozes compatíveis. Arquivos ou recursos não suportados mostram uma razão; vozes incompatíveis num banco misto são excluídas, mantendo números fonte estáveis. O carregamento é transacional: um preset rejeitado deixa o instrumento atual inalterado. Presets fonte substituem a tabela do tracker do instrumento por uma vazia, exceto SysEx DX7, que preserva a tabela existente. CNI restaura sua tabela salva. Patches carregados pertencem à música e continuam válidos após mover ou excluir o pacote fonte.

Bancos WOPL podem conter slots não usados sem flag explícita de vazio. O navegador omite slots sem nome cujos operadores ativos estão totalmente atenuados e não têm ataque. Vozes jogáveis sem nome permanecem disponíveis como `Program N [Bbank]` ou `Drum N [Bbank]`, usando números zero-base do programa e banco da fonte. Um instrumento OPLI único sem nome usa o nome do arquivo. Patches silenciosos nomeados são mantidos.

Arquivos Furnace, DefleMask e GoatTracker são importados como instrumentos, não como músicas inteiras. Macros animadas de origem, samples e configurações de playback específicas podem não ser transferidas. Coloque arquivos Game Boy nas pastas Pulse ou Noise apropriadas.

A ciclagem de presets USER continua entre arquivos, pastas, bancos e ZIPs em ambas direções, reiniciando nas extremidades. Reabrir o navegador USER segue a seleção atual. ALL inclui sons USER sob Unsorted para engines categorizados; USER permanece escolha separada no topo. Sega PSG e Game Boy usam listas ALL/Factory planas e navegador USER hierárquico, sem painel Unsorted. Arquivos DX7 antigos em `instruments/banks/dx7/` continuam acessíveis no USER.

Um engine mostra apenas coleções inclusas compatíveis. Factory Presets combina os tons OPLL/VRC7 incluídos para o engine selecionado; DX7 combina originais ChooChoo e YSE. Coleções nomeadas baixadas permanecem separadas. Fat Man 2-op está listado sob OPL2 e Fat Man 4-op sob OPL3. Arquivos CNI OPL2 compatíveis ainda podem ser usados em pastas USER OPL3.

#### Controles nativos do chip

Pré-visualizações de forma de onda de instrumentos nativos atualizam quando seus controles são redesenhados. Instrumentos FM podem sobrepor a curva opcional Amp env; o SID mantém sua prévia de forma de onda sem essa sobreposição FM, inclusive ao navegar bancos e presets.

Todos os sete engines FM oferecem **Bright**, **Feedback** e **Amp env**. Bright varia entre -63 e +63, com zero preservando o patch; altera níveis de operadores de modulação mantendo operadores portadores. Seu efeito audível depende do algoritmo. Feedback padrão é **Preset**, ou substitui o feedback nativo por 0–7. Esses controles não reescrevem bytes nativos salvos dos operadores.O envelope Amp padrão é **Bypass**. Selecione **ADSR** para adicionar ataque, decay, sustain, release e shape ao redor do som nativo. A/D/R usam um intervalo quadrático compartilhado de 0–5 segundos; sustain vai do silêncio ao nível máximo. Envelopes nativos continuam ativos, então esse envelope não pode prolongar o som além do release nativo. Navegar por presets preserva os controles de amp FM e timbre do slot.

Os seguintes FX de phrase também aparecem como destinos de Modulação e gravação de movimento suportados. Os valores na coluna FX são hexadecimais. Configurações se aplicam durante a reprodução sem alterar o instrumento salvo. Controles nativos de sweep e envelope do GB travam no próximo toque de nota; duty/width e frequência de ruído podem mudar ao vivo.

| Engines | FX | Controle |
|---|---|---|
| Native FM | `OL1`–`OL6` | Níveis absolutos de saída do operador: modulator OPLL/VRC7 e OPL `00–3F`; carrier OPLL/VRC7 `00–0F`; Genesis/Arcade `00–7F`; DX7 `00–63` (0–99). Valores maiores significam maior saída. Apenas operadores suportados aparecem. |
| OPLL/VRC7/OPL2/OPL3 | `OAR`, `ODR`, `ORR`, `OSL` | Atenuação de ataque, decay, release e sustain do operador 1, `00–0F`. |
| OPLL/VRC7/OPL2/OPL3/Genesis/Arcade | `OMU 00–0F` | Multiplicador de frequência do operador 1. |
| Genesis/Arcade | `LFR` | Taxa do LFO nativo: Genesis `00–07`, Arcade `00–FF`. |
| Arcade | `LAD`, `LPD 00–7F` | Profundidade separada de amplitude e pitch do LFO. |
| Genesis/Arcade | `LAS`, `LEN` | Sensibilidade nativa à amplitude `00–03` e habilitação do LFO `00–01`. |
| Genesis/Arcade | `LPS 00–07` | Sensibilidade nativa ao pitch. |
| SID | `SAT`, `SDE`, `SSU`, `SRL 00–0F` | Ataque, decay, sustain e release nativos. Valores de tempo aumentam em direção a `0F`; sustain aumenta para o nível máximo. |
| SID | `SPR 01–10` | Frequência do silent partner de 1× a 16×; afeta modulação em anel e hard sync. |
| Todos native FM | `FBK 00–07` | Feedback absoluto nativo, inicializado pelo instrumento. |
| Todos native FM com Amp env habilitado; Sega/GB | `EAT`, `EDC`, `ESU`, `ERL`, `ESH 00–FF` | Ataque, decay, sustain, release, shape. Estes não ativam um amp FM bypassado. |
| Sega PSG | `CMD 00–02`, `CNR 00–03` | Tom / ruído branco / ruído periódico; três taxas fixas de ruído ou taxa derivada de tom. |
| GB Pulse | `CMD 00–03` | Duty pulse nativo. |
| GB Noise | `CMD 00–01`, `CND 00–07`, `CNS 00–0D` | Largura do ruído, divisor de clock e shift. |
| GB Pulse | `CSP 00–07`, `CSS 00–07`, `CSD 00–01` | Período de sweep, shift, direção para baixo. |
| GB Pulse / Noise | `CEI 00–0F`, `CEP 00–07`, `CED 00–01` | Nível inicial do envelope nativo, período, direção crescente. |

A seleção de FX nativo usa o instrumento na coluna `I` da linha da phrase, ou o instrumento ativo encontrado retrocedendo quando `I` está vazio. O popup de FX mostra uma breve descrição do controle, seu valor atual no preset e o intervalo válido do comando em um bloco compacto. Selecionar um efeito nativo diferente começa naquele valor; reabrir o mesmo efeito preserva o valor editado. Seleção múltipla por linhas resolve os instrumentos de cada linha separadamente. As tabelas usam o contexto do instrumento. Edições de valores nativos param nos seus limites legais, incluindo duty, ruído, sweep, ADSR, feedback e níveis de operador. Os valores são hexadecimais: por exemplo, o máximo `63` no DX7 significa decimal 99.

#### Instrumentos SID e Phrase FX

Escolha **CHIP → SID**. Bank oferece **ChooChoo SID Originals** e **SIDkit Effects**. Navegue, ouça e salve como os presets nativos de outros chips.

A página do instrumento exibe forma de onda/largura de pulso, modo de filtro/corte/resonância e controles ADSR nativos. **EDIT**, ao lado do nome do preset, abre **SID EDIT**. Wave e PW têm rótulos separados; Filter tem uma linha para modo, depois Cutoff e Reso; os rótulos ADSR estão ao lado dos valores. Tocar cada campo seleciona exatamente aquele controle. Intervalos e comportamento de ajuste fino/grosseiro existentes são mantidos.

SID EDIT usa as linhas compactas do editor FM, cores do tema, valores hexadecimais e controles EDIT+direção. OPT ou Left+Shift volta; EDIT+PLAY escuta o instrumento. A página edita o patch armazenado existente diretamente, então as mudanças são salvas com o instrumento e projeto.

SID EDIT agrupa forma de onda, ring/sync, ADSR, largura de pulso, filtro, movimento, arpejo, varredura de pitch e vibrato. A barra de status identifica cada controle e seu intervalo. Ring e sync usam cada um dois dos quatro vozes SID disponíveis, então um acorde SID pode precisar de menos notas quando usados.

| FX | Intervalo | Ação |
| --- | --- | --- |
| `SCP` | `00–FF` | Base de largura de pulso, escalada para largura nativa de 12 bits; o movimento de pulso da receita permanece ativo. |
| `SCT` | `00–FF` | Base de cutoff, escalada para cutoff nativo de 11 bits; o movimento de filtro da receita permanece ativo. |
| `SRN` | `00–0F` | Ressonância nativa. |
| `SWV` | `00–07` | Triângulo, serra, tri+serra, pulso, tri+pulso, serra+pulso, tri+serra+pulso, ruído. |
| `SFI` | `00–07` | Bits do modo de filtro: 1 low-pass, 2 band-pass, 4 high-pass; 0 bypass. |
| `SMR` | `00–FF` | Clock macro da receita, escalado de 1 a 200 Hz. |
| `SRG` | `00–01` | Modulação por ring triangular com silent partner. |
| `SSY` | `00–01` | Sync de oscilador com silent partner. |

Largura de pulso e cutoff suportam `SLE`. Formas de onda discretas, switches, ressonância e feedback FM mantêm seus passos nativos úteis; valores maiores não criariam mais estados de hardware. `FBK` seleciona feedback diretamente `00–07`. Níveis de operador usam os intervalos específicos do engine listados acima. Ajustes de brilho e master mantêm o intervalo completo do byte.

#### Presets factory e USER para SID e chips nativos

Instrumentos nativos usam **Bank:** para escolher ALL, uma coleção incluída ou USER. **Preset:** escolhe um som dentro da seleção. Segure EDIT e aperte Left ou Right em qualquer linha para ciclar; um toque curto em EDIT abre o seletor.

Coleções incluídas são packs ZIP em `instruments/FACTORY/`. Arquivos USER ficam em `instruments/USER/<engine>/`: `dx7`, `opll`, `vrc7`, `opl2`, `opl3`, `genesis`, `arcade`, `sid`, `sega`, `gb-pulse` ou `gb-noise`.
Navegue direto nas pastas ou coloque um ZIP com pastas e presets `.cni` compatíveis ali. DX7 também aceita vozes `.syx` únicas e bancos de 32 vozes suportados, inclusive dentro de ZIPs. ZIPs permanecem intactos no disco. Carregamento direto WOPL/WOPLX não é suportado; use presets CNI compatíveis ou converta formatos suportados offline.

O ciclo de presets USER continua por arquivos, pastas, bancos e ZIPs em ambas direções, com wrap nas extremidades. Reabrir o navegador USER mantém a seleção atual. ALL inclui sons USER sob Unsorted para engines categorizados; USER continua uma escolha separada no topo. Sega PSG e Game Boy usam listas planas ALL/Factory e navegador USER hierárquico, sem painel Unsorted. Arquivos DX7 antigos em `instruments/banks/dx7/` continuam acessíveis no USER.

Um engine mostra apenas suas coleções incluídas compatíveis. Factory Presets combina os sets de timbres OPLL/VRC7 incluídos para o engine selecionado; DX7 combina originais ChooChoo e YSE. Coleções nomeadas baixadas permanecem separadas. Fat Man 2-op está listado em OPL2 e Fat Man 4-op em OPL3. Arquivos CNI OPL2 compatíveis podem ser usados em pastas USER de OPL3.

## 6. Modulação e gravação de movimento

Cada instrumento pode ter até 4 slots de modulação. Instrumentos de áudio podem ser direcionados a **Instrument Pan** ou **Track Pan**; ambos usam `00` para esquerda, `80` para centro e `FF` para direita.

Os tipos de modulação disponíveis são:

- ADSR - envelope clássico ataque-decay-sustain-release
- AHD - envelope ataque-hold-decay
- LFO - oscilador de baixa frequência com várias formas
- syncLFO (antigo SLFO) - LFO com um ajuste de ticks por unidade e multiplicador de período
- slowLFO - ciclos longos com um controle de período proporcional, que começa logo além do intervalo do LFO comum
- FLFO - LFO em frequência de áudio que pode alcançar kHz; FLFO pode ser pesado para a CPU
- SLIN - controle linear via joystick
- SRAT - controle baseado em velocidade via joystick
- Sequence - lê a sequência do tracker como sinal de modulação: **PITCH** é a altura sequenciada final, **VOL** o volume sequenciado final e **TRANCE GATE** um modulador rítmico independente. Motif, Rate e Length escolhem o padrão do gate; Trigger escolhe se roda livre, redispara a cada nota ou reinicia nos limites de frase ou chain. Gates fechados dão zero, abertos meio nível, e Accents distribui gates de nível total de forma euclidiana entre os gates ativos. Não usa nem altera nenhum insert de faixa.

A lista de destinos de modulação depende do tipo de instrumento.

Quando um envelope ADSR ou AHD tem como destino Volume, ele vira o envelope de volume do instrumento. Um LFO com destino Volume desloca o volume atual de saída em vez disso.

A quantidade de modulação pode ser positiva ou negativa. Seu intervalo depende do destino. Se o intervalo total do destino é `127` ou menos, como na maioria dos parâmetros, o valor define um intervalo absoluto. Destinos maiores, como pitch, usam quantidade escalada.

As fontes Sequence PITCH e VOL têm um **Mode**. **Normal** mantém audível o valor do tracker e também o usa como modulação. **Bypass** o transforma apenas em fonte: PITCH redispara na última altura (ou C-4 na primeira nota) e VOL mantém o último nível audível. Trance Gate já é apenas fonte. Se algum slot Sequence ativo coloca PITCH ou VOL em bypass, o bypass vence. Para evitar realimentação, PITCH e VOL não podem apontar para Pitch ou Volume.

Todos os parâmetros de duração, incluindo ataque, hold, decay e período, são medidos em ticks.

**syncLFO** é o antigo SLFO: ajuste seus ticks e seu multiplicador para um movimento sincronizado ao tempo. As configurações SLFO existentes e as músicas mantêm o mesmo timing.

Use **slowLFO** para varreduras mais longas do que o LFO comum permite. Seu controle Period vai de 256 a 131.072 ticks. Todos os LFOs baseados em ticks seguem o tempo da música e compartilham as mesmas formas, a mesma seleção de wavetable e os mesmos modos de trigger. Escolha Free para uma varredura que continua entre notas; Retrig, Phrase e Chain a reiniciam nos respectivos limites.

Formas de LFO:

- Tri - onda triangular bipolar
- Sin - seno bipolar
- UniTri - onda triangular unipolar
- UniSin - seno unipolar
- RampDn - rampa linear descendente (onda serra)
- RampUp - rampa linear ascendente (onda serra)
- ExpDn - rampa exponencial descendente (serra exponencial)
- ExpUp - rampa exponencial ascendente (serra exponencial)
- Square - onda quadrada
- Random - onda aleatória sample-and-hold
- Wavetable - lê uma das wavetables AY 32 passos do projeto. Valores `00` a `0F` mapeiam de `-100%` a `+100%` sem interpolação.

Tipos de gatilho do LFO:

- Free - reinicia o LFO somente quando o instrumento muda
- Retrig - reinicia o LFO a cada nota
- Phrase - reinicia LFO, syncLFO e slowLFO quando a reprodução entra numa nova frase
- Chain - reinicia LFO, syncLFO e slowLFO quando a reprodução entra numa nova chain
- Hold - para após 1 ciclo e mantém o último valor
- Once - para após 1 ciclo e retorna a zero

### Gravação de movimento

- **STICK LIVE** (`L1` por padrão) aplica modulação de stick sem alterar a phrase. Em **Settings > Stick live mode**, `HOLD` (padrão) ativa enquanto segurado; `TOGGLE` ativa num toque e desativa no próximo; `FREE` deixa permanente ativado. Soltar o botão em `TOGGLE` deixa ativo.
- Durante reprodução, segure **MOTION RECORD** (`L2`) para aplicar modulação de stick e gravar destinos alterados como valores FX absolutos.
- Segure **MOTION ERASE** (`R2`) para remover FX correspondentes dos destinos na linha atual.

O `~` no canto inferior direito indica Stick live ativo. Record (`*`, ou `!` para overflow) e Erase (`x`) têm prioridade e são momentâneos em ambos modos. Ambos ativam processamento de stick ao vivo enquanto segurados; soltar mantém um toggle live ativado separadamente.

O modo é salvo nas configurações do app, mas o toggle live sempre inicia desativado. Mudar modos limpa o toggle: `HOLD` segue qualquer botão STICK LIVE segurado; `TOGGLE` espera um toque novo; `FREE` fica ativo imediatamente.

A gravação de movimento escreve FX da track na phrase que está tocando. Atualiza FX correspondentes primeiro, depois usa slots vazios da direita para a esquerda. Nunca sobrescreve FX diferentes. Se os 3 slots estão cheios, o movimento não é gravado no step. Um `!` no canto inferior direito indica que mais destinos mudaram do que os 3 slots FX podiam guardar.

Gravação de movimento suporta destinos Braids, Plaits, Sampler, 2xSCWF, BYOWTBL e Bogie, incluindo macros Bogie ativos e controles de filtro quando aplicável.

## 7. Efeitos de inserção de track

Cada uma das oito tracks tem dois slots seriais, **TF1 → TF2**. De Instrument, use o gesto Shift+Up para abrir MOD, depois Shift+Up de novo para **F: Insert FX**. Shift+Down volta para MOD. OPT+Left/Right seleciona a track (1–8) sem alterar o seletor de instrumento. O cabeçalho do módulo abre o seletor existente; bypass mantém o módulo, valores configurados e automação. Parâmetros são valores byte: coluna esquerda 1–4, direita 5–8. Texto de contexto mostra nomes completos e valores decodificados. Projetos antigos carregam com ambos slots OFF.

O display de navegação mantém a coluna completa F–M–I–P visível em todas as telas do instrumento, incluindo Insert FX e a pool de instrumentos, destacando a tela atual.| Módulo | Parâmetros em ordem |
| --- | --- |
| Work Compressor | Threshold, Attack, Release, Makeup, Ratio, Detector source, Detector filter, Mix |
| Airwindows Distortion | Input, Mode, Output, Mix |
| Airwindows StereoDoubler | Detune, Mix |
| TAPESCAM | Input, Drive, Color, Wobble, Tone, Output |
| OTT (Rui-727) | Depth, Time, Upward, Downward, Input, Output |
| Chorus | Rate, Depth, Tone, Mix, Feedback |
| Flanger | Rate, Depth, Feedback, Mix |
| Phaser | Rate, Depth, Feedback, Mix, Stages |
| Rotary | Speed, Depth, Drive, Mix |
| Saturation | Drive, Tone, Level, Mix |
| Bitcrusher | Bit depth, Rate reduction, Tone, Mix |
| Destruction | Mode, Amount, Tone, Mix |
| FORMANT | Mode, Vowel, First formant, Second formant, Resonance, Mix |
| Trance Gate | Pattern, Rate, Length, Gate, Phase, Mode, Smooth, Mix |
| Beads (Mutable Instruments) | Time, Size, Density, Pitch, Scatter, Feedback, Shape/Freeze, Mix |

Controles contínuos usam 00–FF, exceto FORMANT VOW (00–7F). Controles bipolares têm neutro exato em 80. Modos de distorção são 00 Density, 01 Drive, 02 Spiral, 03 Mojo, 04 Dyno. Razões do compressor 00–07 são 1.5, 2, 3, 4, 6, 8, 16, 20:1; Compressor Detect 00–02 escuta a própria entrada estéreo, esquerda ou direita; 03–0A escuta em vez disso as faixas 1–8. Filtro do detector 80 é neutro, abaixo é low-pass, acima high-pass. Uma fonte de faixa é tomada após seu volume, pan e Tilt EQ mas antes de seus inserts; uma fonte silenciada ou ausente contribui com silêncio. Qualquer slot pode conter qualquer módulo; módulos repetidos são independentes. Chorus e Phaser começam com modulação audível porém controlada. Reduza Mix, Depth ou Feedback para efeito quase transparente; valores altos tornam o efeito intencionalmente extremo. Flanger e Rotary usam padrões de modulação suave. Rotary Speed usa toda a faixa **00-FF**: **00 = 0.01 Hz** (uma rotação a cada 100 segundos), **FF = 25 Hz** (o máximo antigo do Chaos). Espaçamento exponencial dá mais controle sobre movimentos LFO lentos. Novos inserts de Rotary padrão são **8F**, cerca de 0.8 Hz. Isso substitui os quatro valores discretos de velocidade; configurações antigas e automação F11/F21 precisam de ajuste (Slow/Fast/Hyper/Chaos equivalem aproximadamente a **8F/CB/E7/FF**). Outros parâmetros inalterados. Bitcrusher varia de 4 a 16 bits de profundidade e redução de taxa de 1x a 32x. Modos Destruction são Fold, Clip e Crush. Saturação e distorção têm controle de nível ou mix para balancear com o sinal original.
Trance Gate é um sequenciador de amplitude sincronizado ao tempo para pads, leads e stabs. Valores de Pattern 00–3F selecionam motivos musicais nomeados; 00–1F são grooves principais, 20–3F variações experimentais. Rate oferece valores simples, tríplices e pontilhados de 1/2 a 1/32. Length trunca o motivo rotacionado de 16 passos entre 1–16 passos. Cada nota no track reinicia a sequência; nota off deixa rodando o release. Forward, Reverse e Ping-Pong definem o sentido; Chaos aplica mutação repetível que preserva densidade a cada quatro loops. No insert master ele segue o tempo do transporte sem retriggers de nota. Gate define a largura do passo aberto, Smooth arredonda as bordas mantendo fade mínimo sem cliques, Mix faz crossfade entre áudio seco e gateado.

**Beads** usa o DSP granular original do Mutable Instruments como insert estéreo. Escolha Beads em um slot de insert de track. Seus oito controles usam comandos F11–F18 / F21–F28 existentes e destinos MOD de insert; não precisa de IDs FX ou formato novo. Grãos rodam continuamente sem eventos adicionais.

- **Time:** posição no buffer de gravação de quatro segundos.
- **Size:** grãos longos ao reverso em 00, grãos mais curtos perto de 55, depois grãos progressivamente mais longos para frente. FF ativa o modo delay original após a transição.
- **Density:** grãos regulares abaixo do centro, temporização randômica acima; zona morta central bloqueia novos grãos. Afastar do centro aumenta a taxa.
- **Pitch:** −24 a +24 semitons; 80 é uníssono exato.
- **Scatter:** 00 sem aleatoriedade. Aumentar aplica aleatoriedade interna uniforme de Time completa e Size na metade, independentemente por grão. Pitch e Shape permanecem fixos.
- **Feedback:** retorna grãos processados para o caminho de gravação.
- **Shape/Freeze:** 00–7F grava, 80–FF congela. Em qualquer faixa, os sete bits baixos varrem o mesmo envelope de retangular a decay, sino e reverso. Soma/subtrai 80 para alternar freeze sem mudar Shape: 40 e C0 têm o mesmo envelope. Dica mostra posição do envelope e status Gravação/Congelado. Segure EDIT com Up/Down para mudar +10 hex (16); Left/Right muda +1. Controle compacto e discreto para evitar passagem pelo bit freeze; modulação do byte inteiro pode cruzar os estados intencionalmente.
- **Mix:** 00 é dry exato, FF totalmente processado. Mudanças são suavizadas.

Freeze mantém o áudio capturado pelo efeito; trocar de módulo ou resetar Beads limpa o áudio. Envios de reverb da track permanecem disponíveis.

### Fazer um kick bombear a mixagem ou outra faixa

Para um master que bombeia, coloque o kick na faixa 1, selecione Compressor em MASTER FX, defina Detect como 03 (Track 1), Ratio como 04 (6:1), Attack perto de 60 (cerca de 1,6 ms), Release perto de C0 (cerca de 115 ms), Makeup como 00 e Mix como FF. Abaixe Threshold até os outros sons afundarem a cada kick e ajuste então Release para que se recuperem antes do próximo. O som direto do kick mantém seus próprios inserts de faixa mas desvia dos dois slots de insert master; seus retornos de reverb e delay bombeiam mesmo assim.

Para abaixar apenas um baixo, pad ou sintetizador, coloque o Compressor dele nessa faixa e selecione a faixa do kick com Detect. O kick de origem continua audível e seu volume não é alterado pelo compressor dessa faixa. Um kick e um baixo combinados na mesma faixa não podem ser separados com este roteamento.

### FORMANT

Selecione **FORMANT** em TF1 ou TF2 para moldar um som em tons de tipo vocal. Funciona melhor com uma fonte sustentada e rica em harmônicos, como uma onda dente de serra.

Defina MODE como `00` VOW e use **VOW** para varrer de A (`00`) a U (`7F`) passando por E, I e O. Defina MODE como `01` CUSTOM para ajustar você mesmo **F1** e **F2**. **RES** estreita ou alarga o caráter vocal; **MIX** mistura o efeito com o sinal original. Use MOD → INSERT FX para mover os controles ativos. MODE não é um destino MOD.

### Lane FX para Inserts

Comandos de insert usam **F11–F18** para TF1 e **F21–F28** para TF2. A seleção de FX no roll e o popup Phrase/Table mostram só parâmetros usados pelos inserts configurados na track selecionada. Popup agrupa como **TF1: nome do efeito** e **TF2: nome do efeito**; slots OFF estão ocultos. Inserts bypassados continuam editáveis. No popup FX da Phrase e Table, a descrição acompanha o slot de insert da track: mostra módulo, nome do parâmetro, intervalo decodificado e valor base salvo. Comandos de música existentes são preservados se mudar o módulo tornar inúteis; abrir o seletor inicia no controle disponível. Editando um valor, a dica inferior mostra valor decodificado e unidades. Qualquer coluna FX de Phrase ou Table pode controlar qualquer slot; a grade Phrase mantém 3 colunas. Valores absolutos mantêm-se entre notas, instrumentos, phrases, chains e loops comuns. Parar bruscamente ou novo playback reseta. Editar valor base na página F limpa só o override desse parâmetro. MOD age sobre valor base/automação efetivo sem salvar offsets. Selecionar módulo novo restaura padrão e limpa overrides. Bypass mantém automação mas suspende processamento após fade curto.

**Endereços referem-se a slots de track/posições de parâmetros, não nomes de efeitos.** Uma Phrase reutilizada em outra track usa módulos dessa track. Mudar módulo pode alterar significado do comando; comandos não são regravados. Endereços OFF/não usados são no-ops seguros e permanecem intactos nos dados salvos de Phrase/Table.

Seletor MOD do instrumento adiciona categoria Insert FX. Fontes de cada instrumento controlam inserts na track que o toca, incluindo sinal combinado de acordes. Mesmo instrumento em duas tracks afeta correntes independentes. Aplica-se sincronização, valores assinados, regras de combinação, modos stick e gravação de movimento existentes. Controles discretos são quantizados e limitados a opções válidas. Não há banco de modulação novo por track.

Comece com poucos inserts, cuide de overload ou estalos, e bypass efeitos que não ajudam a música. Drive e ganho podem saturar o mix; use o indicador de clip no Mixer para ajustar.

## 8. Tables

Tables são uma ferramenta essencial para design sonoro em trackers. No Vortex Tracker, combinam instrumentos e ornamentos, mas fazem muito mais. Se conhece LSDj ou NerdSEQ, a ideia já é familiar.

A coluna Pitch aceita valores relativos (`~`) ou absolutos (`=`) em semitons. A coluna Volume vai de `00` a `0F`, aplica-se a todo engine além de seu envelope, e pode ser usada como gate: alterne `0F` e `00` e depois faça loop com `HOP 00`. As 4 lanes FX correspondem em grande parte às lanes de phrase, embora alguns FX se comportem diferente em tables.

Colocar um FX `TIC` na última linha da tabela define a velocidade daquela coluna e substitui a velocidade padrão da tabela do instrumento. Cada coluna FX pode usar velocidade de tick diferente. Pitch e Volume seguem a velocidade da 1ª coluna FX.

Tabela de 16 linhas pode parecer curta perto de um instrumento extenso, mas o FX `HOP` cria loops condicionais como "repita estas linhas 5 vezes", além de loops aninhados. Com loops e velocidades independentes nas colunas FX, 16 linhas rendem bastante.

É possível copiar e colar lane FX entre phrases e tables ;)

### Modo Retrigger

Campo `Retrig` no topo da tela Table escolhe quando tabela ativa reinicia. `Inst` é modo padrão e compatível: uma linha da phrase com instrumento seleciona tabela padrão desse instrumento e inicia em linha `00`. `Phrase` reinicia tabela ativa entrando em phrase, `Chain` ao entrar em chain, e `Free` deixa rodando até parar a track ou comando explícito substituir.

Modo estrutural nunca busca no passado. Se uma track não tem tabela ativa, a primeira linha da phrase que insere instrumento ativa a tabela dessa linha exata, mesmo que seja linha `4`, `8` ou `F`. Números de instrumento posteriores não substituem tabela `Phrase`, `Chain` ou `Free` ativa. Isso torna tabelas úteis como automação tipo LFO sem simular passos anteriores silenciosamente.

`TBL`, `TBX` e `RET` são sempre ações explícitas: selecionam ou reiniciam imediatamente, independente do modo. Na fronteira de Phrase ou Chain, o reinício ocorre antes de ler a nova linha, então `TBL` ou `TBX` nesta linha têm prioridade final. FX na linha 00 da tabela rodam novamente normalmente; FX cumulativos mantêm acumulação usual.

Tables `00-7F` são reservadas para tabelas padrão de instrumento. Tables `80-FE` são para tabelas auxiliares iniciadas pelo FX `TBX`. Faixa inferior também funciona para auxiliares, mas pode causar conflitos/confusão.

### Controles

- **EDIT + [UP/DOWN]** na coluna do nome FX: abre seleção de FX
- **OPT + [DIREÇÃO]**: navega entre as tables

## 9. Instrument Pool e AY Wavetable

### Instrument Pool

Instrument Pool mostra todos os instrumentos do projeto e permite reorganizá-los. Durante a reprodução, também indica quais estão ativos.

#### Controles

- **EDIT**: edita o instrumento e vai para a tela Instrument
- **SHIFT + OPT**: copia o instrumento
- **SHIFT + EDIT**: cola o instrumento
- **EDIT + [UP/DOWN]**: reordena instrumentos
- **EDIT + PLAY**: pré-escuta o instrumento

### AY Wavetable

Wavetable é um tipo de oscilador de software AY Plus. Um projeto pode ter até 256 wavetables, cada um com 32 passos. Todos os instrumentos wavetable compartilham o mesmo conjunto de ondas. Aqui você edita elas.

Níveis de volume AY são não lineares, então a forma de onda Standard view é desenhada para refletir os níveis reais de saída. Dê dois toques rápidos em **OPT** para alternar entre Standard view e o persistente **LFO view**, usando o mesmo atalho do Song/Live Mode: o LFO view desenha os passos crus `0`–`15` linearmente, com linha central entre `7` e `8` como ponto zero de modulação.

A tela tem 2 linhas lógicas. A superior tem os botões **Load** e **Save**, a segunda contém o editor wavetable.

#### Controles

- **OPT + [LEFT/RIGHT]**: navega na lista de wavetable
- **OPT + [UP/DOWN]**: navega na lista em saltos de 16 ondas
- **SHIFT + OPT**: copia a wavetable
- **SHIFT + EDIT**: cola a wavetable
- **OPT + PLAY**: alterna entre Standard e LFO view

## 10. Tracker FX

Tracker FX forçam valores do sequenciador ou engine em passos individuais. Outros grooveboxes chamam isso de "parameter locks". Cada linha de Phrase tem 3 slots FX.### Sequencer FX

| FX | Valor | Comportamento detalhado |
|---|---|---|
| `ARP` | `XY` | Arpeja a nota base, `+X` passos e `+Y` passos. `37` produz um padrão de acorde menor. |
| `ARC` | `XY` | `X` seleciona a direção do arpejo ou modo de alcance; `Y` é sua velocidade em ticks. |
| `PVB` | `XY` | Vibrato de pitch: `X` é a velocidade e `Y` a profundidade. No modo Linear, a profundidade usa passos de `10 centésimos`. |
| `PBN` | signed `XX` | Adiciona `XX` unidades de pitch a cada linha de frase/tabela; `FF` significa `-1`. Use `00` para parar. |
| `PSL` | `XX` ticks | Desliza do pitch anterior para a nova nota em `XX` ticks. |
| `PIT` | signed `XX` | Deslocamento relativo acumulado em passos da tabela de pitch. |
| `FIN` | signed `XX` | Deslocamento fino acumulado em cents com pitch Linear, ou unidades de período caso contrário. |
| `PRD` | signed `XX` | Deslocamento acumulado relativo do período do oscilador. |
| `VOL` | signed `XX` | Deslocamento acumulado relativo de volume (`FF` é -1, não volume total). |
| `VSL` | signed `XX` | Adiciona `XX` ao volume a cada linha de frase/tabela. Use `00` para parar. |
| `PAN` | `00-FF` | Panoramização absoluta do instrumento: esquerda, centro (`80`), direita. |
| `TPN` | `00-FF` | Panoramização absoluta da faixa no mixer: esquerda, centro (`80`), direita. |
| `RET` | `XY` | Retriga a cada `Y` ticks; `X` aplica alteração de volume. `Y=0` para o retrig. |
| `DEL` | `XX` ticks | Atrasa o note-on. Atraso maior que o passo do groove atual pula a nota. |
| `OFF` | `XX` ticks | Envia note-off após `XX` ticks e entra na fase de release ADSR. |
| `KIL` | `XX` ticks | Mata a voz imediatamente após `XX` ticks sem usar release ADSR. |
| `TIC` | `XX` ticks | Define ticks por linha de tabela. Em tabela, altera a velocidade da coluna FX. |
| `TBL` | `00-FE`, `FF` off | Substitui e reinicia a tabela de instrumentos imediatamente; `FF` para a tabela. |
| `TBX` | `00-FE`, `FF` off | Inicia ou substitui uma tabela auxiliar imediatamente; `FF` para a tabela. |
| `THO` | linha `XX` | Salta todas as colunas da tabela do instrumento para a linha `XX`. |
| `TXH` | linha `XX` | Salta todas as colunas da tabela auxiliar para a linha `XX`; não usado dentro de uma tabela. |
| `GRV` | groove `XX` | Seleciona um groove para a faixa atual. |
| `GGR` | groove `XX` | Seleciona um groove para todas as faixas. |
| `HOP` | `XY` | Salta para a linha `Y`, `X` vezes; `X=0` repete indefinidamente. HOP em tabela afeta sua própria coluna. |
| `SNG` | signed `XX` | Move a reprodução `XX` linhas da música. |
| `PRO` | `00-64` | Avalia uma probabilidade absoluta de disparo de `0%` a `100%`. |
| `MOD` | `AB` | Dispara na passada `A` de um ciclo de `B` passadas. |
| `RNN` | `MX` | Toca uma nota aleatória. `M` escolhe o modo: `0` sempre aleatório, `1` aleatório a cada PLAY, `2` escolhe e mantém até o valor mudar. `X` escolhe o intervalo: `0` qualquer nota, `1` +3 st, `2` +6 st, `3` +12 st, `4` -3 st, `5` -6 st, `6` -12 st, `7` +/-3 st, `8` +/-6 st, `9` +/-12 st. As escolhas aleatórias seguem a escala do projeto quando o motor de escala se aplica à faixa. |
| `SPD` | signed `XX` | Seleciona uma razão de clock persistente por faixa; veja a tabela de velocidades abaixo. |
| `SLE` | `00-FF` ticks | Define tempo de glide persistente por faixa para FX contínuos do engine. `00` é imediato. Reseta para `00` quando a reprodução para. |

### Envio de Audio FX

| FX | Valor | Comportamento detalhado |
|---|---|---|
| `RSN` | `00-FF` | Define o envio de reverb desta faixa até o próximo gatilho de nota. |
| `DSN` | `00-FF` | Define o envio de delay desta faixa até o próximo gatilho de nota. |

### ADSR / Trigger FX

| FX | Valor | Comportamento detalhado |
|---|---|---|
| `EAT` / `EDC` / `ESU` / `ERL` / `ESH` | `00-FF` | Sobrescreve ataque, decay, sustain, release ou forma do envelope. |
| `TDC` / `TCL` | `00-FF` | Sobrescreve decay ou color do modo trigger em Plaits e Plaits-Alt. |

#### Condições

Inspirado nas "condições trig" suecas, o tracker suporta probabilidades de disparo e condições módulo.

| FX | Valor | Significado |
|---|---|---|
| `PRO` | `00-64` | Probabilidade absoluta de disparo de `0%` a `100%` |
| `MOD` | `AB` | Dispara na iteração `A` de `B`; por exemplo `12`, `22`, `14`, `34` |

Counters `MOD` são locais à faixa e frase. Combinações inválidas, como `A` maior que `B`, não disparam.

`MOD34` dispara na 3ª passada de um ciclo de 4 passadas. `MOD1F` dispara na 1ª passada de um ciclo de 16 passadas.

#### Velocidade de reprodução

Como o NerdSEQ, o ChooChooTracker suporta velocidade independente por faixa.

`SPD` lê seu valor como byte com sinal. `00` é velocidade normal, valores positivos aceleram a faixa, negativos a desaceleram. A configuração permanece ativa até que outro comando `SPD` altere.

| Valor | Velocidade |
|---|---:|
| `00` | `x1` |
| `01` | `x2` |
| `02` | `x3` |
| `7F` | `x128` |
| `FF` | `/2` |
| `FE` | `/3` |
| `80` | `/129` |

Projetos antigos sem suporte a sinal em `SPD` mantêm o mapeamento `00-10` original, tocando como salvo.

Multiplicadores muito altos podem exceder a resolução do scheduler de ticks do tracker e não foram testados em hardware.

### Scale & chords FX

#### Automação de escala

`SCL XY` altera a escala global de reprodução e a tônica sem mudar as notas armazenadas nas frases. `X` seleciona uma escala (`0` Cromo, `1` Maior, `2` Menor, até `C` Customizada); `Y` seleciona a tônica (`0` C até `B` Si). O comando aplica-se só a FX em frases, não em tabelas. Se várias faixas emitirem `SCL` no mesmo tick, a faixa de número mais baixo vence.

`SCL` pertence ao modo Quantizer. Com **Note Lock** ativo, o comando não aparece na lista de FX e qualquer `SCL` já escrito nas frases é ignorado enquanto o modo estiver ativo.

#### Acordes

`CRD XY` transforma a nota na sua linha de frase em acorde. `Y` seleciona: `0` Maior, `1` Menor, `2` Dim, `3` Aug, `4` Sus2, `5` Sus4, `6` Power, `7` Maj7, `8` Min7, `9` Dom7, `A` Min7b5, `B` Dim7, `C` Add9, `D` MinAdd9, `E` Maj9, ou `F` Min9. `X` seleciona uma das 16 voicings: `0-3` são ciclo de inversão fechada ascendente; `4-7` adicionam drop-2; `8-B` adicionam drop-3; `C-F` combinam ambos drops. O ciclo continua por uma oitava extra para tríades e power chords, tornando cada valor distinto; valores maiores são progressivamente mais amplos e menos convencionais. Por exemplo, `CRD 4Y` é drop-2 na posição raiz e `CRD DY` é primeira inversão com ambos drops. No topo ou base da tabela de pitch, as vozes são limitadas ao alcance. O comando não é persistente: escreva-o em cada linha com acorde. Cada nota gerada é quantizada independentemente quando a escala está ativa, tanto no modo Quantizer como no Note Lock. Disponível só para engines de software; instrumentos AY ignoram e permanecem monofônicos.

### Modulation FX

FX de frase e tabela podem alterar um slot de modulação sem editar o instrumento:

| Padrão FX | Significado |
|---|---|
| `M1A`, `M2A`, `M3A`, `M4A` | Deslocamento relativo da quantia para slots de modulação 1-4 |
| `M11`, `M12`, `M13`, `M14` | Deslocamentos relativos P1-P4 para slot de modulação 1 |
| `M21`, `M22`, `M23`, `M24` | Deslocamentos relativos P1-P4 para slot de modulação 2 |
| `M31`, `M32`, `M33`, `M34` | Deslocamentos relativos P1-P4 para slot de modulação 3 |
| `M41`, `M42`, `M43`, `M44` | Deslocamentos relativos P1-P4 para slot de modulação 4 |

O valor é interpretado como mudança relativa `8-bit` com sinal (`01` adiciona `1`, `FF` subtrai `1`). Comandos repetidos acumulam. Valores efetivos são limitados ao intervalo válido.

`P5` não é exposto como FX de frase ou tabela. Está disponível como destino de modulação `M1–M4 Wavetable`, para que um modulador escaneie o índice de AYWavetable de outro.

| Tipo de modulação | P1 | P2 | P3 | P4 | P5 quando Shape é Wavetable |
|---|---|---|---|---|---|
| ADSR | Ataque | Decay | Sustain | Release | Não usado |
| AHD | Ataque | Hold | Decay | Não usado | Não usado |
| LFO | Forma | Modo de trigger | Período | Não usado | Índice AY wavetable |
| syncLFO | Forma | Modo de trigger | Ticks | Multiplicador | Índice AY wavetable |
| slowLFO | Forma | Modo de trigger | Passo de período (00–FF) | - | Índice AY wavetable |
| FLFO | Forma | Modo de trigger | Frequência (`1 Hz` a `20 kHz`) | Não usado | Índice AY wavetable |

### Braids FX

| FX | Valor | Significado |
|---|---|---|
| `BMD` | `00-2E` | Modelo Braids |
| `BTM` | `00-FF` | Timbre absoluto normalizado |
| `BCL` | `00-FF` | Cor absoluta normalizada |
| `BCF` | `00-FF` | Cutoff exponencial, de `20 Hz` a `20 kHz` |
| `BRS` | `00-FF` | Ressonância do filtro exponencial |

### aChChid FX

| FX | Valor | Significado |
|---|---|---|
| `ASL` | `00-FF` | Slide até a nota sem retrigger. `00` é `60 ms`; valores maiores estendem o glide. |
| `ADC` | `00-FF` | Sobrescrita do decay: `200 ms` a `2 s`. |
| `AAC` | `00-FF` | Quantia de acento: nenhum a máximo. |
| `ATM` | `00-FF` | Sobrescrita do Timbre de Braids; ativa só no modo onda Braids. |
| `ACL` | `00-FF` | Sobrescrita da Cor de Braids; ativa só no modo onda Braids. |
| `ACF` | `00-FF` | Cutoff exponencial do filtro 303, `20 Hz` a `20 kHz`. |
| `ARS` | `00-FF` | Ressonância do filtro 303, nenhum a máximo. |
| `AEM` | `00-FF` | Modulação do envelope do filtro 303, nenhum a máximo. |

### Bogie FX

| FX | Valor | Significado |
|---|---|---|
| `DMD` | `00-0B` | Seleciona o modelo Bogie até o próximo gatilho. |
| `DDC`, `DTO`, `DSW`, `DNO`, `DFM`, `DDR` | `00-FF` | Sobrescreve Decay, Tone, Sweep, Noise, FM ou Drive até o próximo gatilho. Todos os seis macros disponíveis em qualquer modelo. |
| `DCF` | `00-FF` | Cutoff do filtro, mapeado logaritmicamente. |
| `DRS` | `00-FF` | Ressonância do filtro. |

### Sintered FX

| FX | Valor | Significado |
|---|---|---|
| `SMD` | `00-05` | Seleciona o modelo Sintered até o próximo gatilho. |
| `SDC`, `SMP`, `SMA`, `SMB`, `SMO`, `SMC` | `00-FF` | Sobrescreve Decay, Mod, os dois primeiros macros do modelo, Motion ou o último macro do modelo até o próximo gatilho. |
| `SCF`, `SRS` | `00-FF` | Cutoff e ressonância do filtro. |

### MIDI Out FX

| FX | Valor | Significado |
|---|---|---|
| `MC1`-`MC4` | `00-FF` | Envia um CC MIDI, reescalonado para `0-127`. Cada slot envia para um número CC configurado por instrumento (`MC1 number`-`MC4 number` na tela do instrumento), não pelo FX em si - slot sem número configurado é inerte. |

### Plaits FX

| FX | Valor | Significado |
|---|---|---|
| `PMD` | `00-17` | Engine Plaits |
| `PHA` | `00-FF` | Harmônicos absolutos normalizados |
| `PTM` | `00-FF` | Timbre absoluto normalizado |
| `PMO` | `00-FF` | Morph absoluto normalizado |
| `PAX` | `00-FF` | Blend Main/Aux: `00` Main, `FF` Aux |
| `PCF` | `00-FF` | Cutoff exponencial, `20 Hz` a `20 kHz` |
| `PRS` | `00-FF` | Ressonância do filtro exponencial |

### Sample FX

| FX | Valor | Significado |
|---|---|---|
| `SPL` | `00-03` | Modo de reprodução: `00` Forward, `01` Reverse, `02` Loop, `03` Ping-Pong |
| `SLI` | `00-FF` | Toca fatia numerada (`01` = primeira fatia) independentemente da nota; `00` mantém o mapeamento normal de notas. Requer modo slice ativado no instrumento |
| `SPT` | signed `XX` | Transposição da amostra em semitons |
| `SST` | `00-FF` | Início da reprodução normalizado |
| `STA` | `00-FF` | Alias de `SST` (início da reprodução normalizado) |
| `SEN` | `00-FF` | Fim da reprodução normalizado |
| `SVL` | `00-FF` | Volume absoluto da amostra |
| `SCF` | `00-FF` | Cutoff exponencial, `20 Hz` a `20 kHz` |
| `SRS` | `00-FF` | Ressonância do filtro exponencial |
| `SSP` | `00-FF` | Velocidade da amostra, mapeada de `0%` a `500%` |
| `RNS` | `M0` | Toca um slice aleatório de um sample fatiado em vez do atribuído à nota. `M` escolhe o modo: `0` sempre aleatório, `1` aleatório a cada PLAY, `2` escolhe e mantém até o valor mudar. Requer o modo slice ativado no instrumento; o nibble baixo não é usado. |

### AY FX compartilhados por instrumentos AY

| FX | Valor | Comportamento detalhado |
|---|---|---|
| `AYM` | `XY` | `X` é a forma do envelope; `Y` seleciona off/tone/noise/tone+noise (`0/1/2/3`). |
| `NOI` | signed `XX` | Deslocamento relativo acumulado do período de ruído. |
| `NOA` | `00-1F`, `FF` | Período de ruído absoluto; `FF` dá prioridade ao período de ruído das faixas anteriores. |
| `ERT` | qualquer | Retriga a forma de envelope hardware atual. |
| `EAU` | `XY` | Razão automática do envelope `X:Y`; `X=0` desativa. |

### AY Classic-only FX

| FX | Valor | Comportamento detalhado |
|---|---|---|
| `EVB` | `XY` | Vibrato no período do envelope: `X` velocidade, `Y` profundidade. |
| `EBN` | signed `XX` | Adiciona `XX` ao período do envelope a cada linha de frase/tabela. |
| `ESL` | `XX` ticks | Desliza do período do envelope anterior para o novo valor. |
| `ENT` | nota `XX` | Define o período do envelope segundo a nota da tabela de pitch mostrada pela UI. |
| `EPT` | signed `XX` | Deslocamento relativo acumulado do período do envelope. |
| `EPL` | byte `XX` | Define o byte baixo do período do envelope. |
| `EPH` | byte `XX` | Define o byte alto do período do envelope. |

### AY Plus FX| FX | Valor | Comportamento detalhado |
|---|---|---|
| `TNN` | nota `XX` | Define o oscilador de tom para uma nota da tabela de afinação, ignorando a nota da linha. |
| `TNP` / `TNF` | `XX` com sinal | Passo acumulado de afinação grossa / ajuste fino do tom. |
| `TRT` | qualquer | Reinicia a fase do oscilador de tom onde suportado. |
| `ENN` | nota `XX` | Define o oscilador de envelope para uma nota da tabela de afinação. |
| `ENP` / `ENF` | `XX` com sinal | Passo acumulado de afinação grossa / ajuste fino do envelope. |
| `SFT` | `00-07` | Tipo de software: None, Pulse, Sync Tone, Sync Env, Wavetable, Tone FM, Env FM, Sample. |
| `SFN` | nota `XX` | Define o oscilador de software para uma nota da tabela de afinação. |
| `SFP` / `SFF` | `XX` com sinal | Passo acumulado de afinação grossa / ajuste fino do oscilador de software. |
| `SRT` | qualquer | Reseta a fase do oscilador de software. |
| `SFM` | `XX` com sinal | Ajuste acumulado da profundidade FM. |
| `PWM` | `XX` com sinal | Ajuste acumulado da largura do pulso. |
| `SPL` | `XX` com sinal | Ajuste acumulado do nível baixo do pulso. |
| `SWT` | `XX` com sinal | Ajuste acumulado do índice da wavetable. |

### FX Sample AY

O Sample AY aceita os comandos AY compartilhados mais `TNN`, `TNP`, `TNF`, `TRT`, `SFN`, `SFP` e `SFF`, conforme descrito acima.

| FX | Valor | Comportamento detalhado |
|---|---|---|
| `SMS` | `00-FF` | Define o início da reprodução do Sample AY legado para `XX x 64` amostras da fonte. |

Os parâmetros FX do motor aplicam-se ao tipo correspondente de instrumento e são resetados no próximo gatilho de nota, a menos que indicado o contrário. `SPD` é intencionalmente persistente.

O seletor de FX filtra esta referência para comandos comuns mais o grupo suportado pelo instrumento ativo. O painel de ajuda contextual informa detalhes dos valores ao editar.

## 11. Mixer, Reverb e Delay

O Mixer está à esquerda na tela principal. Exibe carga da CPU e avisos de clipagem. Um `!` vermelho na coluna **CLIP** marca a faixa cujo sinal seco ultrapassou o limite seguro da mixagem.

Use-o para equilibrar as 8 faixas, moldar cada uma com Tilt EQ e enviar áudio para o reverb ou delay:

| Campo | Significado |
|---|---|
| LVL | Nível pós-motor da faixa, `000-100` |
| REV | Envio para o reverb compartilhado, `000-100` |
| DLY | Envio para o delay ping-pong compartilhado, `000-100` |
| PAN | Move o áudio para esquerda ou direita |
| TLT | Tilt EQ por faixa, `00-FF`; `80` é neutro, valores baixos favorecem graves e altos valores favorecem agudos. Valores baixos introduzem uma suave saturação. |
| M | `*` silencia esta faixa |
| S | `*` solo nesta faixa |

O mixer é baseado em faixas, não em instrumentos. Se uma faixa troca de instrumento, seu nível, PAN, Tilt e envios permanecem nela. Os instrumentos também têm seu próprio **Vol** e **Pan** `00-FF` na linha superior da tela do Instrumento. O PAN do instrumento é aplicado primeiro, depois o PAN da faixa. Ambos preservam uma imagem estéreo existente no centro; atenuam progressivamente o lado oposto ao mover para a esquerda ou direita.

### Auto Mix

O Mixer contém um botão **AUTO MIX**. Ele renderiza `6 segundos` offline, equilibra o volume das faixas, compara então 8 bandas de oitava com um perfil de ruído rosa e abaixa suavemente as faixas que saturam essas bandas. Propõe mudanças de nível prudentes, deixa alguma folga de pico e inicia uma prévia temporária. **Apply** mantém os níveis propostos, enquanto **Cancel** restaura os 8 níveis anteriores. É um bom ponto de partida, mas você provavelmente vai querer ajustar o resultado.

### Reverb

Use **SELECT + [UP]** no Mixer para abrir a tela de configurações do reverb.

- **Algorithm**: escolhe entre Clouds ou Beads. Cada algoritmo mantém seus próprios controles.
- **Return**: nível de reverb úmido na mixagem master.
- **Clouds**: **Time**, **Damping**, além de filtros pré-reverb **LP** e **HP**.
- **Beads**: **Time**, filtro nativo de feedback **Tone** e filtro pré-reverb **HP**.

Esta seção é só o envio de reverb das nuvens Clouds ou Beads, não o processador granular.

### Delay Ping-pong

Use **SELECT + [DOWN]** no Mixer para abrir o Delay, depois **SELECT + [UP]** para voltar. O reverb funciona ao contrário: **SELECT + [UP]** abre, **SELECT + [DOWN]** retorna ao Mixer.

- **Return**: nível de delay úmido.
- **To Reverb**: quantidade do sinal úmido do delay enviado ao reverb compartilhado, `0-100%`.
- **Ticks**: tempo de delay em ticks do tracker (`9 ticks` = `1 batida` por padrão).
- **Feedback**: quantidade de feedback cruzado, limitada a `95%`.
- **Filter**: filtro passa-baixa no caminho do sinal atrasado.

A primeira repetição segue a entrada estéreo. Repetições posteriores cruzam o feedback entre os canais esquerdo e direito.

## 12. Tela do projeto

A tela do projeto oferece os comandos **Load**, **Save**, **New**, **Export**, **Manage** e **Scale**, junto com os metadados de nome de arquivo, título e autor.

### Local de exportação

Exportações, stems e bounces são gravados em uma pasta por projeto dentro do diretório de samples pré-existente do app:

- **Padrão**: `<pasta do app>/samples/Exports/<nome do projeto>/`. Enquanto o projeto estiver sem nome, a pasta é `current-project`.
- Na versão web, o padrão é `/user/samples/Exports/<nome do projeto>/`.
- No Android, a pasta de samples fica dentro da workspace do app: privada no build Play Store, `samples` na pasta escolhida no build APK direto.

A linha **Folder** na tela EXPORT mostra o destino ativo:

- Toque em **EDIT** para escolher uma pasta personalizada com o navegador de pastas. A pasta escolhida é usada exatamente como selecionada (nenhuma subpasta de projeto é adicionada) e lembrada nas configurações.
- **EDIT + OPT** (limpar) reseta para o local padrão.

Quando um projeto nomeado é salvo com novo nome, a pasta padrão de exportação é renomeada para combinar, mantendo os arquivos exportados previamente com o projeto. Renomear não se aplica a uma pasta personalizada, e a pasta não é renomeada por autosave. Se uma pasta com o nome alvo já existir, ambas são mantidas. Se o tracker perder o caminho da pasta (ex: após reinício), salvar um projeto nomeado renomeia a pasta `current-project` existente. Os instrumentos continuam funcionando após o rename: caminhos de samples carregados que apontam para a pasta renomeada são atualizados automaticamente.

**Load** aceita arquivos `.mid`/`.midi`, importados como novo projeto: notas são agrupadas por canal MIDI (um canal por faixa, até o limite de faixas), quantizadas em 4 linhas por batida, e colocadas num único instrumento AY padrão — números de programa MIDI não têm equivalente chiptune, então escolha instrumentos reais depois. Só o primeiro tempo do arquivo é usado (única taxa global de ticks, sem mudanças de tempo por seção). A linha **MIDI** da tela Export faz o inverso: grava as notas atuais, volume e tempo/groove como um Standard MIDI File (uma faixa MIDI por faixa do tracker); instrumentos [MIDI Out](#midi-out) e outros FX por linha além de volume e groove global não têm equivalente MIDI e não são traduzidos.

**Load** aceita músicas Dirtywave M8 (`.m8s`, firmware 2.x-4.x), importadas como novo projeto. Só a estrutura e notas são importadas (linhas da música, chains com transposição, frases com notas, velocidade e número do instrumento, note-offs, tempo e nome da música); as notas mantêm o tom real (o M8 nomeia oitavas duas acima: seu `C-6` é `C-4` aqui). Cada instrumento usado por uma frase é importado com o nome original do instrumento M8. Instrumentos **MACROSYN** viram Braids (as 47 formas são os modelos Braids, mesmos números; timbre, cor, tipo de filtro, cutoff, ressonância e pan são transferidos, com a curva de cutoff aproximada). Instrumentos **SAMPLER** viram instrumentos Sample: o `.m8s` só armazena caminho do WAV no cartão M8, então o arquivo é procurado na pasta do `.m8s` e suas ancestrais (uma cópia da estrutura do cartão funciona) e na pasta atual de samples (`Samples` ou `samples`, ambos funcionam); se não encontrado, o instrumento mantém o caminho do M8 e fica sem som. Os samples importados recebem +24 semitons na afinação, pois o M8 toca o sample original duas oitavas abaixo do padrão deste app. **FMSYNTH** vira instrumento FM Genesis (aproximação), **WAVSYNTH** e **HYPERSYNTH** viram aChChid. **MIDI OUT** vira instrumento MIDI Out (canal, bank, programa e os primeiros quatro CC). Outros tipos (externos) viram instrumento AY padrão — escolha sons reais depois. FX e tabelas são ignorados.

**Load** aceita músicas Little Sound Dj (`.lsdsng`, uma música por arquivo), importadas como novo projeto. Escolher um arquivo de save completo do LSDj (`.sav`, até 32 músicas) abre lista das músicas (slot, nome, versão, tamanho); escolha uma para importar (a música ativa quando o cartucho foi salvo não é listada). Só a estrutura e notas são importadas (linhas da música, chains com transposição, frases com notas e número do instrumento, tempo e nome da música); todas as notas têm volume máximo, pois LSDj não tem volume de nota, e os quatro canais PU1, PU2, WAV e NOI do LSDj viram faixas 1-4. Instrumentos pulse viram GB Pulse (duty, envelope e pan transferidos; sweep não), instrumentos noise viram GB Noise (envelope, estabilidade e pan), instrumentos wave viram o preset GB Pulse Hollow Lead e kits viram GB Noise padrão. Grooves são importados, e comandos de frase `C` (chord), `D` (delay), `E` (pulse e noise envelope, não no canal wave), `G` (groove), `K` (kill) e `V` (vibrato) viram `ARP`, `DEL`, `CEI`/`CEP`/`CED`, `GGR`, `KIL` e `PVB`. Outros comandos (`A`, `F`, `H`, `L`, `M`, `O`, `P`, `R`, `S`, `T`, `W`, `Z`), tabelas, frames de wave, samples de kit e ajustes de synth são ignorados, então músicas que dependam deles soam diferente. O tempo é convertido para groove de 6 ticks.

No desktop e Android, a linha **LSDJ** da tela Export faz o inverso, de forma limitada: toque nela e escolha um `.lsdsng` existente como **template** (lembrado até fechar, **EDIT + OPT** esquece). As linhas, chains, frases, tempo e título são substituídos, e o resto (tabelas, grooves, waves, kits, ajustes de synth) é mantido. Faixas 1-4 viram PU1, PU2, WAV e NOI; instrumentos GB Pulse e GB Noise são escritos como instrumentes pulse e noise do LSDj, outros instrumentos como pulse padrão. FX, volume de notas e note-offs não são exportados. Músicas com chain `80` ou acima, ou frase `FF` ou acima não podem ser exportadas (LSDj tem 128 chains e 255 frases). O arquivo exportado ainda não foi testado no LSDj.

No desktop, a linha **M8S** da tela Export faz o inverso, de forma limitada: toque nela e escolha um `.m8s` existente como **template** (lembrado até fechar, **EDIT + OPT** esquece). Música, chains, frases, tempo e título são substituídos, o resto é mantido, exceto que instrumentos Braids são escritos como MacroSynth e Sample como Sampler (caminho `/Samples/<nome do arquivo>`: copie o WAV para lá no M8 você mesmo), desde que o template tenha instrumento daquele tipo para copiar os parâmetros. FX não são exportados. Músicas com frase `FF` ou acima não podem ser exportadas (M8 tem 255 frases). O arquivo exportado não foi testado em um M8 real. Veja [M8 songs](m8s-format.md) para detalhes.

No desktop, Key jazz permite digitar nome de arquivo, título e autor diretamente no teclado, sem usar teclado virtual na tela — veja [Key jazz](#key-jazz-desktop-only).

### Scale / Quantize

A tela Scale controla o motor global de escala 12-TET. A linha **Mode** seleciona entre dois modos:- **Quantizador** (padrão): a entrada da frase permanece cromática; quando ativado, as notas são arredondadas para baixo imediatamente antes de chegar ao motor de som, permitindo que mudar a raiz ou a escala reharmonize a música sem editar suas frases.
- **Bloqueio de Nota**: as notas digitadas nas frases são ajustadas à nota mais próxima da escala enquanto são inseridas, permitindo escrever apenas notas da escala. Inserções novas e reduções ajustam para baixo; `EDIT + RIGHT` / `EDIT + UP` elevam a nota para a próxima nota da escala acima. A entrada segue sempre a escala do projeto nesta tela: mudar a raiz ou a escala afeta imediatamente as notas novas nas faixas ativadas, enquanto notas já armazenadas permanecem intactas. O FX `SCL` não está disponível nesse modo e qualquer `SCL` já escrito é ignorado. A quantização de reprodução de notas simples é ignorada (a entrada já está bloqueada); a quantização de acordes via `CRD` continua ativa.

- **Clock source** escolhe entre **Internal** (o tempo do próprio projeto), **Midi Clock** (o sequenciador segue um clock MIDI recebido) e **Ableton Link** (o sequenciador segue o tempo de uma sessão Link na rede local). Com **Midi Clock**, a linha **Tempo** aparece esmaecida e mostra o tempo medido do clock recebido; o transporte (play/stop) é controlado pelas mensagens Start/Continue/Stop do emissor, então o controle PLAY do app não é necessário. Com **Ableton Link**, a linha vira a linha **Link**: contém o botão de conectar/desconectar da sessão no lugar de um campo de tempo (veja abaixo). A fonte de clock é salva em settings.txt e vale para todos os projetos.
- **Tempo** define o tempo do projeto em BPM inteiros (`3-500`). Internamente o tracker ainda guarda sua tick rate em Hz (`tick rate = BPM x 0,4`), então projetos existentes carregam e salvam sem mudança; um projeto com tempo fracionário o mostra arredondado e ajusta para um BPM inteiro na primeira edição do tempo. O ajuste grosso avança de 5 em 5 BPM, o fino de 1 em 1; apagar restaura o padrão de 125 BPM.

ChooChooTracker salva projetos como `.cct`. Este é um formato proprietário, fácil de ler até mesmo para humanos.

Use **Save** antes de mudar tipos de instrumento ou carregar outro projeto.

### Ableton Link

O Ableton Link permite que o ChooChooTracker siga o tempo de outros apps compatíveis com Link (Ableton Live, LinkHut etc.) na mesma rede local (Wi-Fi ou Ethernet). É apenas receptor: o tracker ajusta seu próprio tempo à sessão e nunca envia tempo nem transporte a outros participantes.

**O que é sincronizado:**

- Tempo da sessão - o tracker segue continuamente o tempo da sessão Link (dentro do intervalo de 3-500 BPM do tracker).
- Fase do beat - pressionar PLAY alinha a reprodução à grade de beats da sessão: a música começa no próximo compasso (quantum de 4 beats).

**O que não é sincronizado:**

Durante a reprodução com Link, o ChooChooTracker verifica a fase de beat compartilhada uma vez por compasso 4/4 e ajusta suavemente sua própria tick rate quando necessário. Isso compensa pequenas diferenças de clock entre dispositivos sem pular nem repetir ticks do tracker, e nunca altera a sessão Link de outro participante.

- Grooves e valores `SPD` e `DELAY` não são afetados - continuam relativos ao tempo seguido.
- As posições do cursor em song/chain/phrase e as configurações do mixer nunca são compartilhadas.

**Como usar:**

1. Abra a tela Project e defina **Clock source** como **Ableton Link**. A linha abaixo vira a linha **Link**.
2. Pressione EDIT em **[Connect]** para entrar na sessão. A mensagem de status à direita mostra o progresso: `Wait...` ao conectar, `No net` se não há rede, `No sess` se nenhuma sessão é encontrada, `Err` em caso de falha interna. Enquanto estiver conectado, o botão mostra **[Disconnect]** e o status alterna aproximadamente a cada segundo entre `connected` e `Peers: N` (N = número de peers).
3. Aguarde `connected` e pressione PLAY - a reprodução começa alinhada ao próximo compasso da sessão.
4. Pressione EDIT em **[Disconnect]** (ou volte a fonte de clock para Internal) para sair da sessão.

No modo Link a sessão é dona do tempo: a linha não mostra campo de BPM e o tempo da sessão é aplicado automaticamente ao projeto (dentro do intervalo de 3-500 BPM do tracker).

**Disponibilidade por plataforma:** PortMaster (alvo principal), Windows, Linux, macOS e Android são suportados. A versão Web e as do Miyoo não incluem Link.

**Nota PortMaster:** em dispositivos PortMaster o buffer de áudio é maior (512 frames a 48 kHz), então o alinhamento do beat é aproximado, dentro de cerca de 10-15 ms, e não exato ao sample. A sincronização do tempo em si não é afetada.

## 13. Configurações

Use Configurações para preferências que ficam com o app, e não com o projeto atual. Controles gerais do projeto como tempo, groove e níveis do mixer permanecem em suas próprias telas.

### Controles

- **Repeat delay** e **Repeat speed** definem quão rápido e com que atraso um botão de navegação ou edição pressionado repete. Aumente o atraso se a repetição for descontrolada; diminua para entrada rápida como tracker.
- **Stick live mode** escolhe o funcionamento do botão mapeado **STICK LIVE**: `HOLD` fica ativo enquanto segurado, `TOGGLE` mantém ativo após pressionar, e `FREE` está sempre ativo. Veja [Gravação de movimento](#motion-recording).
- **Key mapping** atribui controles do teclado ou controlador. Escolha um comando, pressione o novo controle, depois selecione **Done** para salvar o mapeamento.

### Áudio

**Sample rate** oferece `32 kHz` ou `48 kHz` (padrão). As mudanças se aplicam imediatamente e são salvas. Mudar a taxa de amostragem interrompe a reprodução e a prévia de samples enquanto os motores de som e qualquer entrada de áudio armada são reabertos na nova taxa. Se um dispositivo recusar a nova taxa, a configuração anterior continua ativa. `32 kHz` reduz o trabalho de processamento e limita a frequência reproduzida mais alta a 16 kHz. O indicador de uso de CPU mostra o custo da renderização de áudio.

**Buffer size** troca responsividade por estabilidade. Use valor menor para performance ao vivo mais apertada; aumente se o áudio estalar ou o medidor de CPU ficar sempre alto. Valores disponíveis: `32`, `64`, `128`, `256`, `512` e `1024` amostras. O indicador de uso de CPU ajuda na escolha de uma configuração estável.

### MIDI

Escolha dispositivos de entrada e saída, atribua canais MIDI de entrada para instrumentos e faça mapeamentos de controles MIDI CC aqui. Veja [MIDI](#15-midi) para conectar um teclado, controlar o mixer ou enviar notas para hardware externo.

### Sintetizadores

- **AY Quality** define o nível de qualidade usado pelas vozes AY/YM. **AY Sample dithering** pode suavizar a reprodução das amostras AY; deixe desativado para som mais direto e lo-fi, ou quando for tocar em hardware original.
- **Braids BITS**, **DRFT** e **SIGN** são configurações globais do Braids, compartilhadas por todos os instrumentos Braids. Use para ajustar resolução, drift e assinatura geral antes de afinar sons individuais. Veja [Braids](#braids).

### Mixer

- **Mix volume** é o nível mestre de saída do aplicativo. Reduza ao alimentar outro mixer ou gravador; isso não altera a mistura das faixas da música.
- **Tilt pivot** define a frequência central para todos os controles de **Tilt** no Mixer deste projeto, de `250 Hz` a `4000 Hz` (padrão `1 kHz`).

### Gráficos

- **Edit color theme** altera as cores da interface. **Load font** carrega uma fonte `.cnfont`.
- **Song waveform** mostra ou oculta a forma de onda da mixagem na página Song; veja [Song waveform](#song-waveform).
- **Track visuals** escolhe por faixa entre **Detailed** (forma de onda do instrumento e sobreposições) ou **Audio waveform** (áudio resultante da faixa). Use **All detailed** ou **All audio** para mudar todas as faixas de uma vez.
- **Waveform FPS** define a frequência de redesenho do instrumento e visualizadores do mixer, de `1` a `60` quadros por segundo. Diminua para exibição mais silenciosa ou para economizar recursos.
- **Renderer** mostra se o aplicativo usa renderização `GPU` ou `Software`.

### Relatório de suporte e sair

**Support report** gera um arquivo de diagnóstico para reportar bugs. Anexe junto com o projeto `.cct` quando o problema depender de uma determinada música. **Quit ChooChooTracker** fecha o aplicativo de forma segura.

### Song waveform

Gráficos → Song waveform está DESLIGADO por padrão. Toque EDIT para alternar ou use EDIT + direção; a escolha é salva como preferência do app. Quando ativado, uma forma de onda de duas linhas aparece acima da página SONG, mostrando a mixagem final somada em mono. As outras páginas mantêm o layout de altura total, espaçamento normal e visualização dos instrumentos independentemente dessa opção. A preferência salva é compatível com versões antigas. Os alvos de toque em SONG seguem as linhas exibidas em ambos os modos.

## 14. Solução de problemas

### CPU

O uso da CPU depende dos motores e efeitos ativos. Modelos físicos Plaits e Clouds Reverb são mais pesados que vozes AY básicas. Verifique o desempenho no console alvo, especialmente com 8 vozes Plaits e ambos os envios ativos. Uma leitura próxima a `100%` pode causar estalos ou falhas na reprodução.

Se ocorrerem pops, travamentos ou lentidão, use **Settings > Support report** e anexe o arquivo `support-report.txt` ao report de erro. Se o problema for acionado por uma música específica, anexe também o `.cct` correspondente.

### Sem som

Verifique o número do instrumento, estado de mute ou solo da faixa, nível da faixa, Mix volume do aplicativo e envelope do instrumento. Para samples, confirme se o WAV original ainda existe no caminho salvo.

No ArkOS, pressione **MENU + L3** para alternar o mute do sistema. Se não resolver, reinicie o hardware e reconecte sua interface de áudio. Use a opção "reparar áudio" do ArkOS, pode ajudar.

### Entrada errada

Ajuste Repeat delay e Repeat speed em Configurações. Se um único toque causar movimentos duplicados, confirme se apenas 1 controle físico está mapeado para aquela direção e reporte a tela e atalho usados.

## 15. MIDI

Settings > **MIDI** abre este submenu: **MIDI In** / **MIDI Out** escolhem dispositivos conectados, alternando pelas portas detectadas com `OFF` nas extremidades; ambos os sentidos precisam de dispositivo selecionado.

- **Prévia de som a partir de teclado MIDI**: com um dispositivo MIDI In selecionado, tocar notas no teclado conectado audiciona um instrumento em qualquer tela, igual ao atalho na tela [Instrument](#5-instruments) **EDIT + PLAY**. É só prévia — não insere nota na música.
- **Mapeamento de canais**: abre a lista dos 16 canais MIDI; atribua um instrumento para cada (`OFF` por padrão) para que notas recebidas naquele canal executem o instrumento correspondente, independente do selecionado na tela Instrument — ex. canal `01` -> instrumento `05`. Canal `OFF` volta ao comportamento original, usando instrumento selecionado atualmente. Este mapeamento é salvo em settings.txt.
- **Mapeamento de MIDI CC**: mapeia um controlador MIDI para um parâmetro de instrumento, incluindo seu **Volume**, ou para controles globais. As linhas globais mostram `TRK 1`-`TRK 8` no lugar do instrumento e fornecem Mute, Solo, Volume, envio Reverb e envio Delay da faixa; **Song play/stop** não tem alvo. Um mapeamento novo ignora o CC que o aprendeu/configurou e só entra em efeito no próximo movimento físico, logo mapear durante reprodução não reseta patches a zero. Mute e solo usam `0-63` = desligado, `64-127` = ligado.
- **Sincronização por clock MIDI**: com um dispositivo MIDI In selecionado, defina **Clock source** na tela Project como **Midi Clock** para seguir um sequenciador externo. O tracker mede o tempo do clock recebido (24 pulsos por semínima) e o mostra na linha **Tempo** esmaecida; as mensagens **Start**, **Continue** e **Stop** controlam a reprodução. A fonte de clock é lembrada em settings.txt.
- **Controlando um dispositivo MIDI externo**: dê a uma faixa o tipo de instrumento [MIDI Out](#midi-out) e defina seu Canal; tocar notas nessa faixa envia MIDI real para o dispositivo MIDI Out selecionado ao invés de gerar som no ChooChooTracker.

## 16. Créditos e licenciamento

ChooChooTracker é um fork do ChipNomad e mantém sua licença MIT. Braids, Plaits, Plaits-Alt, Clouds DSP, MME DSP derivado do Warps e código stmlib são derivados das versões open-source da Mutable Instruments sob suas respectivas licenças MIT. Plaits-Alt vem do fork lylepmills/eurorack Plaits Lab; seus avisos de fonte mantidos valem. O motor aChChid usa Open303 de Robin Schmidt, 2009, sob licença MIT. O MonkSynth usa o DSP nativo de JonET sob licença MIT; veja `LICENSE.monksynth.txt`. Bogie e Sintered são implementações originais nativas. I/O MIDI usa RtMidi por Gary P. Scavone, sob licença estilo MIT. Veja os arquivos de licença embalados para atribuição exata. Ableton Link é fornecido com acordo especial da Ableton, não sob licença MIT, e nossa licença não se estende a seus forks ou a outros projetos que não o ChooChooTracker.