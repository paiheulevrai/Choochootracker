# Manual de Usuario de ChooChooTracker
> Esta es una traducción generada automáticamente de `docs/USER_MANUAL.md` del manual actual en inglés. Si difiere del inglés, prevalece el texto en inglés.
>
> **Convenciones.** La interfaz de la aplicación está en inglés. Mantenga en inglés los nombres de pantallas, menús, campos y botones, códigos FX, nombres de módulos y motores, y extensiones de archivo.


ChooChooTracker es un tracker de música de 8 pistas para consolas portátiles.

Sigue el flujo de trabajo y sistema de navegación clásico de LSDj, con varias opciones de diseño sonoro:

- Emulación de chips y consolas vintage
- Soundfonts
- Osciladores Mutable Instruments Braids, Plaits y Plaits-Alt, con filtro multimodo
- Muestras PCM, formas de onda de ciclo único y osciladores wavetable, con filtro multimodo
- aChChid, un sintesizador acid versátil basado en Open303
- Bogie, un sintetizador de batería nativo de 12 modelos con filtro multimodo
- MonkSynth, un sintetizador vocal con unísono de diez voces y delay estéreo
- MME, un sintetizador multi-modulación de dos osciladores con filtro multimodo
- Sintered, un sintetizador experimental digital nativo de batería con seis modelos

## 1. Instalación y archivos

### ArkOS / PortMaster

Instale `choochootracker.zip` mediante PortMaster, o copie el paquete extraído en el directorio `ports` de la consola. Inicie **ChooChooTracker** desde el menú de Ports.

### Miyoo Mini / Mini Plus (Onion OS)

Extraiga el archivo MiyooPorts en la raíz de la tarjeta SD para que sus carpetas `Roms/PORTS/` se mezclen con las existentes de Onion. Inicie **ChooChooTracker** desde el menú de Ports después de actualizar la colección. Haga copia de seguridad de la carpeta `projects/` antes de reemplazar una instalación existente.

El tiempo de carga de un proyecto puede variar según los tipos de instrumento usados en la canción. Si el arranque o la carga de un proyecto parece anormalmente lento, indica cuánto tarda cada uno, junto con el modelo de consola y la versión de Onion OS.

### Raspberry Pi OS 64 bits

Extrae el archivo Raspberry Pi AArch64, instala el runtime de SDL2 (`sudo apt install libsdl2-2.0-0`) y ejecuta `./chipnomad.sh` desde el directorio extraído. Mantén los instrumentos empaquetados y demás recursos junto al ejecutable.

### Windows

Ejecute `choochootracker.exe` con `SDL2.dll` y `libwinpthread-1.dll` en el mismo directorio.

La entrada en Windows soporta teclado (ver sección de modo Jazz para detalles) o Gamepad.

### Web

Puede usar Choochootracker en línea en https://choochootracker.vercel.app/ .

Se puede controlar esta versión con teclado, controles en pantalla en dispositivos móviles o con su gamepad.

Los ajustes guardados, muestras, temas, proyectos, etc. se almacenan en el navegador.

Nota: debido a restricciones de iOS, puede ser imposible usar midi con la versión web en iPhone.

### Configuración de directorios

| Carpeta | Contenido |
|---|---|
| `AY_Wavetables` | Wavetables `.aywave` para chip AY |
| `fonts` | Archivos de fuente `.cnfont` para ChipNomad |
| `instruments` | Presets de instrumento `.cni` y también bancos FM y Soundfonts |
| `pitch-tables` | Afinaciones alternativas `.csv` |
| `projects` | Archivos de canción y proyecto `.cct` |
| `samples` | Archivos `.wav` para el motor Sampler |
| `SR_wavetables` | Wavetables en formato Serum para el motor BYOWTBL |
| `themes` | Temas ChipNomad `.cth` |
| `title` | Recursos de la pantalla de título |
| `waveforms` | Formas de onda de ciclo único `.wav` para el motor 2xSCWF |

## 2. Controles

ChooChooTracker funciona mejor con un gamepad que tenga D-pad, 2 sticks analógicos y 7 botones.

| Control lógico | Predeterminado en portátil | Predeterminado en Windows |
|---|---|---|
| **[IZQUIERDA/DERECHA/ARRIBA/ABAJO]** | D-pad | Flechas |
| **EDIT** | A | X |
| **OPT** | B | Z |
| **PLAY** | Start | Espacio |
| **SELECT** | Select | Shift |
| Ejes de modulación | Sticks izquierdo y derecho | Sticks del gamepad |
| **STICK LIVE** | L1 | Q |
| **MOTION RECORD** | L2 | W |
| **MOTION ERASE** | R2 | E |

Las asignaciones se pueden cambiar en **Settings > Controls > Key mapping**, también en Android con un gamepad externo. En PortMaster, guarde la reasignación con **Done**, luego suelte el botón antes de continuar en Settings.

### Navegación

- Use **[ARRIBA/ABAJO/IZQUIERDA/DERECHA]** para mover el cursor.
- Mantenga **SELECT + [DIRECCIÓN]** para cambiar de pantalla.
- Mantenga **OPT + [DIRECCIÓN]** para navegación específica de la pantalla.
- Sobre un valor, use **EDIT + [IZQUIERDA/DERECHA]** para cambios finos o **EDIT + [ARRIBA/ABAJO]** para cambios gruesos.
- Pulse **EDIT** para entrar un valor o activar un comando.

### Selección y portapapeles

- **SELECT + OPT** entra en modo selección y cicla por rangos útiles.
- Use **[DIRECCIÓN]** para extender el rango.
- Pulse y suelte **OPT** para copiar la selección y salir del modo selección.
- **OPT + EDIT** la corta.
- Fuera de modo selección, mantenga **SELECT** y presione **EDIT** para pegar.
- **EDIT + [DIRECCIÓN]** edita las celdas seleccionadas juntas.

### Reproducción

- **PLAY** empieza desde el cursor. Púlselo otra vez para parar.
- Puede editar notas, instrumentos y chains mientras se reproduce; los cambios se oyen al siguiente tick del secuenciador.
- **SELECT + PLAY** inicia todas las pistas cuando se usa fuera de la pantalla Song.
- **EDIT + PLAY** previsualiza un instrumento desde el Instrument Pool.

### Bounce global

Seleccione un rango de filas y columnas de Song, Chain o Phrase, luego doble-pulse **EDIT** para abrir la pantalla BOUNCE TO SAMPLE. El bounce incluye todas las pistas seleccionadas: pistas cuya primera chain comienza más tarde dentro de la selección esperan en silencio y se unen en su primera chain, por lo que el archivo contiene todas las chains de las pistas seleccionadas.

La pantalla ofrece:

- **File name**: edite con el teclado de caracteres. Al abrir la pantalla, se propone el siguiente número de secuencia libre (`001`, `002`, ...) aquí: el primer número cuyo archivo `.wav` no existe aún en la carpeta de exportación. Edítelo libremente — el nombre se usa tal cual. Si ya existe un archivo con ese nombre, se añade un sufijo `_001`, `_002`, ... para no sobrescribir. El contador se reinicia al cargar o crear un proyecto.
- **Sample rate** y **Bit depth**: mismas opciones que en la pantalla de exportación
- **Include in a sample name**: tres casillas que añaden etiquetas al nombre al pulsar **Start** (todas apagadas por defecto):
  - **[BPM]** — tempo actual del proyecto, ej. `[120]`
  - **[Key]** — nota raíz y escala del proyecto, ej. `[Cmaj]`, `[F#min Pent]`
  - **[Bars:Beats:16ths]** — duración del bounce: una frase equivale a un compás (16 dieciseisavos), un beat son 4 dieciseisavos. Frases completas muestran sólo compases, ej. `[2]`; frases parciales muestran `[0:2]` (dos beats) o `[0:1:3]` (un beat más tres dieciseisavos). En bounce multi-pista decide la pista más larga. Una selección que termina a mitad de un beat cuenta un dieciseisavo menos: la última fila seleccionada es el punto de corte, por lo que su nota-off cae al final del dieciseisavo anterior.
  - Las etiquetas se aplican en orden `[BPM][Key][Length]` antes del nombre.
- **Start**: renderiza la selección a un archivo WAV en la carpeta de exportación (ver *Export location* en pantalla Project)
- **Cancel**: vuelve sin renderizar

Mientras se renderiza, **OPT** lo cancela. Al completar o cancelar, vuelve a la pantalla previa.

## 3. Estructura de la canción y mapa de pantallas

Mantenga **SELECT + [DIRECCIÓN]** para cambiar de pantalla.

La fila principal de navegación es:

<pre>
        I
R P   G M
<strong>M S C P I T</strong>
D S     P W
</pre>

Fila central:

- **M**: Mixer (despliega Reverb, Delay)
- **S**: Song (despliega Project, Settings)
- **C**: Chain
- **P**: Phrase (despliega Groove)
- **I**: Instrument (despliega Modulation e Instrument Pool)
- **T**: Table (editor AY Wavetable)

Fila superior:

- **R**: Reverb
- **P**: Project
- **G**: Groove
- **M**: Modulation
- **I**: Insert FX

Fila inferior:

- **D**: Delay
- **S**: Settings
- **P**: Instrument Pool
- **W**: Editor AY Wavetable

Una canción en ChooChooTracker sigue esta jerarquía:

```text
Song -> Chain -> Phrase row -> Nota + Instrumento + FX
```

Una canción tiene 8 pistas. Cada pista contiene una secuencia de chains, y cada chain una secuencia de phrases. Las phrases son secuencias de notas de 16 pasos. Una nota contiene tono, instrumento, velocidad y hasta 3 FX de pista. Un instrumento es el generador de sonido.



## 4. Song, Chain y Phrase

### Song

La pantalla Song contiene 8 columnas, 1 por pista de audio. Cada celda refiere a una chain. Una canción puede tener hasta 256 filas (`00-FF`).

Las pistas van de izquierda a derecha, y sus chains verticalmente. Cada pista es independiente, y las chains pueden contener de 1 a 16 phrases, por lo que las pistas pueden desincronizarse. Diseñe sus chains para mantenerse juntos, o déjelos fluctuar si la pista lo requiere.

Es común reservar una chain para phrases vacías, normalmente `00` o `FE`.

Puede mantener varias subcanciones en un proyecto creando “islas”: secciones separadas por filas vacías.

#### Modo Live

Doble toque **OPT** para entrar o salir de Live Mode; `LIVE` reemplaza el título Song. En Live Mode, **PLAY** arranca la chain seleccionada en su pista si está parada. En una pista en reproducción, pone en cola la chain seleccionada para el próximo límite de chain; pulsar **PLAY** dos veces cambia inmediatamente en la siguiente phrase. Una celda vacía de Song seleccionada pone en cola una parada cuantizada. `+` marca chain en cola, `!` una chain urgente y `-` parada en cola.

Seleccione una fila de Song en varias columnas para poner en cola o detener esas pistas juntas. Se rechazan selecciones múltiple por filas intencionadamente. Las colas Live se borran durante el loop de un rango Song activo. Live es un modo de performance y no se guarda en el proyecto.

#### Controles

- **OPT + [ARRIBA/ABAJO]**: saltar 16 posiciones arriba o abajo
- Seleccione un rango y use **SHIFT + EDIT**: clona cadenas superficialmente manteniendo sus phrases originales
- Seleccione un rango y doble toque **SHIFT + EDIT**: clonación profunda de chains y phrases
- Seleccione un rango y use **EDIT + [ARRIBA/ABAJO]**: mover selección arriba o abajo
- Pulse **OPT** 3 veces: añade o quita un resaltado para organización visual
- **OPT + SHIFT**: mutea la pista actual o columnas seleccionadas (libere **OPT** primero para mantener el mute)
- **OPT + PLAY**: solo la pista actual o columnas seleccionadas (libere **OPT** para mantener el solo)
- **OPT + [IZQUIERDA/DERECHA]**: solo todas las pistas a la izquierda o derecha de la actual
- Seleccione un rango y doble toque **EDIT**: abre BOUNCE TO SAMPLE (ver Bounce arriba)

Detener la reproducción y cambiar mute o solo usan una breve corrección de salida de 3 ms para reducir los clics. Esto incluye también reactivar el sonido y quitar los solos durante la reproducción.

### Chain

Una chain es una lista ordenada de phrases de 16 pasos con transposición opcional. Puede contener hasta 16 phrases, y la misma phrase puede repetirse. La 2ª columna pone la transposición en semitonos.

Un asterisco (`*`) indica una chain reutilizada en el proyecto. Puede clonar chains desde la pantalla Song.

#### Controles

- **OPT + [IZQUIERDA/DERECHA]**: cambia entre pistas
- **OPT + [ARRIBA/ABAJO]**: cambia entre chains en la pista actual
- Seleccione un rango y use **SHIFT + EDIT**: clona phrases
- Seleccione un rango y doble toque **EDIT**: abre BOUNCE TO SAMPLE para las filas de chain seleccionadas de la pista actual (ver Bounce en Song)

### Phrase

Una phrase es el patrón de pista en un secuenciador por pasos tradicional.

Una phrase tiene 16 pasos, 1 fila por paso. Cada fila contiene nota, instrumento, volumen de `00` a `7F`, y columnas FX. Las notas usan notación tracker, ej. `C-4` (nota y octava).

Para silenciar una nota, inserte `NOTE OFF`, use el FX de kill-note o toque otra nota sin instrumento asignado.

Cuando una chain contiene varias phrases, la pantalla Phrase actúa como un editor de patrones continuo. Mover más allá de la fila `F` o arriba de la fila `0` abre la phrase siguiente o anterior en la chain.

Un asterisco (`*`) indica una phrase reutilizada. Puede clonar phrases desde la pantalla Chain.

El selector FX muestra comandos comunes más aquellos soportados por el instrumento de esa fila. Esto evita una larga lista global de comandos de motores específicos. ChooChooTracker soporta más FX que ChipNomad.

### Controles- **OPT + EDIT** en una nota vacía: inserta `NOTE OFF`
- **EDIT + [ARRIBA/ABAJO]** en una columna de nombre FX: abre la pantalla de selección de FX
- **OPT + [IZQUIERDA/DERECHA]**: moverse entre pistas
- **OPT + [ARRIBA/ABAJO]**: moverse entre frases en la cadena actual
- Selecciona un rango en la columna de instrumento, luego usa **SHIFT + EDIT**: clona los instrumentos
- Selecciona un rango, luego usa **EDIT + [ARRIBA/ABAJO]**: rota las filas de la frase
- Selecciona un rango, luego toca dos veces **EDIT**: abre la pantalla BOUNCE TO SAMPLE para las filas seleccionadas de la frase en la pista actual (ver Bounce en Song)

En esta pantalla, con una selección activa, la columna de notas puede llenarse
con patrones rítmicos:

- **OPT + IZQUIERDA** llena las celdas de nota seleccionadas con un patrón fijo de 16 pasos
  y avanza al siguiente patrón con cada pulsación. Después del último patrón
  las notas se borran y el ciclo comienza de nuevo. Los patrones se alinean a filas absolutas
  de frase, por lo que una selección parcial solo recibe los pasos que caen dentro.
- **OPT + DERECHA** llena las notas seleccionadas con un patrón aleatorio de
  3-8 notas (7 y 8 menos probables), ciclando indefinidamente mientras se mantiene **OPT**.
- **OPT + ARRIBA** modifica las notas ya colocadas en la selección: cada pulsación
  elige 1-3 notas y las desplaza aleatoriamente dentro de una octava
  (desplazamientos pequeños son más comunes). La octava pulsación restaura
  las notas que tenía la selección cuando empezó el ciclo, luego el ciclo reinicia.
- **OPT + ARRIBA** en una selección que cubre solo la columna de volumen (sin nota,
  instrumento o FX) randomiza las velocidades en vez de mutar: la
  primera pulsación aplica el patrón Minimal (cada volumen varía hasta
  +-20), la segunda Medium (hasta +-50), la tercera Random (valores
  completamente aleatorios), y la cuarta restaura los volúmenes originales.
  Soltar **OPT** reinicia el ciclo a Minimal. Filas sin volumen se dejan intactas.
- **OPT + ABAJO** distribuye las notas colocadas de instrumentos Sampler con slices
  en la selección en una escala cromática: la primera nota de cada instrumento
  (la más alta en el secuenciador) mantiene su valor y cada nota siguiente
  sube un semitono respecto a la anterior, convirtiendo una pila de notas idénticas
  en disparos consecutivos de slices. Varios instrumentos con slices se distribuyen
  independientemente según su primera nota, notas en instrumentos sin slices
  quedan intactas, y el resultado nunca se ajusta a la escala del proyecto.
  La última nota escrita se usa para continuar manualmente el run con el siguiente slice.
  Una selección con solo instrumentos sliced distribuye en cada pulsación;
  si hay notas no sliced, la distribución ocurre en la primera pulsación y
  el arp aleatorio actúa en esas notas desde la segunda pulsación en adelante.
- **OPT + ABAJO** en una selección sin notas de instrumentos sliced es un arpegiador
  aleatorio para las notas colocadas. Se elige un acorde de la paleta CRD – acordes
  de 3 notas para selecciones de 1-2 notas, acordes de 3+ notas para 3, cualquier
  acorde para 4 o más – con la tónica en una nota aleatoria de la escala del proyecto
  dentro del registro de la selección (cualquier nota cromática cuando la escala
  está desactivada o la pista excluida), y sus tonos se cuantizan en la escala
  como en la reproducción para CRD. Cada pulsación redistribuye el acorde en las notas:
  aleatoriamente, de menor a mayor, de mayor a menor, luego arriba y abajo;
  la 5ª pulsación restaura las notas originales y la siguiente elige un nuevo acorde.
  El nombre del acorde aparece en la barra de estado mientras el arp está activo,
  no se añaden ni quitan notas, y soltar **OPT** reinicia el proceso.
- Cada pulsación de llenado borra las notas de la selección antes de escribir, y
  las notas colocadas usan el último instrumento usado; soltar **OPT** reinicia
  los ciclos a su paso inicial.

### Key jazz (solo escritorio)

Pulsa **Esc** en la pantalla Phrase para alternar key jazz, un modo de entrada de notas
tipo piano QWERTY (como [m8c](https://github.com/laamaa/m8c)):

- `Z S X D C V G B H N J M` y `Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P` escriben notas
  cromáticamente en más de dos octavas, usando la posición física de cada tecla
  para que el layout sea igual en teclados AZERTY, QWERTY y QWERTZ.
- Cada pulsación escribe la nota en la fila actual y avanza a la siguiente.
- `[` / `]` cambian la octava.
- **Shift + flechas** selecciona un rango de filas y columnas; una flecha simple
  limpia la selección.
- **Ctrl+C** / **Ctrl+X** copian o cortan las columnas seleccionadas, o la nota/instrumento/volumen
  de la fila actual si no hay selección; **Ctrl+V** pega en el cursor.
- **Delete** elimina toda(s) las fila(s) (todas las columnas) y mueve el resto de la
  frase hacia arriba para llenar el vacío; el cursor permanece en la misma fila.
- **Backspace** elimina el elemento bajo el cursor solo en la columna actual (o en
  las columnas seleccionadas), mueve lo que está debajo hacia arriba y retrocede el
  cursor una fila, como un editor de texto.
- **Insert** inserta una fila vacía, desplazando el resto de la frase hacia abajo.
- **Ctrl+S** guarda el proyecto. Fuera de key jazz, Ctrl+S no hace nada.
- Pulsar Esc de nuevo desactiva key jazz.

Mientras key jazz está activado, se apodera completamente de las teclas de nota en la pantalla Phrase,
incluyendo atajos Edit/Opt/Motion que usan esas teclas — desactívalo con Esc para usarlos de nuevo.

Key jazz también funciona, activado independientemente con Esc, en:

- **Song** y **Chain**: escribe directamente el índice hexadecimal de una cadena o frase
  (`0-9`, `A-F`) en vez de usar Arriba/Abajo. El primer dígito sustituye el valor; los dígitos
  siguientes sin mover el cursor se añaden (escribiendo "3" y luego "F" pone 3F). En Song,
  **Shift+flechas** selecciona un rango, **Delete** elimina filas completas y mueve el resto
  arriba, **Backspace** elimina columna(s) actuales/seleccionadas y sube lo de abajo,
  **Insert** inserta fila vacía, y **Ctrl+C**/**Ctrl+X**/**Ctrl+V** copian/cortan/pegan filas
  (o la fila actual si no hay selección) — los mismos atajos de edición de estructura que en Phrase.
  Esto controla Shift, así que Shift + Izquierda/Arriba ya no saltan a Chain/Project mientras key jazz está activo aquí —
  pulsa Esc para recuperar esos atajos.
- **Cualquier pantalla con campo de texto**: escribe directamente en el teclado en vez del teclado virtual en pantalla.
  Esto incluye nombre archivo de proyecto, título y autor, nombre de instrumento, tema de color, tabla de tono,
  nombre de archivo de bounce, y diálogos de nombre (introducir nombre, crear carpeta). Backspace funciona como en
  un campo común de texto; Shift escribe mayúsculas. Esc alterna esta función en todas esas pantallas a la vez,
  y el teclado virtual se abre normalmente si key jazz está apagado.
- **Instrument**: con key jazz activado, las teclas de nota reproducen el instrumento en la pista actual para probarlo,
  sin entrar nota (misma disposición de piano que en Phrase; la última tecla pulsada suena, el sonido se libera al soltar).
  Las teclas de octava cambian la octava, comenzando desde la octava de la primera nota del instrumento. En el campo **Name**
  las teclas escriben texto. No suena nada mientras la canción reproduce esa pista.

## 5. Instrumentos

Todos los instrumentos comparten algunos parámetros:

- Tipo de instrumento
- Nombre, hasta 15 caracteres
- Velocidad de tabla por defecto, en ticks por fila de tabla
- Volumen
- Panoramización
- Transposición activada o desactivada

Cada instrumento tiene una tabla por defecto con el mismo número en el rango `00-7F`.
Puedes usarlas como tablas auxiliares, pero mantener tablas auxiliares en el rango `80-FE`
evita conflictos y confusión. Las tablas son una de las principales herramientas para diseño de sonido en ChooChooTracker.

### Controles

- **OPT + [IZQUIERDA/DERECHA]**: moverse entre instrumentos
- **EDIT + PLAY**: escuchar vista previa del instrumento
- **SHIFT + OPT**: copiar el instrumento
- **SHIFT + EDIT**: pegar el instrumento

### SOUNDFONT (experimental)

Copia bancos SF2/SF3 originales, instrumentos SFZ/EXS con sus samples, o archivos ZIP
en la carpeta `instruments/USER/soundfont`. Selecciona **SOUNDFONT** en Instrument.
La fila de escaneo muestra **Not scanned**, **Scanning...** o **Scanned**, con acción **Rescan**.
El escaneo lee metadata; no carga todos los samples de cada preset en memoria.

La fila **Preset** muestra el preset activo. Toca y suelta EDIT para abrir el navegador en ese preset:
las colecciones aparecen a la izquierda y sus presets a la derecha. Cada colección presenta una lista plana,
incluyendo presets dentro de archivos bank y ZIP. Los archivos WAV/AIFF se mantienen con sus instrumentos mapeados;
colecciones con solo samples individuales listan esos samples como presets. Los IDs de descarga y hashes de archivo
se eliminan de las etiquetas; se conservan número de nota musical, BPM y take. EDIT selecciona un preset; EDIT+PLAY
previsualiza la selección cargada.

Mantén EDIT y presiona Izquierda/Derecha en Preset para moverte secuencialmente en el mismo orden del navegador,
continuando a la siguiente colección y envolviendo al principio o final de la biblioteca. El ciclaje rápido prepara la última
selección solicitada. El preset actual se puede reproducir mientras se carga el reemplazo. Si falla la carga, el preset solicitado
permanece seleccionado e instrumento queda en silencio. **Error:** y el motivo aparecen abajo en Instrument; el ciclaje continúa
desde ese preset en cualquier dirección. Re-escanea tras importar o cambiar archivos de la librería. El estado del preset aparece
directamente bajo Preset con color fijo. La carga no genera mensajes en SONG u otras páginas.

**Native keys** muestra el rango de notas on autores del preset seleccionado, usando C4 para MIDI 60. **(gaps)** indica que algunas notas
dentro de ese rango no tienen zona mapeada. Es una unión sobre capas de velocidad; no todas las velocidades o articulaciones
suenan en todas las teclas mostradas. Los samples release-only no extienden el rango. Tuning y pitch FX no cambian esta vista ni amplían
el mapeo.

**Edit**, a la derecha de Preset, abre el editor experimental SoundFont:
afina (−2400 a +2400 centésimas), cutoff (20–20000 Hz), resonancia (0–40 dB), retardo/ataque/sostenido/decay/release
(0–20000 ms), sustain (0–100%), volumen, panorámica, y **Bend step** (1–100 centésimas por unidad PBN).
Los instrumentos nuevos usan bend step de 1 centésima.
Archivos antiguos muestran **Stock**, reteniendo su escala PBN previa (25 centésimas en modo pitch lineal, 1 en modo período);
establece Bend step a 1 para bends más finos. Clear restaura bend step a 1 centésima. Esto afecta solo PBN SoundFont;
FPT mantiene afinación fija. Volumen y pan son los valores del instrumento mostrados en la página principal.
Los campos de tono y envolvente default son **Bank**, manteniendo valores originales de cada capa. Un valor explícito
sobrescribe ese parámetro en todas las capas del preset. Clear restaura **Bank**, cero centésimas para Tune, volumen máximo,
y paneo centrado. EDIT+Izquierda/Derecha cambia ligeramente; EDIT+Arriba/Abajo cambia más. OPT retorna a Instrument;
EDIT+PLAY prueba el instrumento en su nota de canción o por defecto. Esto edita controles de reproducción del instrumento,
no el mapeo por sample, zonas de tecla/velocidad ni puntos de loop del banco.

Afinación, cutoff y resonancia se aplican en vivo. Ataque, retardo y hold se establecen al note-on. Decay, sustain y release
pueden actualizar una voz sonando; el release comienza con un nota OFF, no solo en fila vacía. Decay es inaudible con sustain al 100%.
Un tiempo de release no puede extender un sample más allá de su audio disponible. Una anulación explícita de release también hace que
un sample one-shot responda a nota OFF. Con ajustes Bank, mantiene comportamiento one-shot original. Los rangos de tecla y velocidad
del preset son definitivos; notas fuera de esos rangos son silenciosas. FPT puede pitchar un sample seleccionado más allá de ese rango
sin cambiar su mapeo. Las ediciones se guardan en el instrumento y proyecto sin modificar el banco fuente; canciones antiguas sin overrides
mantienen sus ajustes originales.Un WAV con un tipo de loop específico del sampler desconocido puede reproducir su audio sin ese loop. La advertencia del Instrument indica el tipo de loop ignorado; el archivo fuente permanece sin cambios. Un loop WAV hacia adelante que termina exactamente un sample después del audio se acepta como un final exclusivo en el límite del sample, con advertencia; los mapeos PCM y explícitos SFZ/EXS de loop permanecen sin cambios. Otros límites de loop inválidos y notas raíz reportan errores específicos. Un único byte de alineación RIFF cero contado erróneamente en una trama PCM de tamaño impar se ignora con advertencia; otras tramas PCM parciales siguen siendo errores. Los loops alternados/hacia atrás WAV estándar aún no son compatibles.

**SoundFont FX** expone ocho controles en el selector contextual de Lane FX. Todos los valores a continuación usan notación hexadecimal de tracker. Los FX afectan solo su pista y no cambian los controles guardados del instrumento. Cada nueva nota parte de los ajustes del instrumento y luego aplica comandos en su fila y filas siguientes.

| FX | Control | Valores |
| --- | --- | --- |
| FPT | Pitch | `00` = −24 semitonos, `18` = cero, `30` = +24; pasos exactos de semitono |
| FCF | Cutoff | `00`–`FE`: 20 Hz–20 kHz, logarítmico |
| FRS | Resonancia | `00`–`FE`: 0–40 dB |
| FAT | Ataque | `00`–`FE`: 0–20 segundos, espaciado más fino en tiempos cortos |
| FHD | Hold | `00`–`FE`: 0–20 segundos antes de la decay, se ajusta en la fila de la nota |
| FSU | Sustain | `00`–`FE`: nivel sostenido 0–100% |
| FDC | Decay | `00`–`FE`: 0–20 segundos, espaciado más fino en tiempos cortos |
| FRL | Release | `00`–`FE`: 0–20 segundos, espaciado más fino en tiempos cortos |

Para FCF/FRS/FAT/FHD/FDC/FSU/FRL, `FF` vuelve al ajuste de Edit (incluyendo **Bank**). Pon Ataque y Hold en la fila de la nota. Decay, Sustain y Release pueden cambiar la nota activa; usa OFF para oír su release. Volumen y paneo usan los controles comunes; los comandos heredados FVL/FPN aún cargan y reproducen pero ya no se ofrecen. Pitch FX cambia la afinación sin modificar la zona de clave seleccionada del banco. Los comandos de pitch/fine-pitch del secuenciador, MODs y efectos de pista siguen disponibles. Los proyectos guardan referencias estables de banco/preset y comandos FX; las librerías de samples permanecen externas.

Todos los bancos son bancos USER. Instrumentos grandes pueden superar la caché PCM predeterminada de 64 MiB, y parámetros fuente no soportados generan advertencias. Consulta la [guía de SoundBanks](soundbanks/README.md) para rutas de plataforma, importación desde navegador, configuración de caché, cobertura de formatos y límites actuales de validación.

#### Compatibilidad SoundFont: bancos, presets y modulación

Un solo archivo **bank** SF2 o SF3 puede contener muchos **presets**, junto con sus samples. Una **colección** de librería es la carpeta usada para agrupar esos archivos en el navegador. Son niveles diferentes: un preset puede tener comportamiento no soportado incluso si otros presets en el mismo archivo se traducen bien.

**Modulación de preset** significa enlaces programados desde entradas como velocity, tecla, mod wheel o presión hacia un parámetro de sonido. Esos enlaces pueden pertenecer a un preset o a una capa instrumental subyacente compartida por varios presets. No necesariamente son un ajuste que afecta todos los sonidos en el banco.

| Función del preset | Traducción actual |
| --- | --- |
| Mapeo de sample/tecla/velocity, pitch raíz y afinación | Implementado; los rangos definidos se mantienen. Zonas de velocity y velocity ordinaria del tracker aún funcionan. |
| Offsets de sample, loops soportados, ganancia/pan y grupos exclusivos | Implementado en el parser y límites de voces. |
| Cutoff/resonancia estáticos de filtro pasa bajos y envolvente de volumen | Implementado mediante el renderizador del tracker; no garantiza respuesta idéntica a otro reproductor SoundFont. |
| Rutas de modulación personalizadas de preset/instrumento (`pmod` / `imod`) | No implementado. Son ignorados los mapeos autorados de controlador a parámetro, cantidades y curvas de respuesta. |
| Modulación LFO/vibrato y rutas de envolvente de modulación SoundFont | No implementado; puede faltar movimiento programado de pitch, filtro o volumen. |
| Hold/decay de envolvente de volumen escalado por tecla, generadores de tecla fija/velocity fija | No implementado. |
| Envíos chorus/reverb SoundFont | No implementado. Efectos de tracker pueden ajustarse manualmente pero no reproducen automáticamente las configuraciones del preset. |

El mensaje antiguo **“Custom bank modulation ignored”** describe una comprobación a nivel de archivo: el parser encontró registros de modulación personalizados en algún lugar del banco. No establece que el preset seleccionado los use, ni que todo el banco sea inutilizable. Esta limitación general se documenta aquí en lugar de repetir ese mensaje en cada preset. Diagnósticos detallados del parser retienen esos registros; otros errores de carga y advertencias de compatibilidad siguen apareciendo cuando procede. El motor tampoco afirma un comportamiento completo de moduladores preset por defecto en SoundFont. PBN del tracker, vibrato, Edit y Lane FX son controles independientes, no traducciones automáticas de programación omitida del preset.

Para elegir una librería, comienza con presets basados principalmente en mapeos de samples, loops y envolventes estáticos. Sonidos cuyo carácter depende de articulaciones por controlador, filtros animados o LFOs programados requieren comparación más detallada en un reproductor SoundFont completo. **Carga y reproduce** no significa **reproduce completamente el preset**. Compara notas y velocities útiles, así como notas sostenidas y tiempo de liberación (note OFF). Cambiar SF2 a SF3 (o al revés) no elimina por sí solo estas necesidades de modulación. Una conversión debe traducir los mapeos y comportamiento a funciones soportadas por este motor; renderizar un efecto en un sample solo preserva ese resultado, no su respuesta interactiva al controlador. Consulta la [cobertura de formato y límites](soundbanks/README.md#format-coverage)
y [notas de validación](soundbanks/validation.md) antes de elegir o convertir un banco. Estos límites describen la implementación experimental actual, no restricciones inherentes a todos los reproductores SoundFont.

#### Explorar colecciones SoundFont

En SAMPLE → SOUNDFONT, **Preset** abre tus colecciones. **Right** o **EDIT/A** entra directamente en una carpeta. En una colección hoja, Right pasa a sus presets; otro Right avanza 15 entradas sin cargar. EDIT/A carga el preset resaltado. Left vuelve a las colecciones y luego a la carpeta superior; OPT también retrocede. PLAY inicia/detiene la canción mientras exploras.

Iconos de carpeta y de sonido identifican las entradas. Las colecciones usan toda la altura del panel. El preset seleccionado permanece bajo las ocho vistas previas de pista; los nombres largos se desplazan tras un segundo de inactividad en la selección y reinician al moverte. EDIT+Left/Right en la página de instrumento sigue el mismo orden de carpetas anidadas, manteniendo juntos los sonidos de cada colección. EDIT+PLAY escucha.

La tercera fila informa Reading preset, Finding samples, Unzipping, Checking samples o Loading samples. Los instrumentos EXS con nombres incrustados vacíos o `(null)` usan sus nombres de archivo. Los bancos de origen y el audio no cambian. Usa **Rescan** tras modificar los archivos de tu biblioteca.

Consulta [cobertura de formatos y límites](soundbanks/README.md#format-coverage) para el comportamiento compatible del sampler y los formatos de sample.

### AY Classic, AY Plus y AY Sample

Estos son los motores originales ChipNomad AY/YM. AY Classic ofrece controles de tono, ruido y envolvente al estilo hardware. AY Plus añade osciladores por software y modulación más rica. AY Sample reproduce un sample mediante niveles de volumen estilo AY y es distinto del motor Sampler.

#### AY Classic

Este tipo instrumental es legado de ChipNomad `v1.0.0`. El instrumento AY Plus más nuevo ofrece más funciones.

Los instrumentos AY Classic tienen estos parámetros:

- Mixer: tono activado o desactivado, ruido activado o desactivado, forma de envolvente (`0-F`)
- Volumen: envolvente ADSR generada por software
- Periodo automático de envolvente: activado o desactivado, con tasa de `1:1` a `F:F`

La **Calidad AY** en Settings afecta solo el render AY/YM. No cambia la calidad de Braids, Plaits ni Sampler. El **dithering de sample** se aplica a la cuantización de AY Sample.

Consulta la [documentación ChipNomad AY-3-8910](https://chipnomad.org/chips/ay-3-8910/) para detalles del chip.

#### AY Plus

AY Plus aplica conceptos de sintetizador clásico al diseño de sonido AY-3-8910/YM2149F. Se puede pensar en AY como un sintetizador con 5 osciladores:

- 3 osciladores de tono de onda cuadrada
- 1 oscilador de ruido
- 1 generador de envolvente de amplitud que puede usarse como oscilador

Bajo este modelo, cada instrumento tiene 3 osciladores hardware: tono, ruido y envolvente. AY Plus añade un 4º oscilador por software. Actualizar registros AY más rápido que la tasa de ticks del proyecto abre más opciones de diseño sonoro. Debido al modo de operación de AY, los osciladores se mezclan usando modulación en anillo. Prueba distintos offsets de pitch, desafinaciones y combinaciones de osciladores para ver resultados.

Cada oscilador tonal puede afinarse independientemente con offsets de pitch y afinación fina.

Tipos de osciladores por software y sus parámetros:

- Pulse - onda cuadrada con ancho de pulso controlable
  - FM Depth - profundidad simple de FM
  - Ancho de pulso - rango completo `00-FF`, con pasos definidos en pantalla de Proyecto
  - Pulse low - controla el valor bajo en un par repetido cuyo valor alto es siempre `15`
- Sync Tone - efecto de hard-sync para el oscilador de tono
  - FM Depth - profundidad simple de FM
- Sync Env - efecto de hard-sync para el oscilador de envolvente
  - FM Depth - profundidad simple de FM
  - Ancho de pulso - razón de duración entre las 2 formas del par de envolvente
  - Envelope Pair - 2 formas de envolvente para alternar
- Wavetable - tablas de ondas de 32 pasos, editadas en la pantalla Wavetable
  - FM Depth - profundidad simple de FM
  - Índice de wavetable
- Tone FM - efecto simple tipo FM para oscilador de tono
  - FM Depth - profundidad de FM para tono
- Env FM - efecto simple tipo FM para oscilador de envolvente
  - FM Depth - profundidad de FM para envolvente

Los osciladores por software Pulse y Wavetable no pueden usarse con el oscilador Envelope. El oscilador Envelope se desactiva automáticamente.

El FM simple cambia rápidamente entre 2 valores de periodo, uno más bajo y otro más alto. Esto es similar a usar una onda cuadrada como modulador FM. Pulse, Sync Tone, Sync Env y Wavetable usan una relación `1:1` entre portadora y modulador FM. Tone FM y Env FM pueden imitar otras relaciones mediante offsets de pitch de oscilador por software.

#### AY Sample

Puedes cargar archivos WAV mono o estéreo en `8-bit`, `16-bit`, `24-bit` o `32-bit`. Se convierten a `8-bit` y se truncan a `16 KB`. Este límite coincide con el tamaño de página RAM del ZX Spectrum.

AY reproduce samples mediante escrituras rápidas al registro de volumen. Su salida es unipolar, así que los samples necesitan "elevarse a cero" para una reproducción más limpia. Esto elimina clicks al final del sample y mejora las colas silenciosas.

AY tiene resolución DAC de `4-bit` con escala de volumen no lineal, por lo que los samples de `8-bit` se convierten a `4-bit` durante la reproducción. La opción de dithering en Settings puede mejorar la profundidad de bits percibida. El dithering no es práctico en plataformas retro como ZX Spectrum y Atari ST, así que apágalo si escribes para hardware real o buscas un sonido crujiente lo-fi.

Los samples funcionan como otro oscilador por software AY Plus, pero sus parámetros extra los hacen más manejables como tipo instrumental separado.

Parámetros de sample:

- Sample rate - reproducir un sample a su tasa nativa produce un `C-4`
- Inicio de sample
- Longitud de sample
- Inicio de loop de sample
- Offset de pitch del sample
- Offset fino de pitch del sample

Las secciones de osciladores Tone y Noise coinciden con las de AY Plus. Debido al modo de AY, todos los osciladores se mezclan usando modulación en anillo.

### Braids

Braids ofrece 47 modelos de síntesis. Sus controles principales son:

- **Model**: oscilador o algoritmo de síntesis
- **Timbre** y **Color**: macros dependientes del modelo
- **Filter**: filtro adicional LP/BP/HP, pendiente de `12` o `24 dB`, cutoff y resonancia
- **ADSR**: ataque, decay, sustain y release

Pulsa **Model** para elegir entre listas de modelos categorizados.

| Categoría | Modelos |
|---|---|
| Analog | `00 CSAW`, `01 MORPH`, `02 SAW-SQUARE`, `03 SINE-TRI`, `04 BUZZ`, `05 SQUARE-SUB`, `06 SAW-SUB`, `07 SQUARE-SYNC`, `08 SAW-SYNC` |
| Multi Osc | `09 TRIPLE-SAW`, `10 TRIPLE-SQR`, `11 TRIPLE-TRI`, `12 TRIPLE-SINE`, `13 TRIPLE-RING`, `14 SAW-SWARM`, `15 SAW-COMB`, `16 TOY` |
| Filter / Voice | `17 FILTER-LP`, `18 FILTER-PEAK`, `19 FILTER-BP`, `20 FILTER-HP`, `21 VOSIM`, `22 VOWEL`, `23 VOWEL-FOF`, `24 HARMONICS` |
| FM / Chaos | `25 FM`, `26 FEEDBACK-FM`, `27 CHAOTIC-FM` |
| Physical | `28 PLUCKED`, `29 BOWED`, `30 BLOWN`, `31 FLUTED`, `32 STRUCK-BELL`, `33 STRUCK-DRUM` |
| Drums | `34 KICK`, `35 CYMBAL`, `36 SNARE` |
| Wavetables | `37 WAVETABLES`, `38 WAVE-MAP`, `39 WAVE-LINE`, `40 WAVE-PARA` |
| Noise / Granular | `41 FILTER-NOISE`, `42 TWIN-PEAKS`, `43 CLOCK-NOISE`, `44 GRAN-CLOUD`, `45 PARTICLE`, `46 DIGI-MOD` |

#### Parámetros

Timbre y Color dependen del modelo seleccionado. Consulta el manual original de Braids de Mutable Instruments para la lista completa y detalles, o simplemente juega de oído.

#### VCF y VCA

Braids alimenta el filtro multimodo estándar de ChooChooTracker y la envolvente ADSR.

#### Settings

Los ajustes globales de Braids están disponibles en los ajustes de la app:

- **BITS**: reduce resolución de salida
- **DRFT**: añade inestabilidad de pitch en el oscilador
- **SIGN**: reproduce el algoritmo original de imperfección de la forma de onda de Braids

### aChChid**aChChid** es un motor monofónico de acid bass basado en Open303. `Square` y `Saw` usan su oscilador TB-303 nativo, filtro, envolvente y comportamiento de acento. `Braids` reemplaza solo el oscilador, continuando por el mismo filtro y amplificador 303. Expone Model, Timbre, Color y un waveshaper extra. El waveshaper evoluciona desde saturación suave hasta wavefolding.

Cutoff y Decay usan edición logarítmica: **EDIT + IZQUIERDA/DERECHA** mueve un paso musical, mientras **EDIT + ARRIBA/ABAJO** mueve dieciséis pasos. Cutoff cubre de 200 Hz a 20 kHz; Decay abarca de 200 ms a 2 s.

Una `F` en la columna de volumen de nota activa un acento. `ASL` desliza a esa nota desde la anterior sin reactivar la envolvente 303; retiene automáticamente la puerta precedente, y `ASL 00` da un glide de `60 ms`. `ATY` mantiene la nota abierta durante todo el paso actual. Las notas sin `ASL` siempre reactivan la envolvente.

Los destinos de modulación incluyen Decay y Accent, además de Timbre y Color en modo onda Braids.

### Bogie

**Bogie** es un sintetizador nativo de percusión one-shot. Las notas del tracker fijan la afinación y el volumen del tracker/instrumento define el nivel; cada disparo tiene su propio decay interno, por lo que no tiene página ADSR. Cuenta con 12 modelos: `Kick`, `Snare`, `Hat`, `Clap`, `Tom`, `Rim`, `FM`, `Noise`, `Cowbell`, `Cymbal`, `Shaker` y `Clave`.

Sus seis macros son **Decay**, **Tone**, **Sweep**, **Noise**, **FM** y **Drive**. Sus etiquetas se adaptan según el modelo (por ejemplo, el Noise de Kick es `Click`, y el FM de Cowbell es modulación cruzada metálica). Cada macro está activa en todos los modelos, pero su función musical cambia según el modelo: Sweep puede alterar el tono, espaciamiento de ráfagas o dispersión metálica; FM puede añadir modulación cruzada, una capa metálica o un armónico tipo ring. La mitad inferior de cada rango es para sonidos de percusión convencionales; la mitad superior abre morfos tímbricos más amplios y resultados más sintéticos.

Cada modelo puede usar el filtro multimodo LP/BP/HP compartido, con carácter Clean, Classic, Aggro o Acid, pendiente de `12`/`24 dB`, cutoff y resonancia. Bogie soporta todas sus macros activas más Volume, Pitch, Cutoff y Resonance para modulación y grabación de movimiento. Usa Decay corto en Cowbell para ataques phonk agudos; aumenta FM para un campanilleo más duro y metálico.

En la pantalla de Modulación, presiona **EDIT** en una fuente para elegir una familia (Envelopes, LFO o Sticks), luego la fuente concreta. **Izquierda** y **Derecha** siguen ciclando fuentes directamente.

### Sintered

**Sintered** es un sintetizador nativo de percusión digital one-shot. Las notas fijan su afinación fundamental; sus seis modelos son `Knot`, `Shard`, `Burst`, `Comb`, `Logic` y `Melt`. Cada modelo tiene **Decay**, **Mod** y **Motion**, más tres controles propios: Knot usa Ratio/Spread/Fold, Shard Ratio/Feedback/Bite, Burst Noise/Color/Feedback, Comb Time/Damping/Regen, Logic Rate/Pattern/Crush y Melt Ratio/Chaos/Drive. **Mod** es siempre la profundidad de acoplamiento interna.

Motion es una macro envolvente interna: valores bajo `80` usan forma AD, `80` desactiva la envolvente, y valores sobre `80` usan forma solo decay. Acercarse más al centro ralentiza la envolvente. Sus destinos son fijos por modelo, haciendo a Sintered predecible en patrones tracker: Knot anima Mod/Fold; Shard Feedback/Bite; Burst Noise/Color/Feedback; Comb Damping/Regen; Logic Rate/Crush; Melt Chaos/Drive. Todos los modelos resetean su estado con disparo de nota y limitan la retroalimentación interna para golpes repetibles.

### MonkSynth

MonkSynth es un sintetizador vocal. Modela su vocal, el carácter de la voz, el soplo, el glide, el vibrato, el unísono y el delay desde la página Instrument. EDIT abre todos los controles y el nombre del patch.

Elige un banco y un preset sobre los controles VOICE. ALL incluye FACTORY y USER; EDIT+PLAY escucha el preset resaltado, mientras que EDIT lo selecciona. Guarda tus propios sonidos en `instruments/USER/monk/`. Los instrumentos y canciones guardados conservan sus ajustes de MonkSynth.

Los valores son hexadecimales `00..FF`, salvo Voices (`01..0A`, de una a diez). Attack, decay y release abarcan 0..5 segundos; el detune 0..50 cents y la apertura de voces 0..0,5. Volume, Pitch y los seis controles principales de voz están disponibles como destinos de modulación y de grabación de movimiento.

Los dieciséis controles aparecen juntos en **MonkSynth FX**: `MWV` (vocal), `MIN` (voz), `MAM` (soplo), `MFL` (glide), `MFB` (profundidad de vibrato), `MSH` (velocidad de vibrato), `MUV` (voces), `MDT` (detune), `MVS` (apertura), `MDM` (mezcla de delay), `MDR` (velocidad de delay), `MLV` (nivel) y `EAT`/`EDC`/`ESU`/`ERL` (ADSR). Estos significados se aplican cuando MONK es el instrumento activo; los significados existentes de los comandos de MME no cambian. El grupo Trigger FX compartido conserva TDC/TCL sin entradas de envolvente duplicadas.

### MME

Elige **SYNTH → MME** en el menú Tipo de instrumento.

**MME** (Multi Modulation Engine) es un sintetizador de dos osciladores dedicado a interacciones entre ellos. Sus modelos son `Ring`, `Fold`, `Cross`, `VPM`, `Sync`, `Logic` y `Vocode`. Ambos osciladores son internos y siguen las notas del tracker; `Interval` define su relación.

Las seis macros son **Waves**, **Interval**, **Amount**, **Flow**, **Feedback** y **Shaper**. Waves interpola entre pares musicales de forma A/B. Amount controla la interacción central del modelo, Flow cambia la dirección o variante algorítmica, y Feedback es limitada pero puede volverse muy extrema al final del rango. Shaper es una etapa final compartida que va de limpio a saturación y wavefolding.

MME usa el filtro multimodo LP/BP/HP compartido y ADSR. Las seis macros, Cutoff y Resonance están disponibles para modulación y grabación de movimiento. Mantén **EDIT** en Model para abrir el popup de selección.

### MIDI Out

**MIDI Out** (desktop, PortMaster y navegadores Web MIDI) controla un dispositivo MIDI externo en vez de sintetizar audio: disparar una nota envía un MIDI Note On real por el **Channel** (`1-16`) del instrumento y al terminar la nota, el Note Off correspondiente. El volumen se convierte en velocity, y las voces de una pista de acorde reciben su Note On/Off propio. No hay audio audible por ChooChooTracker mismo: selecciona primero el dispositivo de salida MIDI en [Settings](#13-settings). Consulta [MIDI](#15-midi) para el panorama completo, incluyendo preescucha desde un teclado MIDI.

**Program** y **Bank high/low** (CC0/CC32) son opcionales - se muestran como `--` cuando están desactivados, `Clear` desactiva un campo y recuerda su valor. Cuando están activados, se envían una vez, justo antes de la siguiente nota, solo cuando difieren de lo que ese canal recibió antes (no antes de cada nota, para evitar retrigger del propio patch/envelopes del dispositivo receptor).

**MC1 number**-**MC4 number** eligen qué número CC (`00-7F`) envía cada una de las cuatro filas MC1-MC4 - véase [MIDI Out FX](#midi-out-fx). También opcionales `--`; un slot sin asignar deja su FX inactivo.

### Motores de arquitectura clásica

Los motores en esta categoría comparten arquitectura VCO a VCF a VCA.

Hay varios tipos de VCO disponibles:

- **Braids**
- **Plaits**
- **Plaits-Alt**
- **Sampler**
- **2xSCWF**
- **BYOWTBL**

Cada instrumento conecta su VCO a un filtro multimodo.

El filtro tiene pendiente conmutable de `12 dB` o `24 dB` y puede funcionar como filtro low-pass (LP), band-pass (BP) o high-pass (HP).

Hay varios caracteres de filtro disponibles:

- **Clean**: estéril y digital
- **Classic**: inspirado en sintetizadores analógicos clásicos americanos
- **Aggro**: inspirado en sintetizadores analógicos japoneses agresivos
- **Acid**: doble agresividad que Aggro

Classic, Aggro y Acid pueden volverse muy resonantes. Empieza bajo y sube la resonancia suavemente.

El filtro alimenta un VCA controlado por una envolvente ADSR. La forma de la envolvente puede morph entre curvas exponenciales, lineales y logarítmicas.

#### Plaits y Plaits-Alt

**Plaits** ofrece 24 motores, agrupados en el popup de selección así:

| Categoría | Motores |
|---|---|
| Analógico / Ondas | `00 VA VCF`, `01 PHASE DIST`, `05 WAVE TERRAIN`, `06 STRING MACH`, `08 VIRTUAL ANALOG`, `09 WAVESHAPING`, `11 FORMANT`, `12 HARMONIC`, `13 WAVETABLE`, `14 CHORD` |
| FM | `02 6-OP FM 1`, `03 6-OP FM 2`, `04 6-OP FM 3`, `10 2-OP FM` |
| Digital | `07 CHIPTUNE`, `15 SPEECH` |
| Textura / Ruido | `16 SWARM`, `17 NOISE`, `18 PARTICLE` |
| Física | `19 STRING`, `20 MODAL` |
| Percusión | `21 BASS DRUM`, `22 SNARE DRUM`, `23 HI-HAT` |

**Plaits-Alt** es una colección alternativa de modelos para el módulo Plaits original. La mayoría son más extraños o más experimentales que un VCO estándar.

| Categoría | Motores |
|---|---|
| Granular / Micro | `GLISSON`, `PULSAR`, `GENDY`, `SCANNED`, `LOOPBACK` |
| Fase / Armónico | `PHASE WEAVE`, `SIDEBAND BANK`, `UNDERTOW`, `ATTRACTOR`, `LOCKSTEP` |
| Acústico / Físico | `REED PIPE`, `BRASS`, `SHAKERS`, `CLAPS`, `FRESHETS FORMANT` |
| Polifonía / Armonía | `DIATONIC CHORD`, `SCALE STACK`, `WT DIATONIC CHORD`, `WT SCALE STACK`, `HELIX` |
| Digital / Extraño | `BYTEBEAT`, `RULEFIELD`, `SPECTRAL SPIRAL`, `PHASE FLOCK` |

Los parámetros siguen el diseño de Mutable:

- **Harmonic** controla relaciones armónicas, balance o selección interna dentro del motor.
- **Timbre** generalmente va de espectros oscuros/escasos a brillantes/densos.
- **Morph** explora otra dimensión tímbrica.
- **Main/Aux** mezcla la salida principal con la alternativa del motor.

Su significado exacto depende del motor. Por ejemplo, motores de acordes los usan para tipo de acorde, inversión y forma de onda. Modelos físicos para material, excitación y decay, mientras que motores de percusión ajustan tono, carácter y decay. Consulta el manual de Plaits para detalles.

Para `CHORD`, `DIATONIC CHORD` y `WT DIATONIC CHORD`, Harmonic muestra el nombre del acorde seleccionado (`MIN`, `MAJ`, `MIN7`, `MAJ7`, etc.) en vez de su valor crudo. Braids `WAVE PARA` nombra de forma similar sus voicings de Color.

Toca **Engine** para elegir de listas categorizadas.

**Env Mode** tiene 2 rutas:

- `TRIG` reproduce el módulo con TRIG conectado y LEVEL sin parchear
- `VCA` mantiene LEVEL abierto y aplica el ADSR del tracker tras la voz

#### Sampler

Este motor de Sample limpio reproduce muestras PCM mono o estéreo cargadas en RAM.- Toca **Sample** para cargar un WAV PCM sin comprimir de `8-bit`, `16-bit` o `24-bit`. Pulsa **PLAY** en el navegador para escuchar el archivo resaltado.
- Al cargar un sample, aparece **EDIT** junto a su nombre. Tócalo para abrir la pantalla de Edición de Sample. **OPT** o **SELECT + [LEFT]** regresa al instrumento.
- La pantalla de Edición de Sample muestra el nombre del archivo (con un marcador `*` cuando el sample en RAM difiere del archivo en disco), el formato (frecuencia de muestreo, canales), una forma de onda alta, y los campos **Region**, **Select**, **Slice**, **Process** y **File**.
- Las ediciones existen solo en RAM hasta que se guardan: el proyecto almacena la ruta del archivo y recarga el WAV desde disco al volver a abrir, por lo que salir del tracker sin **Save**/**Save As** descarta los procesos (el marcador `*` lo avisa). Cargar otro sample en el instrumento también descarta ediciones no guardadas y resetea la Región al sample completo (Inicio `00`, Fin `FF`).
- **Region** ajusta los marcadores de Inicio/Fin de reproducción, también disponibles en la pantalla del instrumento Sampler. Establecen límites normalizados (`00-FF`); si Inicio está tras Fin, el sample se reproduce en reversa. **EDIT + [LEFT/RIGHT]** ajusta finamente el marcador en pasos de uno (`01`) y zoom en la forma de onda mientras **EDIT** se mantiene pulsado; **EDIT + [UP/DOWN]** ajusta en pasos grandes (16) y retorna a la vista completa. **EDIT + OPT** reinicia el marcador a su valor por defecto (Inicio `00`, Fin `FF`).
- La vista de la forma de onda se acerca a una ventana fija de un segundo alrededor del marcador editado y se desplaza para mantenerlo visible; samples que caben en la ventana permanecen en vista 1:1 completa. El zoom es temporal: dura mientras **EDIT** está pulsado, liberar **EDIT** vuelve a la vista completa, lo mismo con ajustes grandes. Al entrar a la pantalla siempre se resetea la vista al sample completo.
- **Select** define una selección de procesamiento en frames absolutos, independiente de los marcadores de Inicio/Fin. **EDIT + [LEFT/RIGHT]** mueve el cursor 20 frames y aplica zoom mientras **EDIT** está pulsado; **EDIT + [UP/DOWN]** salta `frameCount/64` (mínimo 16) y vuelve a la vista completa. **EDIT (tap)** sobre un cursor copia la posición actual del marcador Inicio o Fin al cursor. **EDIT + OPT** en cualquier cursor limpia toda la selección. Al estar vacía muestra `-` en ambos, y si los cursores están invertidos se intercambian automáticamente. La selección es solo estado de sesión: no se guarda en el proyecto y al entrar se inicializa abarcando la Región de reproducción (todo el sample con marcadores por defecto).
- **Slice** es una fila con cuatro celdas de valores — **Mode**, **Count**, **Slice** y **Frame** — y se guarda con el instrumento. Off reproduce la ventana de Inicio/Fin. Las notas Phrase seleccionan slices cromáticamente desde **C-0**, y las notas más allá del último slice vuelven al inicio (nota N reproduce el slice `N % count`, un ciclo continuo). Las notas sliceadas no transponen pitch ni usan cuantización de Scale, pero **CRD** mantiene el slice seleccionado y transpone esas voces como acorde. Los inicios de slice se marcan en la forma de onda con líneas de dos píxeles de ancho (naranja oscuro; la línea del slice actual es más brillante), y el slice bajo el cursor tiene una banda negra en el fondo de la forma de onda (la tonalidad de selección no cubre la banda, que se mantiene negra y claramente diferenciada del resto).
  - **Mode** cicla entre `OFF` / `EQUAL` / `AUTO` / `LAZY` (EDIT + izquierda/derecha o tap; EDIT + OPT apaga el slicing). Cambiar modos inicializa los puntos de slice: **EQUAL** divide la ventana entre Inicio y Fin en partes iguales según **Count**; **AUTO** detecta transitorios y coloca un slice en cada ataque detectado (hasta **Count**); **LAZY** empieza con un slice que abarca toda la ventana. Cambiar a LAZY guarda la configuración EQUAL/AUTO actual (count y límites), y volver a EQUAL o AUTO la restaura intacta — no pide confirmación ni pierde datos. Slice y Stretch son mutuamente excluyentes: activar uno desactiva el otro, y las celdas de Slice se atenúan mientras Stretch controla la duración.
  - **Count** muestra el número total de slices (`01`..`64`). **EDIT + [LEFT/RIGHT]** lo ajusta en uno (mín. 1, máx. 64), **EDIT + [UP/DOWN]** cicla entre potencias de dos `2 / 4 / 8 / 16 / 32 / 64` con envolvimiento. Cada cambio recalcula la división: **EQUAL** vuelve a dividir igual, **AUTO** reejecuta la detección con el nuevo objetivo (la sensibilidad depende de la cuenta — más slices significa umbral más sensible). Muestra `-` cuando el modo es OFF. En **LAZY** permanece iluminado (cuenta los slices colocados manualmente) pero solo es visual: el cursor lo omite — moverse a la derecha desde **Mode** o a la izquierda desde **Slice** cae en **Slice**, bajar desde el marcador **END** de la fila **Select** llega también ahí, y tocar la celda no hace nada (los slices manuales no tienen división que recalcular). **EDIT + OPT** y tap simple en esta celda no hacen nada: apagar slicing es en **Mode**, borrar un slice es en **Slice**.
  - **Slice** es el navegador: muestra el slice actual indexado en 1 (`01`..count). **EDIT + [LEFT/RIGHT]** avanza un slice a la vez, **EDIT + [UP/DOWN]** salta cuatro (ambos limitados, no envuelven); el slice seleccionado se indica en la forma de onda con un marcador más brillante y la banda negra, y la vista zoom se centra en el inicio del slice. **EDIT + OPT** elimina el slice actual (se fusiona con el anterior; el primer slice no puede borrarse, borrar el último convierte slicing en OFF — los límites persisten y se restauran al volver a modo). **EDIT (tap)** reproduce el slice seleccionado en reproducción única (funciona en todos los modos; la preescucha se detiene al soltar la tecla o salir de la pantalla). Muestra `-` cuando el modo es OFF.
  - **Frame** muestra el frame de inicio del slice actual en hex. **EDIT + [LEFT/RIGHT]** lo mueve en pasos de diez frames (haciendo zoom en el marcador mientras **EDIT** está pulsado), **EDIT + [UP/DOWN]** mueve 100 frames y regresa a la vista completa. Un marcador queda dentro de su slice: no puede pasar el inicio del slice previo o siguiente. **EDIT + OPT** elimina el slice.
  - **B** sale a la pantalla del instrumento desde la fila Slice, igual que en el resto de esta pantalla; **SHIFT + LEFT** sale desde cualquier lugar.
  - **Flujo LAZY**: con LAZY seleccionado, toca **PLAY** para iniciar una reproducción única del sample completo — reproduce una vez desde el inicio y para solo, con el pitch y velocidad original (un marcador verde de dos píxeles sigue la posición en la forma de onda); pulsar **PLAY** otra vez la detiene anticipadamente. Mientras reproduce, cada clic de **EDIT** deja un slice en la posición de reproducción, sin importar donde esté el cursor (slices a menos de 50 ms de uno existente son rechazados con el mensaje `Too close to slice`). Reproducir el sample de nuevo mantiene los cortes existentes y añade nuevos en cada posición pulsada, en orden — el contador **Count** se actualiza y el navegador **Slice** sigue el nuevo slice. Las mismas reglas de navegación y edición que en otros modos aplican a los slices colocados a mano: **EDIT + dirección** en **Slice** los recorre, **Frame** ajusta el inicio seleccionado, **EDIT + OPT** borra el slice actual. La celda Frame se atenúa mientras la creación de slice está armada. **SHIFT + PLAY** no afecta en LAZY: inicia reproducción de Phrase como en otras pantallas. Los slices LAZY se reproducen en el secuenciador igual que EQUAL y AUTO: las notas Phrase se asignan cromáticamente desde **C-0** a slices manuales, ciclando past el último.
  - **Sugerencias combinadas**: cuando el cursor está en una celda de Slice y no hay otro mensaje activo, la barra de estado muestra la sugerencia de esa celda y la mantiene mientras el cursor permanece ahí. En **Mode** la sugerencia cambia según el modo activo y muestra solo su descripción (el nombre del modo ya está en la celda): `Divides sample in equal parts`, `Divides sample based on transients`, `Press Play and add slices with EDIT` — cambiar modo actualiza el texto inmediatamente, y OFF limpia la barra. En **Count** dice `Adjust the number of slices`, en **Slice** `Browse slices`, y en **Frame** `Adjust slice start`. Cualquier mensaje de acción reemplaza la sugerencia hasta expirar.
- **Process** selecciona una operación destructiva: **Crop**, **Normalize**, **Delete**, **Silence**, **Fade In**, **Fade Out** o **Reverse** (EDIT + izquierda/derecha cicla, tap avanza, EDIT + OPT selecciona ninguna). **GO** (en la misma fila) ejecuta la operación sobre la selección actual, o sobre todo el sample si no hay selección. **Crop** mantiene solo la selección; **Normalize** escala la selección para que su pico llegue a escala máxima (ambos canales comparten ganancia, preservando imagen estéreo); **Delete** elimina la selección y une los extremos (no se permite borrar todo el sample); **Silence** silencia la selección; **Fade In**/ **Fade Out** crean rampas lineales desde/hacia silencio en la selección; **Reverse** invierte la selección (los frames se intercambian en su lugar, los dos canales del frame se mueven juntos, duración y marcadores sin cambios). Crop y Delete requieren selección; sin ella muestran `Select region first`. Cada operación pausa el audio brevemente, guarda un nivel de deshacer, y marca el sample como modificado en RAM: el archivo en disco no se toca hasta el flujo de guardado (ver abajo), salir descarta el deshacer.
- **UNDO** (junto a GO) restaura el estado previo a la última operación. Está atenuado hasta que se realiza una operación, alterna entre pre y post operación con pulsaciones repetidas, y se limpia al volver a entrar a la pantalla.
- **File** gestiona los flujos de guardado. El instrumento guarda la ruta del WAV cargado; estos flujos escriben ese archivo o apuntan a uno nuevo. Nunca modifican el nombre del instrumento.
  - El campo File cicla entre **Save** y **Save As** (EDIT + izquierda/derecha o tap); **GO** ejecuta la acción mostrada.
  - **Save** sobreescribe el WAV desde donde se cargó el sample. Para un sample sin slices pregunta primero `Overwrite <name>?`. Un sample sliceado abre el diálogo **SAVE SLICES** preguntando dónde guardar los puntos de slice: **SAVE TO SAMPLE** los escribe en el WAV como chunks cue (portables — DAWs los muestran como marcadores) y los guarda también en el proyecto, **SAVE TO PROJECT** guarda un WAV plano y mantiene los puntos solo en el proyecto, **CANCEL** cancela. Una casilla `Don't ask again in this project` guarda la elección en el proyecto y omite este diálogo después (la opción se almacena como `- Sample save choice:` en el .cct y solo puede cambiarse editando el archivo del proyecto). El diálogo se omite para samples sin slices. **Save** está atenuado (y se omite en navegación) hasta cargar un sample con ruta de archivo. Un error al escribir mantiene el marcador sucio y muestra el error.
  - **Save As** guarda el sample actual en un archivo nuevo: introduce un nombre (prellenado con el actual sin extensión), luego selecciona una carpeta en el navegador. El sample se guarda como `<folder>/<name>.wav`, el instrumento apunta al nuevo archivo, y la carpeta queda guardada como carpeta de sample por defecto. Está atenuado (y se omite en navegación) hasta cargar datos de sample, para poder asignar una ruta a un sample recién cargado. Se rechazan rutas mayores a 255 caracteres.
  - El marcador `*` delante del nombre indica que el sample en RAM difiere del archivo en disco (cualquier operación de proceso lo activa, Save y Save As lo limpian). El marcador es solo de sesión: no se guarda con el proyecto y se reinicia al entrar a la pantalla. Si varios instrumentos usan el mismo WAV, guardar uno sobreescribe el archivo para todos ellos.
- En la pantalla del instrumento Sample, usa **EDIT + [LEFT/RIGHT]** para cargar el WAV anterior o siguiente en la misma carpeta.
- **Pitch** transpone en semitonos (`-48` a `+48`).
- **Loop** selecciona Off, Loop o Ping-Pong.
- **Speed** controla la estiración temporal granular de `0%` a `500%` (`100%` es normal).Los formatos WAV no compatibles muestran un error. Convierte archivos inusuales a WAV `PCM8`, `PCM16` o `PCM24` antes de importarlos.

##### Entrada de audio

Abre **Settings → Audio → Audio Input** antes de grabar desde un micrófono o una interfaz de audio USB. La lista muestra las entradas disponibles en ese momento en el dispositivo. Si se desconecta una interfaz armada, la captura se interrumpe y el gestor de audio se reinicia para que la app siga funcionando. Si estabas grabando, pulsa **STOP** para terminar la toma interrumpida. Tras reconectar, selecciona **[Refresh]** y comprueba que la entrada deseada esté armada antes de volver a grabar; reconectar por sí solo no la rearma de forma fiable. **[List]** vuelve a leer la lista de entradas actual sin reiniciar el audio.

Selecciona la casilla **ARM** de una entrada para ponerla a disposición del grabador del Sampler; su pequeño medidor muestra el nivel entrante. Arma una sola interfaz a la vez: esta versión solo abre la primera interfaz armada, aunque haya varias casillas marcadas. Tu selección se recuerda por nombre de dispositivo, de modo que una interfaz no disponible permanece desarmada en lugar de ser sustituida por otra entrada. Pulsa **OPT** para volver a los ajustes de Audio.

##### Grabación de entrada de audio

En la pantalla de instrumento Sample, **REC** (o **EDIT/REC** cuando hay un sample cargado) abre el editor de samples, que reúne grabación y edición en una pantalla. Arma primero las entradas en la pantalla **Settings → Audio → Audio Input** (ver [Entrada de audio](#entrada-de-audio)). Las filas superiores del editor controlan la grabación:

- **Input** recorre la fuente de grabación: `T1`..`T8` graban las ocho pistas del tracker, seguidas del micrófono o la interfaz USB armados. Para grabar una fuente externa, sigue avanzando más allá de `T8` hasta que aparezca el nombre del dispositivo. `-` significa que no se eligió ninguna fuente explícitamente; **REC** usa entonces la primera entrada armada y muestra su nombre. Mantén **EDIT** y pulsa **LEFT**/**RIGHT** para retroceder/avanzar, o **EDIT+OPT** para volver a `-`. Tocar Input lo hace avanzar directamente.
- El medidor de nivel junto a **Input** muestra el nivel en vivo de la entrada armada (solo fuentes externas). **MON** activa el monitoreo de la entrada: la entrada armada se envía a la salida a -6 dB para reducir el acople. El ajuste se guarda en el archivo de ajustes.
- **REC** inicia la grabación. Las fuentes externas graban de inmediato; una fuente de pista muestra **ARMED** y la grabación empieza cuando arranca la reproducción (pulsa **PLAY**). **STOP** termina la grabación: el WAV se escribe en la carpeta de exportación del proyecto (`recNNN.wav`), se carga en el instrumento Sampler actual y la Region vuelve a ser el sample completo. Salir de la pantalla con **OPT** durante una grabación en curso la finaliza del mismo modo.
- **PLAY** (se muestra tras una grabación) escucha la toma; se convierte en **STOP** mientras suena la vista previa. **CLEAR** (a su lado) borra el archivo WAV y descarga el sample del instrumento; tras borrar, **REC** vuelve a ocupar su lugar.
- La forma de onda muestra la entrada en vivo durante la grabación y la forma de onda completa una vez terminada. La línea de estado muestra el nombre del archivo durante la grabación y el número de frames después; una toma externa muestra además `drop 0` cuando no se perdió ningún frame de audio. Un sample grabado es totalmente editable: región de bucle, selección, slicing, procesado y guardado funcionan igual que en cualquier archivo cargado.

Las grabaciones se escriben a la frecuencia de muestreo de audio seleccionada, como WAV estéreo de 16 bits. Las fuentes de pista capturan lo que renderizan los instrumentos de la pista (tras los FX); las fuentes externas capturan directamente la entrada del dispositivo.

#### 2xSCWF

2xSCWF es un oscilador de forma de onda de ciclo único dual. Carga 1 WAV mono que contenga exactamente 1 periodo en cada oscilador. Ambos osciladores leen hacia adelante y se reinician al final de su ciclo.

Mezcla crossfades A y B.

Detune es fino y exponencial desde unísono hasta `+200 ct`, luego avanza en pasos de semitono de `+3 st` a `+24 st`.

2xSCWF trata cada archivo como una tabla de 1 ciclo y lo lee exactamente una vez por ciclo del oscilador. Esto permite mezclar y desafinar 2 ondas de ciclo único.

La carpeta de fábrica `waveforms/AKWF/` contiene formas de onda de la librería Adventure Kid. Hay muchas más disponibles en línea si deseas ampliar la colección.

#### BYOWTBL

BYOWTBL es un oscilador wavetable dual compatible con tablas Serum. Cada oscilador carga un WAV mono con frames consecutivos de ciclo único. `Pos A` y `Pos B` escanean sus tablas independientemente de `00` a `FF`. El motor interpola linealmente dentro de cada frame y entre frames adyacentes. `Mix` crossfadea los osciladores, y `Detune` desplaza el oscilador B.

### Familia de instrumentos FM

#### Instrumentos nativos OPLL y VRC7

El selector de Tipo de Instrumento incluye un grupo FM con **OPLL / MSX** (YM2413) y **VRC7** (DS1001). Cada uno ofrece ahora **73 programas** en listas deslizantes de Bank / Preset: los 15 tonos originales, 40 tonos adicionales distintos de las tablas YM2413/VRC7/YMF281B de emu2413, y 18 programas de dos operadores creados por ChooChoo. Se omiten bytes de tono duplicados exactos dentro de cada motor. Las paletas adicionales usan el slot de tono programable del chip; no amplían el ROM físico. El programa cero identifica un tono personalizado. EDIT + izquierda/derecha selecciona el preset anterior o siguiente dentro del filtro de banco. Fine ct ajusta la afinación de -100 a +100 centésimas.

En la lista de programas, EDIT + PLAY audiciona el sonido resaltado; suelta los botones para detener. EDIT confirma y OPT cancela. Navegar o audicionar no cambia el instrumento, tabla ni la canción. El gesto normal de audición en la página Instrument continúa funcionando tras la selección. Los envelopes nativos del chip controlan ataque/liberación; la envolvente Amp opcional añade un ADSR por software (ver controles chip nativo más abajo). No hay editor completo de operadores.

#### AdLib / OPL2 y OPL3

FM también incluye AdLib / OPL2 (YM3812) y OPL3 (YMF262). Bank filtra la lista de fábrica; Preset abre grupos por categoría y una vista Todo. EDIT + PLAY audiciona antes de seleccionar, EDIT elige, OPT cancela, y EDIT + izquierda/derecha en Preset carga la entrada previa/siguiente que coincida. Fine ct ajusta la afinación local. Mode muestra 2 operadores, 4 operadores o voz Dual. OPL2 oculta patches incompatibles con OPL3; OPL3 puede reproducir la colección compartida de dos operadores.

Los títulos emergentes de banco y preset identifican el motor activo. Cambiar motor o ranura de instrumento reinicia el filtro de banco a **Todos los bancos**, incluso al alternar entre OPL2 y OPL3 compatibles. Volver de un popup en la misma ranura mantiene el filtro elegido. Cancelar la importación de un banco DX7 restaura el filtro anterior.

La confirmación del preset también publica las ediciones hechas al soltar el botón para el siguiente tick de UI/audio. Las canciones en reproducción ya no necesitan otro pulsado o reinicio de transporte para recibir el parche. La salida sigue el buffer de audio configurado y el comportamiento del envelope/retrigger del motor.

La colección de fábrica contiene 697 entradas fuente de The Fat Man 2-op, The Fat Man 4-op y DMXOPL3, con 589 identidades únicas de patch normalizadas. Los alias conservan nombres y atribución de fuente. Se mantienen niveles nativos de operador, flags de profundidad LFO, offsets de nota, tonos fijos de percusión y afinación de voz dual. El volumen en tracker usa amplitud por software; no reproduce curvas originales MIDI ni políticas de offset por velocidad. Las duraciones de liberación fuente no cortan notas sostenidas. Algunos efectos tienen ataques lentos: mantén presionado Seashore en vez de esperar un click corto para revelarlo.

Seleccionar un preset no cambia la tabla ni inserciones propias del track.

#### Instrumentos nativos Sega PSG y Game Boy

CHIP incluye Sega PSG, GB Pulse y GB Noise. En Sega PSG, **Bass range: Extended** mantiene notas bajas distinguibles. Elige **Chip** para el rango original limitado, donde las notas más bajas convergen. El ruido de tasa fija mantiene su tono original; el ruido derivado de tono sigue el rango de graves seleccionado.

Los instrumentos GB usan registros de pulso/ruido DMG; no se expone el canal de onda Game Boy. Sus listas de presets contienen ahora **24, 24 y 28** presets respectivamente. Las adiciones cubren ruido Sega ligado/periódico, barridos de pitch GB, percusiones breves, ruido metálico, drones y ascensos. Son programas originales, no sonidos extraídos de juegos.

La página ofrece controles nativos de modo/ciclo/anchura, envolvente/barrido/ruido según corresponda, y un ADSR de amplitud por software.

#### DX7 FM

Selecciona **Type → FM → DX7 FM**; DX7 es la séptima opción, después de Arcade / YM2151. DX7 FM usa seis operadores. Su página ofrece Bank, Preset y Fine ct junto con ajustes comunes del instrumento. La navegación de presets usa listas bank/categoría. Mantén EDIT+PLAY para escuchar el sonido resaltado; soltar detiene la audición. Solo EDIT confirma al soltar; OPT cancela. Cargar un sonido copia su patch completo sin cambiar tablas del tracker ni inserciones de track FX.

Para una biblioteca personal persistente, coloca archivos `.syx` en **`instruments/banks/dx7/`** junto a la biblioteca existente. Se admiten subcarpetas. Abre **DX7 → Bank**: cada archivo aparece como un banco nombrado listo para navegar en Preset. Vuelve a abrir Bank tras añadir o eliminar archivos; no se requiere conversión ni paso Load Instrument. Un banco original estándar DX7/TX7 contiene **32 voces**. Un archivo con cuatro mensajes bank aparece como cuatro bancos numerados (128 voces en total). También se aceptan archivos de voz única. El patch seleccionado se guarda con el instrumento, por lo que borrar su banco fuente no cambia la canción.

**Load Instrument** también acepta voces simples y bancos 32 voces originales `.syx` DX7/TX7. Selecciona un patch para usarlo, o cancela para mantener el instrumento actual. Usa la carpeta Bank arriba para biblioteca persistente; guarda la canción o un `.cni` para conservar un patch editado. Se rechazan formatos Yamaha no soportados.

DX7 tiene un límite de **16 notas activas en toda la canción**, incluidas voces de acordes y colas de liberación. Al alcanzarse el límite, primero ceden las colas de liberación, luego notas sostenidas silenciosas, para mantener nuevas notas jugables. La audición de preset no está disponible mientras la canción suena.

#### Editor de operadores FM

Los instrumentos Genesis FM, Arcade FM, OPL2, OPL3, OPLL y VRC7 tienen un botón **EDIT** a la derecha de Fine ct que abre una tabla de parámetros del patch:

- Genesis FM / Arcade FM
- OPL2 / OPL3: en OPL3 el modo (2op, 4op, dual) se puede cambiar aquí.
- OPLL / VRC7: editar un preset ROM lo convierte en «Custom».

Izquierda/Derecha y las teclas de edición habituales cambian valores; el instrumento sigue sonando para oír cambios. La barra de estado describe el parámetro bajo el cursor. OPT o Left+Shift regresa a la pantalla del instrumento. DX7 y SID no tienen editor de operadores.

Genesis FM (YM2612) y Arcade FM (YM2151) usan el mismo navegador Bank/Preset, audición y control de afinación fina que los demás motores FM. Sus envelopes nativos controlan la liberación; cambiar volumen o pitch en tracker no los reactiva.

#### Presets FM de fábrica y USER

Los instrumentos nativos usan **Bank:** para elegir TODO, una colección incluida o USER. **Preset:** elige un sonido dentro de esa selección. Mantén EDIT y pulsa Izquierda o Derecha en cualquier fila para ciclar; pulsar brevemente EDIT abre el selector.

Las colecciones incluidas son paquetes ZIP bajo `instruments/FACTORY/`. Los archivos USER van en `instruments/USER/<engine>/`: `dx7`, `opll`, `vrc7`, `opl2`, `opl3`, `genesis`, `arcade`, `sid`, `sega`, `gb-pulse` o `gb-noise`.

Explora carpetas directamente o coloca ZIPs de presets compatibles allí. Los ZIP se mantienen intactos en disco. Todas estas carpetas aceptan archivos `.cni` compatibles; CNI es el formato portátil de instrumento de ChooChoo/ChipNomad, no un estándar de fabricante. El navegador USER lee también estos formatos fuente, incluso dentro de ZIPs:

| Carpeta USER | Formatos adicionales |
| --- | --- |
| `dx7` | Voces únicas y bancos 32 voces Yamaha DX7/TX7 `.syx` |
| `genesis` | `.tfi`, DefleMask `.dmp`, Furnace `.fui` |
| `arcade` | Bancos VOPM `.opm`, DefleMask `.dmp`, Furnace `.fui` |
| `opl2`, `opl3` | `.wopl`, `.opli`, `.woplx`, `.sbi`, Furnace `.fui` |
| `opll`, `vrc7` | Patches personalizados Furnace `.fui` y selecciones ROM fijas |
| `sega`, `gb-pulse`, `gb-noise` | Furnace `.fui`, DefleMask `.dmp` |
| `sid` | GoatTracker 2 `.ins` (GTI5), Furnace `.fui` |

Un banco se abre en voces compatibles. Archivos o funciones no soportadas informan la razón; voces incompatibles en banco mixto se excluyen pero mantienen números de fuente estables. La carga es transaccional: un preset rechazado no cambia el instrumento actual. Los presets fuente reemplazan la tabla del tracker por una vacía, excepto SysEx DX7 que conserva su tabla. CNI restaura su tabla guardada. Los patches cargados pertenecen a la canción y siguen funcionando tras mover o eliminar el paquete fuente.

Los bancos WOPL pueden contener ranuras sin uso sin flag explícito de vacío. El navegador omite ranuras sin nombre cuyos operadores activos están totalmente atenuados sin ataque. Voces jugables sin nombre permanecen disponibles como `Program N [Bbank]` o `Drum N [Bbank]`, usando el número de programa y banco de origen base cero. Un instrumento OPLI simple sin nombre usa su nombre de archivo. Patches silenciosos con nombre se retienen.

Los archivos Furnace, DefleMask y GoatTracker se importan como instrumentos, no canciones completas. Macros fuente animados, samples y ajustes específicos de reproducción pueden no transferirse. Coloca archivos Game Boy en la carpeta Pulse o Noise correspondiente.

El ciclo de presets USER continúa a través de archivos, carpetas, bancos y packs ZIP en ambas direcciones, envolviendo al final. Reabrir el navegador USER sigue la selección actual. TODO incluye sonidos USER bajo Sin clasificar para motores categorizados; USER queda como opción independiente de nivel superior. Sega PSG y Game Boy usan listas planas TODO/Fábrica y navegador jerárquico USER, sin panel Sin clasificar. Archivos DX7 antiguos en `instruments/banks/dx7/` siguen accesibles en USER.

Un motor muestra solo sus colecciones incluidas compatibles. Factory Presets combina los conjuntos de tonos OPLL/VRC7 incluidos para el motor seleccionado; DX7 une originales ChooChoo y YSE. Las colecciones con nombre descargadas permanecen separadas. Fat Man 2-op está listada en OPL2 y Fat Man 4-op en OPL3. Archivos CNI compatibles OPL2 pueden usarse también en carpetas USER OPL3.

#### Controles chip nativo

Las previsualizaciones de formas de onda de instrumentos nativos se actualizan al redibujar sus controles. Los instrumentos FM pueden superponer su curva de Amp env opcional; SID mantiene su previsualización de forma de onda sin esa superposición exclusiva FM, incluso al navegar bancos y presets.

Los siete motores FM ofrecen **Bright**, **Feedback** y **Amp env**. Bright va de -63 a +63, con cero que preserva el patch; cambia niveles de operadores moduladores manteniendo niveles del portador. Su efecto audible depende del algoritmo. Feedback por defecto es **Preset**, o sobreescribe el feedback nativo con valores 0–7. Estos controles no reescriben los bytes nativos guardados de los operadores.Amp env predeterminado a **Bypass**. Selecciona **ADSR** para añadir ataque, decaimiento, sustain, release y moldear el sonido nativo. A/D/R usan un rango cuadrático compartido de 0–5 segundos; sustain va de silencio a nivel completo. Las envolventes nativas siguen activas, por lo que esta envolvente no puede extender un sonido más allá de su release nativo. La navegación de presets conserva el control FM amp y tono del slot.

Los siguientes FX de frase también aparecen como destinos soportados para Modulación y grabación de movimiento. Los valores en la columna FX son hexadecimales. Los ajustes se aplican a la reproducción sin cambiar el instrumento guardado. Los controles de sweep y envolvente nativos GB se bloquean al siguiente disparo de nota; duty/width y frecuencia de ruido pueden cambiar en vivo.

| Engines | FX | Control |
|---|---|---|
| Native FM | `OL1`–`OL6` | Niveles absolutos de salida del operador: modulador OPLL/VRC7 y OPL `00–3F`; portadora OPLL/VRC7 `00–0F`; Genesis/Arcade `00–7F`; DX7 `00–63` (0–99). Mayor valor significa mayor salida. Solo aparecen operadores soportados. |
| OPLL/VRC7/OPL2/OPL3 | `OAR`, `ODR`, `ORR`, `OSL` | Atenuación de ataque, decaimiento, release y sustain del operador 1, `00–0F`. |
| OPLL/VRC7/OPL2/OPL3/Genesis/Arcade | `OMU 00–0F` | Multiplicador de frecuencia del operador 1. |
| Genesis/Arcade | `LFR` | Velocidad nativa del LFO: Genesis `00–07`, Arcade `00–FF`. |
| Arcade | `LAD`, `LPD 00–7F` | Profundidades separadas de amplitud y tono del LFO. |
| Genesis/Arcade | `LAS`, `LEN` | Sensibilidad nativa de amplitud `00–03` y activación de LFO `00–01`. |
| Genesis/Arcade | `LPS 00–07` | Sensibilidad nativa de tono. |
| SID | `SAT`, `SDE`, `SSU`, `SRL 00–0F` | Ataque, decaimiento, sustain y release nativos. Los tiempos aumentan hacia `0F`; el sustain hacia el nivel máximo. |
| SID | `SPR 01–10` | Frecuencia del socio silencioso de 1× a 16×; afecta modulación en anillo y sincronización dura. |
| Todos native FM | `FBK 00–07` | Retroalimentación nativa absoluta, inicializada desde el instrumento. |
| Todos native FM con Amp env activo; Sega/GB | `EAT`, `EDC`, `ESU`, `ERL`, `ESH 00–FF` | Ataque, decaimiento, sustain, release, moldeado. No habilitan un bypass en el FM amp. |
| Sega PSG | `CMD 00–02`, `CNR 00–03` | Tono / ruido blanco / ruido periódico; tres tasas fijas de ruido o tasa derivada del tono. |
| GB Pulse | `CMD 00–03` | Duty nativo del pulso. |
| GB Noise | `CMD 00–01`, `CND 00–07`, `CNS 00–0D` | Ancho de ruido, divisor de reloj y desplazamiento. |
| GB Pulse | `CSP 00–07`, `CSS 00–07`, `CSD 00–01` | Periodo de sweep, shift, dirección descendente. |
| GB Pulse / Noise | `CEI 00–0F`, `CEP 00–07`, `CED 00–01` | Nivel inicial de la envolvente nativa, periodo, dirección ascendente. |

La selección de FX nativos usa el instrumento en la columna `I` de la fila de frase, o el instrumento activo encontrado mirando hacia atrás si `I` está en blanco. El popup de FX muestra una breve descripción del control, su valor presente y el rango válido en un bloque compacto. Seleccionar un efecto nativo distinto comienza en ese valor; reabrir el mismo efecto conserva el valor editado. Un selección multi-fila resuelve cada fila con su instrumento separado. Las tablas usan el contexto del instrumento. Las ediciones conservan los puntos extremos legales, incluyendo duty, ruido, sweep, ADSR, retroalimentación y niveles de operador. Los valores son hexadecimales: por ejemplo, el máximo `63` en DX7 equivale a decimal 99.

#### Instrumentos SID y FX de frase

Elige **CHIP → SID**. Bank ofrece **ChooChoo SID Originals** y **SIDkit Effects**. Navega, escucha y guarda como los otros presets nativos.

La página de instrumento expone forma de onda/ancho de pulso, modo de filtro/corte/resonancia y controles ADSR nativos. **EDIT**, junto al nombre del preset, abre **SID EDIT**. Wave y PW tienen etiquetas separadas; Filter tiene una fila de modo, luego Cutoff y Reso; etiquetas ADSR al lado de sus valores. Tocar cada campo selecciona ese control exacto. Se conservan los rangos y comportamiento de ajuste fino/gordo existentes.

SID EDIT usa las filas compactas del editor FM, colores del tema, valores hexadecimales y controles EDIT+dirección. OPT o Left+Shift regresa; EDIT+PLAY escucha el instrumento. La página edita el parche almacenado, guardando cambios con instrumento y proyecto.

SID EDIT agrupa forma de onda, ring/sync, ADSR, ancho de pulso, filtro, motion, arpegio, barrido de tono y vibrato. La barra de estado identifica cada control y su rango. Ring y sync usan dos de las cuatro voces SID disponibles, por lo que un acorde SID puede requerir menos notas si alguno está activo.

| FX | Rango | Acción |
| --- | --- | --- |
| `SCP` | `00–FF` | Base de ancho de pulso, escalada a 12-bit nativo; el movimiento de pulso de la receta sigue activo. |
| `SCT` | `00–FF` | Base de cutoff, escalada a corte 11-bit nativo; el movimiento de filtro de la receta sigue activo. |
| `SRN` | `00–0F` | Resonancia nativa. |
| `SWV` | `00–07` | Triángulo, sierra, tri+sierra, pulso, tri+pulso, sierra+pulso, tri+sierra+pulso, ruido. |
| `SFI` | `00–07` | Bits del modo de filtro: 1 paso bajo, 2 paso banda, 4 paso alto; 0 bypass. |
| `SMR` | `00–FF` | Reloj macro de receta, escalado de 1 a 200 Hz. |
| `SRG` | `00–01` | Modulación en anillo de triángulo con socio silencioso. |
| `SSY` | `00–01` | Sincronización de oscilador con socio silencioso. |

Ancho de pulso y cutoff soportan `SLE`. Formas de onda discretas, switches, resonancia y retroalimentación FM mantienen sus pasos nativos útiles; valores más grandes no crearían más estados hardware. `FBK` selecciona feedback `00–07` directamente. Los niveles de operador usan los rangos específicos del engine arriba listados. Brillo y ajustes maestros conservan rango completo de byte.

#### Presets Factory y USER SID y native-chip

Instrumentos native chip usan **Bank:** para elegir ALL, una colección incluida o USER. **Preset:** elige un sonido dentro. Mantén EDIT y presiona Left o Right en cualquier fila para ciclar; un toque corto EDIT abre el selector.

Colecciones incluidas son paquetes ZIP en `instruments/FACTORY/`. Archivos USER van en `instruments/USER/<engine>/`: `dx7`, `opll`, `vrc7`, `opl2`, `opl3`, `genesis`, `arcade`, `sid`, `sega`, `gb-pulse`, o `gb-noise`. Navega carpetas directo o coloca un ZIP con carpetas y presets `.cni` compatibles. DX7 acepta `.syx` voces simples y bancos de 32 voces compatibles, incluso dentro de ZIPs. Los ZIPs permanecen íntegros en disco. No se soporta carga directa WOPL/WOPLX; usa presets CNI compatibles o convierte fuentes soportadas offline primero.

El ciclo de preset USER continúa a través de archivos, carpetas, banks y ZIPs en ambas direcciones, envolviendo en los extremos. Reabrir el navegador USER sigue la selección actual. ALL incluye sonidos USER bajo Unsorted para engines categorizados; USER sigue siendo una opción separada al nivel superior. Sega PSG y Game Boy usan listas planas ALL/Factory y navegador jerárquico USER, sin panel Unsorted. Archivos DX7 antiguos en `instruments/banks/dx7/` permanecen accesibles en USER.

Un engine muestra solo sus colecciones incluidas compatibles. Factory Presets combina los sets de tono OPLL/VRC7 incluidos para el engine seleccionado; DX7 combina los originales ChooChoo y YSE. Las colecciones descargadas permanecen separadas. Fat Man 2-op está listado bajo OPL2 y Fat Man 4-op bajo OPL3. Archivos CNI compatibles OPL2 aún pueden usarse en carpetas USER OPL3.

## 6. Modulación y grabación de movimiento

Cada instrumento puede tener hasta 4 ranuras de modulación. Instrumentos de audio pueden apuntar a **Instrument Pan** o **Track Pan**; ambos usan `00` para izquierda, `80` centro y `FF` derecha.

Los tipos de modulación disponibles son:

- ADSR – clásica envolvente ataque-decaimiento-sustain-release
- AHD – envolvente ataque-hold-decaimiento
- LFO – oscilador de baja frecuencia con varias formas
- syncLFO (antes SLFO) – LFO con un ajuste de ticks por unidad y multiplicador de periodo
- slowLFO – ciclos largos con un control de periodo proporcional, que empieza justo más allá del rango del LFO ordinario
- FLFO – LFO a frecuencia de audio que puede llegar a kHz; FLFO consume mucho CPU
- SLIN – control lineal de joystick
- SRAT – control de joystick basado en velocidad
- Sequence – lee la secuencia del tracker como señal de modulación: **PITCH** es el tono secuenciado final, **VOL** el volumen secuenciado final y **TRANCE GATE** un modulador rítmico independiente. Motif, Rate y Length eligen el patrón del gate; Trigger elige si corre libre, se redispara por nota o se reinicia en los límites de frase o cadena. Los gates cerrados dan cero, los abiertos medio nivel, y Accents reparte gates a nivel completo de forma euclidiana entre los gates activos. No usa ni cambia ningún insert de pista.

La lista de destinos de modulación depende del tipo de instrumento.

Cuando una envolvente ADSR o AHD apunta a Volumen, se vuelve la envolvente de volumen del instrumento. Un LFO apuntando a Volumen desplaza el volumen de salida actual.

La cantidad de modulación puede ser positiva o negativa. Su rango depende del destino. Si el rango completo del destino es `127` o menor, como en la mayoría de parámetros, la cantidad define un rango absoluto. Destinos más amplios como tono usan cantidad escalada.

Las fuentes Sequence PITCH y VOL tienen un **Mode**. **Normal** mantiene audible el valor del tracker y a la vez lo usa como modulación. **Bypass** lo convierte solo en fuente: PITCH redispara en el último tono (o C-4 en la primera nota) y VOL mantiene el último nivel audible. Trance Gate ya es solo fuente. Si algún slot Sequence activo pone PITCH o VOL en bypass, gana el bypass. Para evitar realimentación, PITCH y VOL no pueden apuntar a Pitch o Volume.

Todos los parámetros de duración, incluyendo ataque, hold, decaimiento y periodo, se miden en ticks.

**syncLFO** es el antiguo SLFO: ajusta sus ticks y su multiplicador para un movimiento sincronizado con el tempo. Los ajustes SLFO existentes y las canciones mantienen el mismo timing.

Usa **slowLFO** para barridos más largos de lo que puede hacer el LFO ordinario. Su control Period va de 256 a 131.072 ticks. Todos los LFO basados en ticks siguen el tempo de la canción y comparten las mismas formas, la misma selección de wavetable y los mismos modos de disparo. Elige Free para un barrido que continúa entre notas; Retrig, Phrase y Chain lo reinician en sus límites respectivos.

Formas de LFO:

- Tri – onda triangular bipolar
- Sin – onda seno bipolar
- UniTri – onda triangular unipolar
- UniSin – onda seno unipolar
- RampDn – rampa lineal descendente (onda sierra)
- RampUp – rampa lineal ascendente (onda sierra)
- ExpDn – rampa exponencial descendente (onda sierra exponencial)
- ExpUp – rampa exponencial ascendente (onda sierra exponencial)
- Square – onda cuadrada
- Random – onda aleatoria sample-and-hold
- Wavetable – lee una de las wavetable AY de 32 pasos del proyecto. Valores de `00` a `0F` mapean de `-100%` a `+100%` sin interpolación.

Tipos de disparo LFO:

- Free – reinicia el LFO solo cuando cambia el instrumento
- Retrig – reinicia el LFO en cada nota
- Phrase – reinicia LFO, syncLFO y slowLFO al entrar en una nueva frase
- Chain – reinicia LFO, syncLFO y slowLFO al entrar en una nueva cadena
- Hold – para después de 1 ciclo y mantiene el último valor
- Once – para después de 1 ciclo y vuelve a cero

### Grabación de movimiento

- **STICK LIVE** (`L1` por defecto) aplica modulación de stick sin cambiar la frase. En **Settings > Stick live mode**, `HOLD` (por defecto) la activa mientras se mantiene; `TOGGLE` la activa con una pulsación y la desactiva con la siguiente; `FREE` la mantiene siempre activa. Soltar el botón en `TOGGLE` la deja activa.
- Durante la reproducción, mantén **MOTION RECORD** (`L2`) para aplicar modulación de stick y grabar destinos cambiados como valores FX absolutos.
- Mantén **MOTION ERASE** (`R2`) para eliminar FX destino que coincidan en la fila actual.

El `~` abajo a la derecha indica Stick live activo. Grabación (`*`, o `!` por desborde) y Borrado (`x`) tienen prioridad de indicador y son momentáneos en ambos modos. Ambos habilitan procesado de stick en vivo mientras se mantienen; soltarlos mantiene un toggle live activado por separado.

El modo se guarda con la configuración de la aplicación, pero el toggle live siempre inicia desactivado. Cambiar modo limpia el toggle: `HOLD` sigue cualquier botón Stick live presionado, `TOGGLE` espera nueva pulsación y `FREE` está activo inmediatamente.

La grabación de movimiento escribe FX de pista en la frase que se está reproduciendo. Actualiza primero FX coincidentes, luego usa ranuras vacías de derecha a izquierda. Nunca sobrescribe FX distinto. Si las 3 ranuras están llenas, no se graba movimiento en ese paso. Un `!` en la esquina inferior derecha indica que más destinos cambiaron de los que pueden contener las 3 columnas FX.

La grabación de movimiento soporta destinos Braids, Plaits, Sampler, 2xSCWF, BYOWTBL y Bogie, incluyendo macros Bogie activos y controles de filtro donde aplica.


## 7. Efectos insertados en la pista

Cada una de las ocho pistas tiene dos ranuras en serie, **TF1 → TF2**. Desde Instrumento, usa el gesto Shift+Up habitual para abrir MOD, luego Shift+Up otra vez para **F: Insert FX**. Shift+Down vuelve a MOD. OPT+Left/Right selecciona pista (1–8), sin cambiar el selector de instrumento. El encabezado del módulo abre el selector existente; el bypass conserva el módulo, valores y automatización configurados. Los parámetros son valores byte: columna izquierda 1–4, derecha 5–8. El texto contextual muestra nombres completos y valores decodificados. Proyectos antiguos cargan con ambas ranuras OFF.

La pantalla de navegación mantiene visible la columna completa F–M–I–P en todo momento en las pantallas de instrumento, incluyendo Insert FX y el pool de instrumentos, destacando la pantalla actual.| Módulo | Parámetros en orden |
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

Los controles continuos usan 00–FF, salvo FORMANT VOW (00–7F). Los controles bipolares tienen un neutral exacto en 80. Los modos de Distortion son 00 Density, 01 Drive, 02 Spiral, 03 Mojo, 04 Dyno. Las ratios del Compressor 00–07 son 1.5, 2, 3, 4, 6, 8, 16, 20:1. Compressor Detect 00–02 escucha su propia entrada estéreo, izquierda o derecha; 03–0A escucha en cambio las pistas 1–8. El filtro del detector en 80 es neutral; por debajo es low-pass y por encima high-pass. Una fuente de pista se toma tras su volumen, pan y Tilt EQ pero antes de sus inserts; una fuente silenciada o ausente aporta silencio. Cualquier slot puede contener cualquier módulo; módulos repetidos son independientes. Chorus y Phaser comienzan con modulación audible pero controlada. Reduce Mix, Depth o Feedback para un efecto casi transparente; Depth, Feedback y Mix altos son intencionadamente extremos. Flanger y Rotary usan modulaciones suaves por defecto. La velocidad de Rotary usa el rango completo **00-FF**: **00 = 0.01 Hz** (una rotación cada 100 segundos), **FF = 25 Hz** (el máximo anterior de Chaos). La escala exponencial aporta control fino sobre movimientos LFO lentos. Nuevos insertos Rotary inician en **8F**, alrededor de 0.8 Hz. Esto reemplaza los cuatro valores discretos de velocidad; las configuraciones antiguas y automatización F11/F21 requieren reajuste (Slow/Fast/Hyper/Chaos antiguos corresponden aproximadamente a **8F/CB/E7/FF**). Otros parámetros sin cambios. Bitcrusher varía profundidad de bits entre 4 y 16 y reducción de tasa de 1x a 32x. Modos de Destruction: Fold, Clip y Crush. Saturation y distortion incluyen controles de nivel o mezcla para balancear con la señal cruda.

Trance Gate es un secuenciador de amplitud sincronizado a tempo para pads, leads y stabs. Valores de Pattern 00–3F seleccionan motivos musicales nombrados; 00–1F son grooves básicos y 20–3F variaciones experimentales. Rate ofrece valores rectos, en tripletes y punteados de 1/2 hasta 1/32. Length trunca el motivo de 16 pasos rotado independientemente a 1–16 pasos. Cada note-on en pista reinicia la secuencia; note-off la deja correr en release. Forward, Reverse y Ping-Pong definen la dirección; Chaos usa mutación repetible que preserva densidad cada cuatro loops. En inserto master sigue el tiempo de transporte sin retriggers de nota. Gate ajusta la anchura de cada paso abierto, Smooth redondea bordes conservando un fade mínimo sin clics, y Mix crossfadea entre audio seco y gateado.

**Beads** usa el DSP granular original de Mutable Instruments como inserto estéreo. Elige Beads en un slot de pista. Sus ocho controles usan comandos existentes F11–F18 / F21–F28 y destinos MOD de insert; no se requieren nuevos FX ID ni formatos de proyecto. Los granos corren continuamente sin eventos adicionales.

- **Time:** posición en el buffer de grabación de cuatro segundos.
- **Size:** granos largos reversos en 00, granos más cortos alrededor de 55, luego granos progresivamente más largos hacia adelante. FF activa el modo delay original tras la transición.
- **Density:** granos regulares bajo el centro, temporización aleatoria arriba; la zona muerta central detiene nuevos granos. Al alejarse del centro aumenta la tasa.
- **Pitch:** −24 a +24 semitonos; 80 es unísono exacto.
- **Scatter:** 00 sin aleatoriedad. Aumentarlo aplica aleatoriedad interna uniforme de Time completa y Size a mitad, independiente por grano. Pitch y Shape permanecen fijos.
- **Feedback:** reintroduce granos procesados en el camino de grabación.
- **Shape/Freeze:** 00–7F graba, 80–FF congela. Dentro de cada rango, los siete bits bajos barren la misma envolvente de rectangular a decay y campana hasta reverso. Añadir o restar 80 cambia freeze sin alterar Shape: 40 y C0 tienen la misma envolvente. El valor muestra el estado de la envolvente y grabación/congelación. Mantener EDIT con Arriba/Abajo cambia de 10 hex (16); Izquierda/Derecha cambia de 1. Este control empaquetado es discreto, por lo que no barre suavemente el bit de freeze; modular el byte completo puede cruzar intencionadamente entre estados.
- **Mix:** 00 es seco exacto, FF totalmente procesado. Cambios suavizados.

Freeze mantiene el audio capturado por el efecto; cambiar de módulo o resetear Beads lo limpia. Los envíos de reverb de pista siguen disponibles.

### Hacer que un kick bombee la mezcla u otra pista

Para un master que bombee, pon el kick en la pista 1, selecciona Compressor en MASTER FX, fija Detect en 03 (Track 1), Ratio en 04 (6:1), Attack hacia 60 (unos 1,6 ms), Release hacia C0 (unos 115 ms), Makeup en 00 y Mix en FF. Baja Threshold hasta que los demás sonidos se hundan con cada kick y ajusta después Release para que se recuperen antes del siguiente. El sonido directo del kick conserva sus propios inserts de pista pero esquiva los dos slots de insert master; sus retornos de reverb y delay bombean igualmente.

Para bajar solo un bajo, pad o sintetizador, pon su Compressor en esa pista y selecciona la pista del kick con Detect. El kick de origen sigue audible y su volumen no cambia por el compresor de esa pista. Un kick y un bajo combinados en la misma pista no se pueden separar con este enrutamiento.

### FORMANT

Selecciona **FORMANT** en TF1 o TF2 para moldear un sonido en tonos de tipo vocal. Funciona mejor con una fuente sostenida y rica en armónicos, como una onda de sierra.

Pon **MODE** en `00` VOW y usa **VOW** para barrer de A (`00`) a U (`7F`) pasando por E, I y O. Pon MODE en `01` CUSTOM para afinar tú mismo **F1** y **F2**. **RES** estrecha o ensancha el carácter vocal; **MIX** mezcla el efecto con la señal original. Usa MOD → INSERT FX para mover los controles activos. MODE no es un destino MOD.

### Lane FX para Inserts

Los comandos de insert usan **F11–F18** para TF1 y **F21–F28** para TF2. La selección de FX en el roll y el popup de Phrase/Table muestran solo parámetros usados por los inserts configurados de la pista actual. El popup los agrupa como **TF1: nombre del efecto** y **TF2: nombre del efecto**; slots OFF están ocultos. Inserts bypassed siguen editables. En el popup de FX de Phrase y Table, la descripción sigue el insert slot seleccionado: muestra módulo, nombre de parámetro, extremos del rango decodificado y valor base guardado. Comandos existentes se preservan si un cambio de módulo los deja sin usar; abrir el picker comienza en un control disponible. Al editar un valor, la pista inferior muestra su valor decodificado y unidades. Cualquier columna FX de Phrase o Table puede dirigir a cualquiera de los slots; la grilla Phrase mantiene sus tres columnas. Estos valores absolutos de tiempo en ejecución persisten entre notas, instrumentos, frases, chains y loops normales. Reproducción nueva o parada brusca los resetea. Editar un valor base en la página F solo limpia esa anulación. MOD actúa sobre el valor base/automatizado efectivo sin guardar desplazamientos. Seleccionar un nuevo módulo restaura valores por defecto y limpia anulaciones de ese slot. Bypass mantiene la automatización existente y suspende procesamiento tras un fade corto.

**Las direcciones se refieren a slots y posiciones de parámetros en pista, no a efectos con nombre.** Una Phrase reutilizada en otra pista usa los módulos de esa pista. Cambiar módulos puede cambiar el significado del comando; no se reescriben comandos. Direcciones OFF/no usadas son no-ops seguros y permanecen intactas en datos guardados Phrase/Table.

El selector MOD de instrumento añade una categoría Insert FX. Las fuentes de cada instrumento controlan inserts en la pista que lo reproduce, incluyendo la señal combinada de acorde. El mismo instrumento en dos pistas afecta cadenas independientes. Se aplican reglas existentes de timing de fuente, cantidades firmadas, combinación de fuentes, modos stick y grabación de movimiento. Controles discretos son cuantizados y limitados a opciones válidas. No hay banco de modulación nuevo propiedad de la pista.

Comienza con pocos inserts, observa sobrecargas o crujidos y bypassea efectos que no ayuden en la canción. Drive y ganancia pueden saturar la mezcla, usa el indicador de clip del Mixer mientras los ajustas.

## 8. Tablas

Las tablas son una herramienta central de diseño sonoro en trackers. En términos de Vortex Tracker combinan instrumentos y adornos, pero pueden hacer mucho más. Si conoces LSDj o NerdSEQ, el concepto ya debería ser familiar.

La columna Pitch acepta valores relativos (`~`) o absolutos (`=`) en semitonos. La columna Volume es `00`-`0F`, se aplica a cada engine encima de su envolvente y puede usarse como gate: alterna `0F` y `00` y luego itera con `HOP 00`. Las 4 lanes FX coinciden en gran parte con las de una frase, aunque algunos FX se comportan diferente en tablas.

Colocar un FX `TIC` en la última fila de tabla establece la velocidad de esa columna y sobreescribe la velocidad predeterminada del instrumento. Cada columna FX puede funcionar a una velocidad de ticks distinta. Pitch y Volume siguen la velocidad de la primera columna FX.

Una tabla de 16 filas puede parecer corta frente a un instrumento largo de Vortex Tracker, pero el FX `HOP` crea bucles condicionales como “repetir estas filas 5 veces” y bucles anidados. Con bucles y velocidad independiente por columnas FX, esas 16 filas pueden rendir mucho.

Es posible copiar/pegar lane FX entre frases y tablas ;)

### Modo Retrigger

El campo `Retrig` en la parte superior de la pantalla Table selecciona cuándo reiniciar una tabla activa. `Inst` es el modo por defecto y compatible: una fila de frase con instrumento selecciona la tabla por defecto de ese instrumento y la inicia en la fila `00`. `Phrase` reinicia la tabla activa al entrar en una frase, `Chain` al entrar en un chain, y `Free` la deja correr hasta que la pista detenga o un comando explícito la reemplace.

Un modo estructural nunca retrocede en el tiempo. Si no hay tabla de instrumento activa, la primera fila de frase que introduce un instrumento activa su tabla en esa fila exacta, sea fila `4`, `8` o `F` de la frase. Instrumentos posteriores no reemplazan tablas activas `Phrase`, `Chain` o `Free`. Esto hace que las tablas sean útiles como automatización tipo LFO sin simular silenciosamente pasos previos de frase.

`TBL`, `TBX` y `RET` son siempre acciones explícitas: seleccionan o reinician inmediatamente, cualquiera sea el modo. En un límite Phrase o Chain, se reinicia antes de leer la nueva fila, así que un `TBL` o `TBX` en dicha fila tiene la última palabra. Los FX en fila 00 de tabla se ejecutan de nuevo normalmente; FX acumulativos mantienen su acumulación habitual.

Las tablas `00-7F` son para tablas por defecto de instrumentos. Las tablas `80-FE` son para tablas auxiliares iniciadas por el efecto `TBX`. El rango inferior también funciona para tablas auxiliares, pero usarlo así puede generar conflictos o confusión.

### Controles

- **EDIT + [ARRIBA/ABAJO]** en columna de nombre FX: abre pantalla de selección de FX
- **OPT + [DIRECCIÓN]**: moverse entre tablas

## 9. Pool de Instrumentos y Wavetable AY

### Pool de Instrumentos

El Pool de Instrumentos muestra todos los instrumentos del proyecto y permite reordenarlos. Durante la reproducción también indica qué instrumentos están activos.

#### Controles

- **EDIT**: edita instrumento y salta a pantalla Instrument
- **SHIFT + OPT**: copia instrumento
- **SHIFT + EDIT**: pega instrumento
- **EDIT + [ARRIBA/ABAJO]**: reordena instrumentos
- **EDIT + PLAY**: previsualiza instrumento

### Wavetable AY

Wavetable es un tipo de oscilador software AY Plus. Un proyecto puede contener hasta 256 wavetables, cada una con 32 pasos. Todos los instrumentos wavetable comparten ese conjunto de ondas. Aquí se editan.

Los niveles de volumen AY son no lineales, por eso la vista Standard dibuja la forma de onda coincidiendo con niveles de salida reales. Doble toque en **OPT** alterna entre vista Standard y **LFO view**, usando el mismo atajo que Song/Live Mode: LFO view dibuja linealmente los pasos crus `0`–`15`, con la línea central entre `7` y `8` como cero de modulación.

La pantalla tiene 2 filas lógicas. La fila superior contiene botones **Load** y **Save**, la segunda el editor de wavetable.

#### Controles

- **OPT + [IZQUIERDA/DERECHA]**: navegar en lista de wavetables
- **OPT + [ARRIBA/ABAJO]**: navegar lista saltando de 16 en 16 ondas
- **SHIFT + OPT**: copiar wavetable
- **SHIFT + EDIT**: pegar wavetable
- **OPT + PLAY**: alternar vista Standard y LFO

## 10. Tracker FX

Los FX de pista fuerzan valores de secuenciador o motor sonoro en pasos individuales. En otros grooveboxes se llaman “bloqueos de parámetro”. Cada fila de Phrase tiene 3 slots FX.### FX del Secuenciador

| FX | Valor | Comportamiento detallado |
|---|---|---|
| `ARP` | `XY` | Arpegia la nota base, `+X` pasos y `+Y` pasos. `37` produce un patrón de acorde menor. |
| `ARC` | `XY` | `X` selecciona la dirección del arpegio o modo de rango; `Y` es su velocidad en ticks. |
| `PVB` | `XY` | Vibrato de tono: `X` es velocidad y `Y` profundidad. En modo Lineal, la profundidad usa pasos de `10-cent`. |
| `PBN` | signed `XX` | Añade `XX` unidades de tono cada fila de frase/tabla; `FF` significa `-1`. Usa `00` para detener. |
| `PSL` | `XX` ticks | Desliza desde el tono anterior a la nueva nota durante `XX` ticks. |
| `PIT` | signed `XX` | Desplazamiento relativo acumulado en pasos de tabla de tonos. |
| `FIN` | signed `XX` | Desplazamiento fino acumulado en centésimas con tono Lineal, en unidades de periodo de otro modo. |
| `PRD` | signed `XX` | Desplazamiento relativo acumulado del periodo del oscilador. |
| `VOL` | signed `XX` | Desplazamiento relativo acumulado de volumen (`FF` es -1, no volumen completo). |
| `VSL` | signed `XX` | Añade `XX` a volumen en cada fila de frase/tabla. Usa `00` para detener. |
| `PAN` | `00-FF` | Panorama absoluto del instrumento: izquierda, centro (`80`), derecha. |
| `TPN` | `00-FF` | Panorama absoluto del canal en el mixer: izquierda, centro (`80`), derecha. |
| `RET` | `XY` | Reactiva cada `Y` ticks; `X` aplica cambio de volumen. `Y=0` detiene reactivación. |
| `DEL` | `XX` ticks | Retrasa un note-on. Un retraso más largo que el paso actual del groove ignora la nota. |
| `OFF` | `XX` ticks | Envía note-off tras `XX` ticks y entra en etapa de release ADSR. |
| `KIL` | `XX` ticks | Mata la voz radicalmente tras `XX` ticks sin ejecutar release ADSR. |
| `TIC` | `XX` ticks | Define ticks de tabla por fila. En tabla cambia la velocidad de esa columna FX. |
| `TBL` | `00-FE`, `FF` apagado | Sustituye y reinicia la tabla del instrumento inmediatamente; `FF` la detiene. |
| `TBX` | `00-FE`, `FF` apagado | Inicia o sustituye una tabla auxiliar inmediatamente; `FF` la detiene. |
| `THO` | fila `XX` | Salta todas las columnas de tabla de instrumento a la fila `XX`. |
| `TXH` | fila `XX` | Salta todas las columnas de tabla auxiliar a la fila `XX`; no se usa desde dentro de una tabla. |
| `GRV` | groove `XX` | Selecciona un groove para el track actual. |
| `GGR` | groove `XX` | Selecciona un groove para todos los tracks. |
| `HOP` | `XY` | Salta a la fila `Y`, `X` veces; `X=0` repite indefinidamente. HOP en tabla afecta su propia columna. |
| `SNG` | signed `XX` | Mueve la reproducción `XX` filas de canción. |
| `PRO` | `00-64` | Evalúa una probabilidad absoluta de activación de `0%` a `100%`. |
| `MOD` | `AB` | Activa en paso `A` de un ciclo de `B` pasos. |
| `RNN` | `MX` | Toca una nota aleatoria. `M` elige el modo: `0` siempre aleatorio, `1` aleatorio por PLAY, `2` elige y mantiene hasta que cambie el valor. `X` elige el rango: `0` cualquier nota, `1` +3 st, `2` +6 st, `3` +12 st, `4` -3 st, `5` -6 st, `6` -12 st, `7` +/-3 st, `8` +/-6 st, `9` +/-12 st. Las elecciones aleatorias siguen la escala del proyecto cuando el motor de escala se aplica a la pista. |
| `SPD` | signed `XX` | Selecciona una razón de reloj persistente por track; ver la tabla de velocidades abajo. |
| `SLE` | `00-FF` ticks | Define tiempo de glide persistente por track para FX continuos del engine. `00` es inmediato. Se reinicia a `00` al parar la reproducción. |

### FX de envío de audio

| FX | Valor | Comportamiento detallado |
|---|---|---|
| `RSN` | `00-FF` | Define el envío a reverb de este track hasta la próxima activación de nota. |
| `DSN` | `00-FF` | Define el envío a delay de este track hasta la próxima activación de nota. |

### FX ADSR / Trigger

| FX | Valor | Comportamiento detallado |
|---|---|---|
| `EAT` / `EDC` / `ESU` / `ERL` / `ESH` | `00-FF` | Sobrescribe ataque, decay, sustain, release o forma del envolvente. |
| `TDC` / `TCL` | `00-FF` | Sobrescribe Decay o Color en modo Trigger en Plaits y Plaits-Alt. |

#### Condiciones

Inspirado en las "condiciones trig" suecas, el tracker soporta probabilidades de activación y condiciones módulo.

| FX | Valor | Significado |
|---|---|---|
| `PRO` | `00-64` | Probabilidad absoluta de activación entre `0%` y `100%` |
| `MOD` | `AB` | Activa en iteración `A` de `B`; por ejemplo `12`, `22`, `14`, `34` |

Los contadores `MOD` son locales al track y a la frase. Combinaciones inválidas, como `A` mayor que `B`, no activan.

`MOD34` activa en el 3er paso de cada ciclo de 4 pasos. `MOD1F` activa en el 1er paso de cada ciclo de 16 pasos.

#### Velocidad de reproducción

Como NerdSEQ, ChooChooTracker soporta velocidad de reproducción independiente por cada track.

`SPD` interpreta su valor como byte con signo. `00` velocidad normal, valores positivos aceleran el track, negativos lo hacen más lento. El valor se mantiene activo hasta que otro `SPD` lo cambia.

| Valor | Velocidad |
|---|---:|
| `00` | `x1` |
| `01` | `x2` |
| `02` | `x3` |
| `7F` | `x128` |
| `FF` | `/2` |
| `FE` | `/3` |
| `80` | `/129` |

Proyectos antiguos sin soporte a `SPD` con signo mantienen su mapeo original `00-10`, por lo que siguen sonando igual que guardados.

Multiplicadores muy altos pueden superar la resolución del scheduler de ticks y no han sido testeados en hardware.

### FX de escala y acordes

#### Automatización de escala

`SCL XY` cambia la escala global de reproducción y la tónica sin cambiar las notas almacenadas en frases. `X` selecciona una escala (`0` Cromática, `1` Mayor, `2` Menor, hasta `C` Personalizada); `Y` selecciona la tónica (`0` Do hasta `B` Si). El comando afecta solo FX de frase, no tablas. Si varias pistas emiten `SCL` en el mismo tick, gana la de número más bajo.

`SCL` pertenece al modo Cuantizador. Con **Note Lock** activo el comando no aparece en la lista de FX, y cualquier `SCL` ya escrito en frases se ignora mientras el modo esté activado.

#### Acordes

`CRD XY` convierte la nota en su fila de frase en un acorde. `Y` selecciona: `0` Mayor, `1` Menor, `2` Disminuido, `3` Aumentado, `4` Sus2, `5` Sus4, `6` Power, `7` Maj7, `8` Min7, `9` Dom7, `A` Min7b5, `B` Dim7, `C` Add9, `D` MinAdd9, `E` Maj9, o `F` Min9. `X` selecciona una de 16 inversiones: `0-3` son un ciclo cerrado ascendente; `4-7` añaden drop-2; `8-B` añaden drop-3; `C-F` combinan ambos drops. El ciclo continúa una octava más para triadas y power chords, por lo que cada valor es distinto; los valores más altos son progresivamente más abiertos y menos convencionales. Por ejemplo, `CRD 4Y` es drop-2 en posición fundamental y `CRD DY` es primera inversión con ambos drops. En los extremos de la tabla de tonos, las voces se restringen al rango. El comando no es persistente: debe escribirse en cada fila con acorde. Cada nota generada se cuantiza de forma independiente cuando la Escala está activa, tanto en modos Cuantizador como Note Lock. Disponible solo en engines software; instrumentos AY lo ignoran y siguen monofónicos.

### FX de modulación

Los FX de frase y tabla pueden cambiar una ranura de modulación sin editar el instrumento:

| Patrón FX | Significado |
|---|---|
| `M1A`, `M2A`, `M3A`, `M4A` | Desplazamiento relativo de Amount para ranuras de modulación 1-4 |
| `M11`, `M12`, `M13`, `M14` | Desplazamientos relativos P1-P4 para ranura de modulación 1 |
| `M21`, `M22`, `M23`, `M24` | Desplazamientos relativos P1-P4 para ranura de modulación 2 |
| `M31`, `M32`, `M33`, `M34` | Desplazamientos relativos P1-P4 para ranura de modulación 3 |
| `M41`, `M42`, `M43`, `M44` | Desplazamientos relativos P1-P4 para ranura de modulación 4 |

El valor se interpreta como un cambio relativo con signo de 8 bits (`01` suma `1`, `FF` resta `1`). Comandos repetidos se acumulan. Los valores efectivos se limitan a su rango válido.

`P5` no está expuesto como FX de frase o tabla. Está disponible como destino de modulación `M1–M4 Wavetable`, para que un modulador pueda barrer el índice de AYWavetable de otro.

| Tipo de modulación | P1 | P2 | P3 | P4 | P5 cuando la forma es Wavetable |
|---|---|---|---|---|---|
| ADSR | Ataque | Decay | Sustain | Release | Sin usar |
| AHD | Ataque | Hold | Decay | Sin usar | Sin usar |
| LFO | Forma | Modo de activación | Periodo | Sin usar | Índice AY wavetable |
| syncLFO | Forma | Modo de activación | Ticks | Multiplicador | Índice AY wavetable |
| slowLFO | Forma | Modo de activación | Paso de periodo (00–FF) | - | Índice AY wavetable |
| FLFO | Forma | Modo de activación | Frecuencia (`1 Hz` a `20 kHz`) | Sin usar | Índice AY wavetable |

### FX Braids

| FX | Valor | Significado |
|---|---|---|
| `BMD` | `00-2E` | Modelo Braids |
| `BTM` | `00-FF` | Timbre absoluto normalizado |
| `BCL` | `00-FF` | Color absoluto normalizado |
| `BCF` | `00-FF` | Corte exponencial, `20 Hz` a `20 kHz` |
| `BRS` | `00-FF` | Resonancia del filtro exponencial |

### FX aChChid

| FX | Valor | Significado |
|---|---|---|
| `ASL` | `00-FF` | Desliza hacia la nota sin retrigger. `00` es `60 ms`; valores mayores extienden el glide. |
| `ADC` | `00-FF` | Sobrescribe decay: `200 ms` a `2 s`. |
| `AAC` | `00-FF` | Cantidad de acento: ninguno a máximo. |
| `ATM` | `00-FF` | Sobrescribe Timbre de Braids; activo solo en modo onda Braids. |
| `ACL` | `00-FF` | Sobrescribe Color de Braids; activo solo en modo onda Braids. |
| `ACF` | `00-FF` | Corte exponencial filtro 303, `20 Hz` a `20 kHz`. |
| `ARS` | `00-FF` | Resonancia filtro 303, ningún efecto a máximo. |
| `AEM` | `00-FF` | Modulación envolvente filtro 303, ningún efecto a máximo. |

### FX Bogie

| FX | Valor | Significado |
|---|---|---|
| `DMD` | `00-0B` | Selecciona modelo Bogie hasta el siguiente trigger. |
| `DDC`, `DTO`, `DSW`, `DNO`, `DFM`, `DDR` | `00-FF` | Sobrescribe Decay, Tone, Sweep, Noise, FM o Drive hasta el siguiente trigger. Los seis macros están disponibles para todos los modelos. |
| `DCF` | `00-FF` | Corte del filtro, mapeado logarítmicamente. |
| `DRS` | `00-FF` | Resonancia del filtro. |

### FX Sintered

| FX | Valor | Significado |
|---|---|---|
| `SMD` | `00-05` | Selecciona modelo Sintered hasta el siguiente trigger. |
| `SDC`, `SMP`, `SMA`, `SMB`, `SMO`, `SMC` | `00-FF` | Sobrescribe Decay, Mod, los dos primeros macros del modelo, Motion o el macro final hasta el siguiente trigger. |
| `SCF`, `SRS` | `00-FF` | Corte y resonancia del filtro. |

### FX de salida MIDI

| FX | Valor | Significado |
|---|---|---|
| `MC1`-`MC4` | `00-FF` | Envía un CC MIDI, reescalado a `0-127`. El número de CC que envía cada ranura se ajusta por instrumento (`MC1 number`-`MC4 number` en la pantalla del instrumento), no por el FX mismo – una ranura sin número es inerte. |

### FX Plaits

| FX | Valor | Significado |
|---|---|---|
| `PMD` | `00-17` | Motor Plaits |
| `PHA` | `00-FF` | Armónicos absolutos normalizados |
| `PTM` | `00-FF` | Timbre absoluto normalizado |
| `PMO` | `00-FF` | Morph absoluto normalizado |
| `PAX` | `00-FF` | Mezcla Main/Aux: `00` Main, `FF` Aux |
| `PCF` | `00-FF` | Corte exponencial, `20 Hz` a `20 kHz` |
| `PRS` | `00-FF` | Resonancia de filtro exponencial |

### FX Sample

| FX | Valor | Significado |
|---|---|---|
| `SPL` | `00-03` | Modo de reproducción: `00` Adelante, `01` Reversa, `02` Loop, `03` Ping-Pong |
| `SLI` | `00-FF` | Reproduce el slice numerado (`01` = primer slice) sin importar la nota; `00` respeta el mapeo normal de notas. Requiere slice mode activado en el instrumento |
| `SPT` | signed `XX` | Transposición de sample en semitonos |
| `SST` | `00-FF` | Inicio de reproducción normalizado |
| `STA` | `00-FF` | Alias de `SST` (inicio de reproducción normalizado) |
| `SEN` | `00-FF` | Fin de reproducción normalizado |
| `SVL` | `00-FF` | Volumen absoluto del sample |
| `SCF` | `00-FF` | Corte exponencial, `20 Hz` a `20 kHz` |
| `SRS` | `00-FF` | Resonancia filtro exponencial |
| `SSP` | `00-FF` | Velocidad del sample, mapeada de `0%` a `500%` |
| `RNS` | `M0` | Toca un slice aleatorio de un sample cortado en lugar del asignado a la nota. `M` elige el modo: `0` siempre aleatorio, `1` aleatorio por PLAY, `2` elige y mantiene hasta que cambie el valor. Requiere el modo slice activado en el instrumento; el nibble bajo no se usa. |

### FX AY compartidos por instrumentos AY

| FX | Valor | Comportamiento detallado |
|---|---|---|
| `AYM` | `XY` | `X` es la forma del envolvente; `Y` selecciona apagado/tono/ruido/tono+ruido (`0/1/2/3`). |
| `NOI` | signed `XX` | Desplazamiento relativo acumulado del periodo del ruido. |
| `NOA` | `00-1F`, `FF` | Periodo de ruido absoluto; `FF` da prioridad al periodo de ruido de pistas anteriores. |
| `ERT` | cualquier | Reactiva la forma de envolvente hardware actual. |
| `EAU` | `XY` | Ratio automática de envolvente `X:Y`; `X=0` la desactiva. |

### FX solo AY Classic

| FX | Valor | Comportamiento detallado |
|---|---|---|
| `EVB` | `XY` | Vibrato de periodo de envolvente: `X` velocidad, `Y` profundidad. |
| `EBN` | signed `XX` | Suma `XX` al periodo de envolvente cada fila de frase/tabla. |
| `ESL` | `XX` ticks | Desliza desde el periodo de envolvente anterior al nuevo valor. |
| `ENT` | nota `XX` | Define periodo de envolvente desde la nota de tabla de tonos mostrada por la UI. |
| `EPT` | signed `XX` | Desplazamiento relativo acumulado del periodo de envolvente. |
| `EPL` | byte `XX` | Define el byte bajo del periodo de envolvente. |
| `EPH` | byte `XX` | Define el byte alto del periodo de envolvente. |

### FX AY Plus| FX | Valor | Comportamiento detallado |
|---|---|---|
| `TNN` | nota `XX` | Ajusta el oscilador de tono a una nota de la tabla de tonos, ignorando la nota de la fila. |
| `TNP` / `TNF` | `XX` con signo | Desplazamiento acumulado de paso de tono / corrección fina para el tono. |
| `TRT` | cualquiera | Reinicia la fase del oscilador de tono donde sea soportado. |
| `ENN` | nota `XX` | Ajusta el oscilador de envolvente a una nota de la tabla de tonos. |
| `ENP` / `ENF` | `XX` con signo | Desplazamiento acumulado de paso de tono / corrección fina para la envolvente. |
| `SFT` | `00-07` | Tipo de software: Ninguno, Pulso, Sync Tone, Sync Env, Tabla de ondas, Tone FM, Env FM, Sample. |
| `SFN` | nota `XX` | Ajusta el oscilador de software a una nota de la tabla de tonos. |
| `SFP` / `SFF` | `XX` con signo | Desplazamiento acumulado de paso de tono / corrección fina para el oscilador de software. |
| `SRT` | cualquiera | Reinicia la fase del oscilador de software. |
| `SFM` | `XX` con signo | Desplazamiento acumulado de profundidad FM. |
| `PWM` | `XX` con signo | Desplazamiento acumulado del ancho de pulso. |
| `SPL` | `XX` con signo | Desplazamiento acumulado del nivel bajo del pulso. |
| `SWT` | `XX` con signo | Desplazamiento acumulado del índice de tabla de ondas. |

### FX Sample AY

AY Sample acepta los comandos AY compartidos más `TNN`, `TNP`, `TNF`, `TRT`, `SFN`, `SFP` y `SFF` tal como se describió arriba.

| FX | Valor | Comportamiento detallado |
|---|---|---|
| `SMS` | `00-FF` | Define el inicio de reproducción del Sample AY legado a `XX x 64` muestras fuente. |

Los parámetros FX del motor se aplican al tipo de instrumento correspondiente y se reinician en la siguiente activación de nota, salvo que se indique lo contrario. `SPD` es deliberadamente persistente.

El selector FX filtra esta referencia a comandos comunes más el grupo soportado por el instrumento activo. El panel de ayuda contextual muestra detalles de valores mientras se edita.

## 11. Mixer, Reverb y Delay

El Mixer es la pantalla principal más a la izquierda. Muestra la carga de CPU y avisos de clipping. Un `!` rojo en la columna **CLIP** señala la pista cuyo sonido seco hizo que la mezcla sobrepasara el rango seguro.

Úsalo para balancear las 8 pistas, moldear cada pista con Tilt EQ y enviar audio a la reverberación o delay:

| Campo | Significado |
|---|---|
| LVL | Nivel post motor de la pista, `000-100` |
| REV | Envío a la reverberación compartida, `000-100` |
| DLY | Envío al delay ping-pong compartido, `000-100` |
| PAN | Desplaza el audio a la izquierda o derecha |
| TLT | Tilt EQ por pista, `00-FF`; `80` es neutro, valores bajos favorecen graves y valores altos agudos. Valores bajos generan un suave overdrive. |
| M | `*` silencia esta pista |
| S | `*` solo activa esta pista |

El mixer es por pista, no por instrumento. Si una pista cambia de instrumento, su nivel, PAN, Tilt y envíos permanecen atados a la pista. Los instrumentos además tienen su propio **Vol** y **Pan** `00-FF` en la fila superior de la pantalla Instrument. El PAN del instrumento se aplica primero, luego el PAN de pista. Ambos preservan una imagen estéreo en el centro; atenuando progresivamente el lado opuesto al moverlos.

### Auto Mix

El Mixer contiene un botón **AUTO MIX**. Renderiza `6 segundos` sin conexión, equilibra el volumen de las pistas, compara después 8 bandas de octava con un perfil de ruido rosa y baja suavemente las pistas que saturan esas bandas. Propone cambios de nivel prudentes, deja algo de margen de pico e inicia una vista previa temporal. **Apply** conserva los niveles propuestos, mientras que **Cancel** restaura los 8 niveles anteriores. Es un buen punto de partida, pero probablemente querrás retocar el resultado.

### Reverberación

Usa **SELECT + [UP]** desde Mixer para abrir la pantalla de ajustes de reverb.

- **Algorithm**: selecciona Clouds o Beads. Cada algoritmo mantiene sus controles propios.
- **Return**: nivel de reverb húmeda en la mezcla master.
- **Clouds**: **Time**, **Damping**, más filtros pre-reverb **LP** y **HP**.
- **Beads**: **Time**, filtro de retroalimentación nativo **Tone**, y filtro pre-reverb **HP**.

Esta es solo la sección de envío de reverb de Clouds o Beads, no el procesador granular.

### Delay ping-pong

Usa **SELECT + [DOWN]** desde Mixer para abrir Delay, luego **SELECT + [UP]** para volver. La reverb funciona en dirección opuesta: **SELECT + [UP]** la abre y **SELECT + [DOWN]** vuelve al Mixer.

- **Return**: nivel de delay húmedo.
- **To Reverb**: cantidad de señal húmeda de delay enviada a la reverb compartida, `0-100%`.
- **Ticks**: tiempo de delay en ticks del tracker (`9 ticks` = `1 beat` por defecto).
- **Feedback**: cantidad de retroalimentación cruzada, limitada a `95%`.
- **Filter**: filtro pasa-bajo en la señal retardada.

La primera repetición sigue la entrada estéreo. Los siguientes feedbacks cruzan entre canal izquierdo y derecho.

## 12. Pantalla de proyecto

La pantalla de proyecto provee los comandos **Load**, **Save**, **New**, **Export**, **Manage** y **Scale**, junto con metadatos de nombre de archivo, título y autor.

### Ubicación de exportación

Las exportaciones, stems y bounces se escriben en una carpeta por proyecto dentro del directorio de samples preexistente de la app:

- **Por defecto**: `<carpeta app>/samples/Exports/<nombre proyecto>/`. Mientras el proyecto no tiene nombre, la carpeta se llama `current-project`.
- En la versión web el defecto es `/user/samples/Exports/<nombre proyecto>/`.
- En Android la carpeta de samples está dentro del área de trabajo de la app: privada para la app en la versión Play Store, `samples` en la carpeta que seleccionaste en la compilación directa APK.

La fila **Folder** en la pantalla EXPORT muestra el destino activo:

- Toca **EDIT** para elegir una carpeta personalizada con el navegador de carpetas. La carpeta elegida se usa tal cual (no se añade subcarpeta de proyecto) y se recuerda en la configuración.
- **EDIT + OPT** (borrar) restablece la ubicación por defecto.

Cuando un proyecto con nombre se guarda con uno nuevo, la carpeta de exportación por defecto se renombra para coincidir, manteniendo los archivos exportados previamente. Renombrar no afecta carpetas personalizadas, que tampoco son renombradas por guardado automático. Si ya existe una carpeta con el nombre objetivo, ambas se mantienen. Si el tracker perdió la carpeta (por ejemplo tras reinicio), guardar un proyecto con nombre renombra la carpeta `current-project`. Los instrumentos siguen funcionando tras renombrar: las rutas de muestras cargadas apuntando a la carpeta renombrada se actualizan automáticamente.

**Load** acepta archivos `.mid`/`.midi`, importados como un proyecto nuevo: las notas se agrupan por canal MIDI (un canal por pista, hasta el límite de pistas), cuantizan a 4 filas por beat y se colocan en un instrumento AY por defecto – los números de programa MIDI no tienen equivalente chiptune, elige instrumentos reales luego. Solo se usa el primer tempo del archivo (una velocidad global de ticks, sin cambios por sección). La fila **MIDI** en Export hace el reverso: escribe notas, volumen y tempo/groove actuales como un archivo MIDI Estándar (una pista MIDI por pista del tracker); los instrumentos [MIDI Out](#midi-out) y otros FX por fila más allá de volumen y groove global no tienen equivalente MIDI y no se traducen.

**Load** acepta canciones Dirtywave M8 (`.m8s`, firmware 2.x-4.x), importadas como nuevo proyecto. Solo se conserva estructura y notas (filas de canción, chains con transpose, frases con nota, velocity e instrumento, note-offs, tempo y nombre); las notas mantienen su tono real (el M8 nombra octavas dos más altas: su `C-6` es aquí `C-4`). Cada instrumento usado por frase se importa bajo su nombre M8. Instrumentos **MACROSYN** M8 se convierten en Braids (los 47 shapes son modelos Braids, mismos números; timbre, color, tipo de filtro, cutoff, resonancia y paneo se transfieren, la curva de cutoff es aproximada). Instrumentos **SAMPLER** M8 pasan a Sample: `.m8s` solo almacena camino del WAV en la tarjeta M8, así se busca en la carpeta del `.m8s` y sus padres (una copia de la tarjeta funciona) y en la carpeta actual de samples (`Samples` o `samples`); si no se encuentra se mantiene la ruta M8 y queda silencioso. Las muestras importadas tienen +24 semitonos en Pitch porque M8 reproduce la muestra dos octavas más bajas que esta app. **FMSYNTH** pasa a instrumento Genesis FM (aproximación), **WAVSYNTH** y **HYPERSYNTH** pasan a aChChid. **MIDI OUT** pasa a instrumento MIDI Out (canal, bank, programa y primeros cuatro CC). Otros tipos (externos) se convierten en instrumento AY por defecto - elige sonidos reales luego. FX y tablas se ignoran.

**Load** acepta canciones Little Sound Dj (`.lsdsng`, una canción por archivo), importadas como proyecto nuevo. Elegir un save LSDj completo (`.sav`, hasta 32 canciones) abre una lista de sus canciones (slot, nombre, versión, tamaño); selecciona una para importar (la canción abierta cuando se guardó el cartucho no aparece). Solo se conservan estructura y notas (filas, chains con transpose, frases con nota e instrumento, tempo y título); cada nota obtiene volumen completo porque LSDj no maneja volumen por nota, y los cuatro canales LSDj PU1, PU2, WAV y NOI pasan a pistas 1-4. Instrumentos de pulso pasan a GB Pulse (duty, envolvente y paneo se transfieren; la variación de pulso no), instrumentos de ruido a GB Noise (envolvente, estabilidad y paneo), de onda a preset GB Pulse Hollow Lead y kits a GB Noise por defecto. Se importan grooves, y comandos de frase `C` (chord), `D` (delay), `E` (envolvente pulso y ruido, no onda), `G` (groove), `K` (kill) y `V` (vibrato) se traducen a `ARP`, `DEL`, `CEI`/`CEP`/`CED`, `GGR`, `KIL` y `PVB`. Otros comandos (`A`, `F`, `H`, `L`, `M`, `O`, `P`, `R`, `S`, `T`, `W`, `Z`), tablas, frames de onda, samples de kits y ajustes synth se ignoran, por lo que canciones que los usen suenan distinto. El tempo se ajusta para groove de 6 ticks.

En escritorio y Android, la fila **LSDJ** en Export hace el reverso limitado: tócalo y elige un `.lsdsng` existente como **template** (recordado hasta salir, **EDIT + OPT** olvida). Sus filas, chains, frases, tempo y título se reemplazan y todo lo demás (tablas, grooves, ondas, kits, ajustes synth) se conserva. Las pistas 1-4 vuelven a PU1, PU2, WAV y NOI; GB Pulse y GB Noise se escriben como instrumentos LSDj de pulso y ruido, otros tipos como instrumento pulso por defecto. FX, volúmenes y note-offs no se exportan; canciones con chain `80` o más o frase `FF` o más no pueden exportarse (LSDj tiene 128 chains y 255 frases). El archivo exportado no ha sido probado en LSDj aún.

En escritorio, la fila **M8S** en Export hace el reverso limitado: tócalo y elige un `.m8s` existente como **template** (recordado hasta salir, **EDIT + OPT** olvida). Su canción, chains, frases, tempo y título se reemplazan y todo lo demás se conserva, excepto que instrumentos Braids se escriben como MacroSynth y Sample como Sampler (ruta `/Samples/<nombre archivo>`: copia el WAV ahí en el M8 tú mismo), si el template tiene instrumento de ese tipo para copiar parámetros. FX no se exportan; canciones con frase `FF` o más no pueden exportarse (M8 tiene 255 frases). El archivo exportado no ha sido probado en un M8 real. Consulta [M8 songs](m8s-format.md) para detalles.

En escritorio, key jazz permite escribir archivo, título y autor directamente con el teclado físico en lugar del virtual en pantalla — mira [Key jazz](#key-jazz-desktop-only).

### Scale / Quantize

La pantalla Scale controla el motor global de escala 12-TET. La fila **Mode** selecciona entre dos modos:- **Cuantizador** (por defecto): la entrada de frases permanece cromática; cuando está activado, las notas se redondean hacia abajo justo antes de llegar al motor de sonido, así cambiar la raíz o la escala reharmoniza la canción sin editar sus frases.
- **Bloqueo de nota**: las notas introducidas en las frases se ajustan a la nota más cercana de la escala a medida que se ingresan, así solo se pueden escribir notas de la escala. Las entradas nuevas y las disminuciones se ajustan hacia abajo; `EDIT + RIGHT` / `EDIT + UP` suben la nota a la siguiente nota de la escala arriba. La entrada siempre sigue la escala del proyecto en esta pantalla: cambiar la raíz o la escala afecta inmediatamente las notas recién ingresadas en las pistas habilitadas, mientras que las notas ya almacenadas permanecen sin tocar. El FX `SCL` no está disponible en este modo y cualquier `SCL` ya escrito se ignora. La cuantización en reproducción de notas simples se omite (la entrada ya está bloqueada); la cuantización de acordes vía `CRD` sigue aplicándose.

- **Clock source** elige entre **Internal** (el tempo propio del proyecto), **Midi Clock** (el secuenciador sigue un reloj MIDI entrante) y **Ableton Link** (el secuenciador sigue el tempo de una sesión Link en la red local). Con **Midi Clock**, la fila **Tempo** aparece atenuada y muestra el tempo medido del reloj entrante; el transporte (play/stop) lo controlan los mensajes Start/Continue/Stop del emisor, así que el control PLAY de la app no hace falta. Con **Ableton Link**, la fila pasa a ser la fila **Link**: contiene el interruptor de conexión/desconexión de la sesión en lugar de un campo de tempo (ver abajo). La fuente de reloj se guarda en settings.txt y se aplica a todos los proyectos.
- **Tempo** fija el tempo del proyecto en BPM enteros (`3-500`). Internamente el tracker sigue guardando su tick rate en Hz (`tick rate = BPM x 0,4`), por lo que los proyectos existentes se cargan y guardan sin cambios; un proyecto con tempo fraccionario lo muestra redondeado y se ajusta a un BPM entero en la primera edición del tempo. El ajuste grueso avanza de 5 en 5 BPM, el fino de 1 en 1; borrar restablece el valor por defecto de 125 BPM.

ChooChooTracker guarda proyectos en `.cct`. Este es un formato propietario fácil de leer incluso por un humano.

Usa **Save** antes de cambiar tipos de instrumento o cargar otro proyecto.

### Ableton Link

Ableton Link permite que ChooChooTracker siga el tempo de otras apps compatibles con Link (Ableton Live, LinkHut, etc.) en la misma red local (Wi-Fi o Ethernet). Es solo un receptor: el tracker ajusta su propio tempo a la sesión y nunca envía tempo ni transporte a otros participantes.

**Qué se sincroniza:**

- Tempo de sesión - el tracker sigue continuamente el tempo de la sesión Link (dentro del rango de 3-500 BPM del tracker).
- Fase del beat - pulsar PLAY alinea la reproducción con la rejilla de beats de la sesión: la canción empieza en el siguiente compás (quantum de 4 beats).

**Qué no se sincroniza:**

Durante la reproducción con Link, ChooChooTracker comprueba la fase de beat compartida una vez por compás 4/4 y ajusta suavemente su propio tick rate cuando hace falta. Esto compensa pequeñas diferencias de reloj entre dispositivos sin saltar ni repetir ticks del tracker, y nunca cambia la sesión Link de otro participante.

- Los grooves y los valores `SPD` y `DELAY` no se ven afectados - siguen siendo relativos al tempo seguido.
- Las posiciones del cursor en song/chain/phrase y los ajustes del mixer nunca se comparten.

**Cómo usarlo:**

1. Abre la pantalla Project y pon **Clock source** en **Ableton Link**. La fila de debajo pasa a ser la fila **Link**.
2. Pulsa EDIT en **[Connect]** para unirte a la sesión. El mensaje de estado a la derecha muestra el progreso: `Wait...` al conectar, `No net` si no hay red, `No sess` si no se encuentra sesión, `Err` ante un fallo interno. Mientras estás conectado, el botón muestra **[Disconnect]** y el estado alterna aproximadamente cada segundo entre `connected` y `Peers: N` (N = número de peers).
3. Espera a `connected` y pulsa PLAY - la reproducción empieza alineada con el siguiente compás de la sesión.
4. Pulsa EDIT en **[Disconnect]** (o vuelve a poner la fuente de reloj en Internal) para salir de la sesión.

En modo Link la sesión es dueña del tempo: la fila no muestra campo de BPM y el tempo de la sesión se aplica automáticamente al proyecto (dentro del rango de 3-500 BPM del tracker).

**Disponibilidad por plataforma:** PortMaster (objetivo principal), Windows, Linux, macOS y Android son compatibles. La versión Web y las de Miyoo no incluyen Link.

**Nota PortMaster:** en dispositivos PortMaster el buffer de audio es mayor (512 frames a 48 kHz), por lo que la alineación de beat es aproximada, dentro de unos 10-15 ms, y no exacta al sample. La sincronización del tempo en sí no se ve afectada.

## 13. Configuración

Usa Settings para preferencias que permanecen con la aplicación, no con el proyecto actual. Los controles globales del proyecto como tempo, groove y niveles del mixer se mantienen en sus pantallas propias.

### Controles

- **Repeat delay** y **Repeat speed** configuran cuán pronto y qué tan rápido se repite un botón de navegación o edición mantenido. Aumenta el delay si una pulsación mantenida se dispara demasiado rápido; redúcelo para entrada rápida tipo tracker.
- **Stick live mode** elige cómo funciona el botón mapeado **STICK LIVE**: `HOLD` está activo mientras se mantiene presionado, `TOGGLE` se mantiene activo tras una pulsación, y `FREE` está siempre activo. Ver [Grabación de movimiento](#motion-recording).
- **Asignación de teclas** asigna controles de teclado o controlador. Elige un comando, presiona su nuevo control y luego selecciona **Done** para guardar la asignación.

### Audio

**Sample rate** ofrece `32 kHz` o `48 kHz` (por defecto). Los cambios se aplican de inmediato y se guardan. Cambiar la frecuencia de muestreo detiene la reproducción y la vista previa de samples mientras los motores de sonido y cualquier entrada de audio armada se reabren a la nueva frecuencia. Si un dispositivo rechaza la nueva frecuencia, el ajuste anterior sigue activo. `32 kHz` reduce el trabajo de procesado y limita la frecuencia reproducida más alta a 16 kHz. El indicador de carga de CPU muestra el coste del renderizado de audio.

**Buffer size** equilibra rapidez con estabilidad. Usa un valor menor para tocar en vivo con menos latencia; súbelo si el audio cruje o la CPU está siempre alta. Los valores disponibles son `32`, `64`, `128`, `256`, `512` y `1024` muestras. La lectura de CPU ayuda a elegir un ajuste estable.

### MIDI

Elige dispositivos de entrada y salida, asigna canales MIDI entrantes a instrumentos y mapea controles MIDI CC aquí. Ver [MIDI](#15-midi) para conectar un teclado, controlar el mixer o enviar notas a hardware externo.

### Sintetizadores

- **AY Quality** elige el nivel de calidad usado por instrumentos AY/YM. **AY Sample dithering** puede suavizar la reproducción de samples AY; déjalo desactivado para un resultado más directo y lo-fi o si apuntas a hardware original.
- **Braids BITS**, **DRFT** y **SIGN** son ajustes globales de carácter Braids, compartidos por todos los instrumentos Braids. Úsalos para ajustar la resolución, deriva y carácter general antes de afinar sonidos individuales. Ver [Braids](#braids).

### Mixer

- **Mix volume** es el nivel maestro de salida de la aplicación. Bájalo si alimentas otro mixer o grabador; no afecta la mezcla individual de pistas en la canción.
- **Tilt pivot** fija la frecuencia central para todos los controles Mixer **Tilt** en este proyecto, entre `250 Hz` y `4000 Hz` (por defecto `1 kHz`).

### Gráficos

- **Tema de color de edición** cambia los colores de la interfaz. **Load font** carga una fuente para pantalla `.cnfont`.
- **Forma de onda de la canción** muestra u oculta la forma de onda del mix en la página Song; ver [Forma de onda de la canción](#song-waveform).
- **Visuales de pista** elige para cada pista entre **Detailed** (forma de onda del instrumento y superposiciones) o **Audio waveform** (audio resultante de la pista). Usa **All detailed** o **All audio** para cambiar todas las pistas a la vez.
- **Waveform FPS** ajusta la frecuencia de refresco del osciloscopio del instrumento y visualizadores del mixer, de 1 a 60 cuadros por segundo. Bájalos si quieres visualización más suave o más espacio de CPU.
- **Renderer** indica si la aplicación usa renderizado `GPU` o `Software`.

### Informe de soporte y salir

**Support report** crea un pequeño archivo diagnóstico para reporte de bugs. Adjúntalo con el proyecto `.cct` si un problema depende de una canción específica. **Quit ChooChooTracker** cierra la aplicación limpiamente.

### Forma de onda de la canción

Gráficos → Forma de onda de la canción está DESACTIVADA por defecto. Toca EDIT para activar o usa EDIT + dirección; la elección se guarda como preferencia de la app. Al activarse, una forma de onda de dos filas aparece sobre la página SONG mostrando el mix final sumado a mono. Las demás páginas conservan su diseño completo, espaciado normal y previsualizaciones del instrumento sin cambio. La preferencia guardada es compatible con versiones anteriores. Los objetivos táctiles en SONG siguen las filas mostradas en ambos modos.

## 14. Solución de problemas

### CPU

El consumo de CPU depende de los motores y efectos activos. Los modelos físicos de Plaits y la reverberación Clouds pesan más que voces básicas AY. Revisa rendimiento en la consola destino, especialmente con 8 voces Plaits y ambos envíos activos. Una CPU cercana a `100%` puede causar crujidos o retrasos de audio.

Si hay chasquidos, cierres o lentitud, usa **Settings > Support report** y adjunta el archivo `support-report.txt` al reporte de error. Si un tema específico causa el problema, adjunta ese `.cct`.

### Sin sonido

Revisa el número de instrumento, estado mute o solo de pista, nivel LVL de pista, volumen de salida de la aplicación y el envolvente del instrumento. En samples, confirma que el WAV original siga en su ruta guardada.

En ArkOS, presiona **MENU + L3** para alternar mute del sistema operativo. Si no ayuda, reinicia el hardware y reconecta la interfaz de audio. Prueba la opción “repair audio” en ArkOS, puede ser útil.

### La entrada no responde bien

Ajusta Repeat delay y Repeat speed en Settings. Si una pulsación simple mueve dos veces, verifica que solo hay 1 control físico asignado a esa dirección y reporta la pantalla y atajo exactos.

## 15. MIDI

Settings > **MIDI** abre este submenú: **MIDI In** / **MIDI Out** selecciona un dispositivo conectado, alternando entre puertos detectados con `OFF` en ambos extremos; ambos sentidos necesitan un dispositivo seleccionado.

- **Previsualización de sonido desde un teclado MIDI**: con un dispositivo MIDI In seleccionado, tocar notas en el teclado conectado reproduce un instrumento en cualquier pantalla, igual que el atajo en pantalla **EDIT + PLAY** en la pantalla [Instrument](#5-instruments). Es solo una previsualización, no entra notas en la canción.
- **Mapeo de canales**: abre la lista de los 16 canales MIDI; asigna a cada uno un instrumento (`OFF` por defecto) para que las notas recibidas en ese canal previsualicen ese instrumento sin importar cuál esté seleccionado en la pantalla Instrument - por ejemplo canal `01` -> instrumento `05`. Un canal con `OFF` usa el instrumento actualmente seleccionado, comportamiento original. El mapeo se guarda en settings.txt.
- **Mapeo MIDI CC**: asigna un controlador entrante a un parámetro de instrumento, incluyendo su **Volume**, o a controles globales. Las filas globales muestran `TRK 1`-`TRK 8` en lugar de instrumento y ofrecen **Mute**, **Solo**, **Volume**, **Reverb send** y **Delay send** por pista; **Song play/stop** no tiene destino. Un mapeo nuevo ignora el CC que lo aprendió/configuró y entra en efecto en el próximo movimiento físico, por lo que mapear mientras se reproduce no resetea parches a cero. Mute y solo usan `0-63` = apagado, `64-127` = encendido.
- **Sincronización por reloj MIDI**: con un dispositivo MIDI In seleccionado, pon **Clock source** de la pantalla Project en **Midi Clock** para seguir a un secuenciador externo. El tracker mide el tempo del reloj entrante (24 pulsos por negra) y lo muestra en la fila **Tempo** atenuada; los mensajes **Start**, **Continue** y **Stop** controlan la reproducción. La fuente de reloj se recuerda en settings.txt.
- **Controlar un dispositivo MIDI externo**: asigna a una pista el tipo de instrumento [MIDI Out](#midi-out) y fija su Canal; tocar notas en esa pista enviará MIDI real al dispositivo MIDI Out seleccionado en lugar de reproducir sonido en ChooChooTracker.

## 16. Créditos y licencias

ChooChooTracker es un fork de ChipNomad y mantiene su licencia MIT. Braids, Plaits, Plaits-Alt, Clouds DSP, MME DSP basado en Warps y código stmlib derivan de liberaciones open-source de Mutable Instruments bajo sus respectivos avisos MIT. Plaits-Alt proviene del fork lylepmills/eurorack Plaits Lab; aplican sus avisos de fuente retenidos. El motor aChChid usa Open303 de Robin Schmidt, copyright 2009, bajo Licencia MIT. MonkSynth usa el DSP nativo de JonET bajo Licencia MIT; véase `LICENSE.monksynth.txt`. Bogie y Sintered son implementaciones nativas originales. El I/O MIDI usa RtMidi de Gary P. Scavone, con licencia estilo MIT. Consulta los archivos de licencia incluidos para atribución exacta. Ableton Link se proporciona bajo un acuerdo especial de licencia de Ableton, no está bajo licencia MIT, y nuestra licencia no se extiende a tus forks ni a otros proyectos fuera de ChooChooTracker.