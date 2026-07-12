# Diseño

## Contexto: por qué esto es un injerto y no una reescritura

OPL ya tiene todo lo necesario. La auditoría del código lo confirma:

- `src/renderman.c` es un wrapper fino sobre gsKit, con el presupuesto de VRAM ya declarado (`__VRAM_SIZE = 4194304`).
- `src/fntsys.c` ya hace FreeType + atlas de glifos.
- `src/themes.c` es un registro de elementos tipados con `init`/`draw` por tipo — un punto de extensión limpio.
- OPL **ya descarga arte de portadas** (elemento `ItemCover`, cachés `ART`).
- OPL ya soporta 480p, 576p, VGA, 720p y 1080i.

Y lo más importante: OPL ya usa las convenciones correctas del GS, las mismas que el prototipo tuvo que descubrir depurando:

```c
const u64 gDefaultAlpha = GS_SETREG_ALPHA(0, 1, 0, 1, 0);   // source over
const u64 gDefaultCol   = GS_SETREG_RGBA(0x80, 0x80, 0x80, 0x80); // neutro de MODULATE
gsKit_set_primalpha(gsGlobal, gDefaultAlpha, 0);
```

Ambas cosas son necesarias porque el default de gsKit (`GS_BLEND_BACK2FRONT`, valor crudo `0x01`) decodifica a `Cv = (Cd − Cs)·As + Cs`, donde el alpha significa **transparencia**: con `As = 0x80` el primitivo sale invisible. OPL ya lo corrige. El código nuevo hereda ese contexto y no debe tocarlo.

## Decisión 1: dónde vive el desenfoque

**Módulo nuevo `src/rmblur.c`**, no dentro de `renderman.c`.

Razón: `renderman.c` manipula estado global del GS y es el corazón del render. La cadena de desenfoque reapunta el registro `FRAME` a mitad de frame — es la operación más invasiva del proyecto. Aislarla en un módulo con una superficie mínima (`rmBlurInit`, `rmBlurBackdrop`, `rmBlurTexture`, `rmBlurAvailable`) mantiene el blast radius acotado y hace el cambio revisable.

`renderman.h` sólo expone `rmBlurBackdrop()` y `rmDrawFrosted()`.

## Decisión 2: cómo se hace render-to-texture en gsKit

gsKit no tiene API de render target. Se consigue reapuntando el buffer activo:

```c
gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1] = vram_addr;
gsGlobal->Width  = rt_w;
gsGlobal->Height = rt_h;
gsKit_setactive(gsGlobal);
```

**Trampa crítica, verificada en el prototipo:** `gsKit_setactive()` emite el registro `FRAME` **de inmediato, no lo encola**. Si quedan primitivos en la cola, se dibujarán contra el destino *nuevo* en vez del viejo. Hay que vaciar la cola con `gsKit_queue_exec()` **antes** de cada cambio de destino.

Al terminar, se restauran `ScreenBuffer[]`, `Width` y `Height`, y se vuelve a llamar a `gsKit_setactive()`.

`OffsetX`/`OffsetY` NO se tocan: el GS aplica el offset a los vértices y lo resta al rasterizar, así que se cancela y el mapeo se mantiene correcto para cualquier tamaño de destino.

## Decisión 3: el algoritmo

Pirámide de reducción + ping-pong bilineal. Sin blending en ninguna pasada.

```
framebuffer (640×448)
   ↓ bilineal                      (1 tap = promedio de 2×2 gratis)
rt0 (320×224, CT16S)
   ↓ bilineal
rt1 (128×112, CT16S)
   ↓ 4 pasadas ping-pong rt1 ⇄ rt2, desplazamiento de téxel alternando signo
resultado (128×112)
```

Iterar un box blur converge a un gaussiano. Cada pasada trabaja a 128×112 sobre un chip de 2.4 Gpixel/s: el coste es ruido.

**Las pasadas van con `PrimAlphaEnable = OFF`.** Son copias, no composiciones. Si se dejara el blending activo, el canal alpha del framebuffer entraría en la ecuación — y en CT16S ese canal es de **un solo bit**, cuyo valor depende de detalles del formato que no controlamos. Apagarlo elimina toda esa clase de bugs.

### Los anchos tienen que ser múltiplos EXACTOS de 64

La primera versión de este diseño usaba `160` en el segundo nivel y afirmaba que era múltiplo de 64. **No lo es** (160/64 = 2.5), y no es un detalle cosmético: gsKit escribe y lee el mismo buffer con dos redondeos distintos.

```c
// gsCore.c:105 — gsKit_setactive(): FRAME.FBW
gsGlobal->Width / 64                                    // TRUNCA:   160 → 2

// gsMisc.c:15 — gsKit_setup_tbw(): TEX0.TBW
(-64) & (Texture->Width + 63)  / 64                     // REDONDEA: 160 → 3
```

Un buffer de 160 de ancho se **renderiza** con stride de 128 téxeles y se **samplea** con stride de 192. El contenido sale rasgado. Como `FRAME` no lleva campo de altura, la restricción aplica sólo al ancho.

Anchos válidos: `320` (=5·64) y `128` (=2·64), que coinciden bajo ambos redondeos. Por eso el segundo nivel es **128**, no 160. La reducción horizontal queda en /2.5 en vez de /2 — el kernel resulta un pelo más ancho en horizontal que en vertical, invisible en algo que ya está desenfocado a propósito.

### Los render targets van en `GSKIT_ALLOC_SYSBUFFER`

`FRAME.FBP` direcciona la VRAM en unidades de **8192 bytes**. `GSKIT_ALLOC_USERBUFFER` alinea a 256, así que un render target reservado así tendría los bits bajos de su dirección truncados y el GS dibujaría en otro sitio. `GSKIT_ALLOC_SYSBUFFER` es el tipo que redondea a 8 KiB.

### Los offsets alternan signo

Un desplazamiento con parte fraccionaria de medio téxel es justo lo que hace que el tap bilineal promedie un vecindario de 2×2. Pero un desplazamiento siempre en el mismo sentido *arrastra* la imagen. Los offsets son `{+0.5, −0.5, +1.5, −1.5}`: la magnitud crece (el kernel se ensancha) y la suma es cero (la imagen no se mueve).

Los buffers son **CT16S aunque el framebuffer sea CT24**: el resultado se ve desenfocado y escalado, la pérdida de precisión de color es invisible, y cuesta la mitad.

## Decisión 4: presupuesto de VRAM — el riesgo NO es el que parecía

La primera versión de este diseño decía: "si `gsKit_vram_alloc()` falla, degradar". **Eso era incorrecto**, y la medición del código lo destapó.

**OPL no mantiene las texturas residentes en VRAM.** Usa el TexManager de gsKit (`gsKit_TexManager_bind` / `_invalidate` / `_nextFrame`), que trata toda la VRAM posterior a `gsGlobal->CurrentPointer` como un **pool de streaming**: las texturas se suben bajo demanda y se desalojan. El propio OPL lo muestra en su overlay de debug:

```c
"%dKiB FIXED",  gsGlobal->CurrentPointer / 1024
"%dKiB TEXMAN", (4*1024*1024 - gsGlobal->CurrentPointer) / 1024
```

La prueba de que el pool es imprescindible: **los assets integrados suman 6,508 KiB en CT32 — más que los 4 MiB de eDRAM total.** No caben ni queriendo. OPL los hace caber de dos formas:

1. **PNG paletizados** para los assets grandes. `background.png` e `info.png` son 1024×512 con paleta de 4 bits → `GS_PSM_T4`: 256 KiB cada uno en vez de los 2 MiB que costarían en CT32.
2. **Streaming**: lo que no cabe, se re-sube.

### Qué implica esto para el blur

Reservar los buffers de desenfoque con `gsKit_vram_alloc()` **nunca va a fallar**. Lo que hace es **encoger el pool de streaming**:

| Modo | PSM | Framebuffers (FIXED) | Pool TEXMAN | Con blur |
|---|---|---|---|---|
| NTSC 640×448 | CT24 | 2,240 KiB | 1,856 KiB | 1,648 KiB (−208 KiB, −11%) |
| 480p 640×448 | CT24 | 2,240 KiB | 1,856 KiB | 1,648 KiB (−208 KiB, −11%) |
| PAL 640×512 | CT24 | 2,560 KiB | 1,536 KiB | 1,312 KiB (−224 KiB, −15%) |
| 720p / 1080i | CT16S | — | — | **deshabilitado** |

Cuenta de la cadena, ya redondeada a los bloques de 8 KiB que impone `GSKIT_ALLOC_SYSBUFFER`:

- **NTSC** (320×224 + 2×128×112): `147,456 + 2·32,768 = 212,992 B ≈ 208 KiB`
- **PAL** (320×256 + 2×128×128): `163,840 + 2·32,768 = 229,376 B ≈ 224 KiB`

**El fallo, si llega, no es un error de asignación: es thrashing.** Si el conjunto de trabajo de texturas por frame deja de caber en el pool encogido, el TexManager re-sube texturas por DMA en cada frame y el rendimiento cae. Se manifiesta como pérdida de fps, no como una pantalla en negro.

## Decisión 5: las portadas se reescalan al cargar — es un prerrequisito, no una optimización

**El consumidor grande no es el blur. Son las portadas.**

OPL sube las portadas a VRAM **a la resolución nativa del PNG**, y el escalado ocurre al dibujar:

```c
texture->Width  = pngWidth;                 // src/textures.c:477
texture->Height = pngHeight;

// Not related to screen size, just to limit at some point
static int maxSize = 720 * 512 * 4;         // src/textures.c:102  →  1,440 KiB
```

Es decir: **una sola portada puede ocupar hasta 1,440 KiB** — casi todo el pool de 1,856 KiB. Funciona hoy porque el menú actual dibuja **una** portada a la vez (`ItemCover`) y el TexManager hace streaming.

El `CardShelf` quiere dibujar **siete simultáneas**. A tamaño nativo eso son entre 3 y 10 MiB. No es que el margen sea estrecho: **no cabe en la consola**.

### La decisión

Las portadas del shelf SE REESCALAN AL CARGAR, en el EE, a un tile fijo, y se almacenan en **CT16** (RGBA5551):

| | |
|---|---|
| Tile | **128 × 192** (2:3, la proporción real de una carátula) |
| Formato | **CT16** (`GS_PSM_CT16`), no CT32 |
| Coste por portada | 128·192·2 = **48 KiB** |
| 7 portadas visibles | **336 KiB** |

Frente a los ~784 KiB del cálculo optimista anterior, o a los varios MiB del caso real. Es una reducción de entre 10× y 30×.

**Por qué CT16 y no CT32:** la tarjeta enfocada se dibuja a ~124 px, así que 128×192 ya es prácticamente 1:1 — no hay resolución que ganar. Y RGBA5551 con el dithering del GS (que OPL ya activa) es indistinguible a ese tamaño. Cuesta la mitad.

**Por qué reescalar en el EE y no en el GS:** podríamos reescalar por render-to-texture reusando la maquinaria del blur, pero eso obligaría a subir la portada nativa a VRAM primero — un pico transitorio de hasta 1,440 KiB, justo lo que queremos evitar. Reescalar en el EE (filtro de caja, una vez por portada, cacheado) mantiene la imagen nativa fuera de la VRAM por completo. El coste en CPU es de unos pocos ms sobre una imagen de ≤720×512, y ocurre una sola vez.

### Conjunto de trabajo resultante

| | NTSC | PAL |
|---|---|---|
| Pool TEXMAN con blur activo | 1,646 KiB | 1,296 KiB |
| Fondo (T4) | 256 KiB | 256 KiB |
| Atlas de fuentes | ≤256 KiB | ≤256 KiB |
| 7 portadas (128×192 CT16) | 336 KiB | 336 KiB |
| Iconos y varios | ~100 KiB | ~100 KiB |
| **Total** | **~948 KiB** | **~948 KiB** |
| **Margen** | **698 KiB** | **348 KiB** |

Cabe en ambos, y **también cabría en PAL sin el blur activado** con holgura. El diseño deja de estar al borde.

## Decisión 5: animación por tiempo, no por frames

OPL corre a 50 Hz en PAL y 60 Hz en NTSC, y pierde frames. Una animación indexada por frames iría un 20% más lenta en Europa.

`src/uianim.c` expone un suavizado exponencial independiente del framerate:

```c
/* Todo en float. El R5900 no tiene doble precisión: cada double
   se emula por software vía libgcc, 1-2 órdenes de magnitud más caro. */
float uiApproach(float cur, float target, float rate, float dt);
```

Se compila con `-Wdouble-promotion` para que un `double` colado no pase desapercibido.

## Decisión 6: compatibilidad hacia atrás

Los tipos nuevos se **añaden al final** de `elementsType[]`. No se reordena nada. Un tema que no los declare no los instancia, no reserva VRAM de blur, y renderiza byte a byte como antes.

Ésta es también la condición para que el cambio sea *upstreamable*: una reescritura de la UI no se mergea nunca; una extensión aditiva del sistema de temas, sí.

## Alternativas descartadas

- **Reescribir la UI desde cero.** Tira 28k líneas maduras y hace el fork immergeable. El sistema de temas de OPL ya es el punto de extensión correcto.
- **Portar RmlUi (HTML/CSS).** Arrastra libstdc++ y ~1–2 MiB de binario, y no resuelve el desenfoque (que vive en el renderer, no en el motor de layout). El coste no compra nada aquí.
- **Blur con acumulación de 4 taps y alpha blending.** Menos pasadas, pero depende del canal alpha del framebuffer. Descartado por la razón de la Decisión 3.
- **Bajar la pirámide hasta 80×56.** Rompe la restricción de `TBW` múltiplo de 64.

## Riesgos abiertos

| Riesgo | Mitigación |
|---|---|
| La VRAM no alcanza con temas reales | Reserva condicional + degradación. Hay que **medir** con los temas populares, no suponer. |
| El modo `hires` de OPL usa un camino distinto (`gsKit_hires_*`) | Blur desactivado ahí desde el principio. Ni se intenta. |
| Modos entrelazados: el blur opera sobre un campo | Aceptable: el resultado está desenfocado por definición. Verificar en hardware real, no sólo en PCSX2. |
| Regresión de rendimiento en consolas reales | Medir en PS2 física, no en emulador. PCSX2 no modela el coste del GS. |
