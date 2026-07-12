# Tareas

Cada fase termina en algo verificable **en hardware real**, no sólo en PCSX2. El emulador no modela el coste del GS ni la presión de VRAM.

## 1. Base: render-to-texture — ✅ VERIFICADO EN PCSX2

- [x] 1.1 Crear `src/rmblur.c` + `include/rmblur.h` con la superficie mínima: `rmBlurInit`, `rmBlurEnd`, `rmBlurBackdrop`, `rmBlurTexture`, `rmBlurAvailable`.
- [x] 1.2 Implementar el bind/restore del render target vía `ScreenBuffer[]` + `gsKit_setactive()`. **Vaciar la cola con `gsKit_queue_exec()` antes de cada cambio de destino** (ver design.md, Decisión 2). Se salva y restaura también `PSM`, que `gsKit_setactive()` mete en `FRAME`.
- [x] 1.3 Reservar los buffers CT16S con `gsKit_vram_alloc()` y **comprobar `GSKIT_ALLOC_ERROR`**. Si falla, dejar el módulo deshabilitado. **Van en `GSKIT_ALLOC_SYSBUFFER`**: `FRAME.FBP` direcciona en unidades de 8 KiB y `USERBUFFER` sólo alinea a 256 (ver design.md).
- [x] 1.4 Prueba puntual: el panel de cristal de `gEnableBlurTest` sale bien en PCSX2 — se ve el fondo desenfocado dentro de la región y **el resto del frame no se corrompe**.

**Estado:** ✅ **Verificado en PCSX2.** El render-to-texture sobre gsKit funciona: el bind/restore del destino es correcto y el `gsKit_queue_exec()` previo a cada `gsKit_setactive()` mantiene el orden de dibujado. **Era el riesgo #1 del proyecto y está despejado.**

**Verificable:** el RT se dibuja en pantalla y el resto del frame no se corrompe.

## 2. La cadena de desenfoque — ✅ SE VE BIEN EN PCSX2; FALTA MEDIR

- [x] 2.1 Pirámide de reducción: framebuffer → 320×H/2 → 128×H/4, bilineal, `PrimAlphaEnable = OFF`.
- [x] 2.2 Cuatro pasadas de ping-pong. Los offsets **alternan signo** (`{+0.5, −0.5, +1.5, −1.5}`): la magnitud crece pero suman cero, así la imagen no se arrastra.
- [x] 2.3 Derivar los tamaños de los RT del modo de vídeo activo (las alturas salen de `gsGlobal->Height`). Anchos **múltiplos exactos de 64**.
- [x] 2.4 Deshabilitar el blur cuando `hires` esté activo (720p/1080i).

**Corrección de diseño (hallazgo al implementar):** el diseño decía que el segundo nivel era 160×112 y que 160 era múltiplo de 64. **No lo es.** gsKit escribe ese buffer con `FRAME.FBW = Width/64` (trunca → 2) y lo lee con `TEX0.TBW = ceil(Width/64)` (→ 3): stride de escritura 128, stride de lectura 192, contenido rasgado. El segundo nivel pasa a **128** de ancho. Ver design.md.

**Verificable:** una pantalla de prueba muestra el fondo desenfocado. Medir el coste con `rmEndFrame` y confirmar que quedan 60 fps.

## 3. `rmDrawFrosted()` — ✅ VERIFICADO EN PCSX2

- [x] 3.1 Dibujar el backdrop desenfocado en la región, **sin blending** (no depender del alpha de 1 bit del framebuffer). Vive en `renderman.c`, que es donde están `X_SCALE()` y `fRenderXOff` — los que mapean la región lógica de 640×480 a píxeles reales del framebuffer, y por tanto a UVs de la cadena.
- [x] 3.2 Componer el tinte encima, ese sí con blending.
- [x] 3.3 Camino de degradación: si no hay cadena, `rmBlurTexture()` devuelve `NULL` y se dibuja sólo el tinte. Siempre es seguro llamarla.
- [x] 3.4 **Punto de llamada.** Nada llamaba a `rmBlurBackdrop()`, así que el enlazador se comía el módulo entero. Añadido un test puntual en `guiShow()` bajo `#ifdef __DEBUG` (`gEnableBlurTest`), que es lo que hace verificable la tarea 1.4. El tema PS5 lo sustituirá en la fase 6.

**Verificable:** un panel de cristal sobre el menú actual de OPL, sin tocar ningún tema. ✅ Confirmado en PCSX2.

**Lo que PCSX2 NO contesta, y sigue abierto:**

- **Los fps.** PCSX2 no modela el coste real del GS ni la presión sobre el pool del TexManager. El requisito de «menos del 20% del presupuesto de frame» (spec `gs-backdrop-blur`) sólo se puede cerrar en consola física.
- **PAL.** Todo lo verificado hasta ahora es NTSC. En PAL los RT son más grandes (320×256 + 2×128×128 = 224 KiB) y el pool que queda es bastante más estrecho.
- **Hardware real**, FAT y SLIM. Ver fase 7.

## 4. Animación — CÓDIGO ESCRITO, LISTO PARA VERIFICAR

- [x] 4.1 `src/uianim.c` + `include/uianim.h`: suavizado exponencial independiente del framerate, todo en `float`. Dos primitivas, porque hacen falta las dos: `uiApproach()` (asintótica, para perseguir un objetivo que se mueve — el foco) y `uiAdvance()` + easings (con duración definida, para cuando «200 ms» tiene que significar 200 ms).
- [x] 4.2 Enganchar un delta de tiempo real al bucle de dibujado. **Va en `guiStartFrame()` (gui.c), no en `menusys.c`**: ahí es donde empieza el frame de verdad, así que una sola muestra de reloj sirve a todos los elementos que animan, y todos ven el mismo delta. Reutiliza el `clock()` / `CLOCKS_PER_SEC` que OPL ya usa; no introduce un segundo reloj.
- [x] 4.3 Compilar el módulo con `-Wdouble-promotion` y dejarlo **sin avisos**. ✅ Limpio, también con `-Wfloat-conversion`.

**Detalle de robustez:** el delta se recorta a 60 ms (`UI_DT_MAX`). Sin eso, un tirón —escanear un dispositivo, arrancar el DVD— teletransportaría todas las animaciones a su destino de golpe.

**Verificable:** una animación de 200 ms dura 200 ms tanto en PAL como en NTSC.

**Pendiente de tu lado:** el panel de prueba de `gEnableBlurTest` ahora se desplaza con `uiApproach()`. En PCSX2 debería moverse suave. La equivalencia PAL/NTSC es correcta por construcción (el paso depende del tiempo transcurrido, no de que se llame a la función), pero medirla de verdad es fase 7.

## 5. Elementos de tema

- [x] 5.1 `FrostedPanel` añadido **al final** de `elementsType[]`, sin reordenar nada. Un solo `rmBlurBackdrop()` por frame aunque el tema ponga varios paneles (guardado con `guiFrameId`).
- [x] 5.2 `CardShelf`: fila horizontal, el foco crece y se eleva, las no enfocadas se atenúan. El crecimiento y la elevación salen de la proximidad al foco **animado** (`uiApproach`), no al índice seleccionado — por eso suavizan en vez de saltar al cambiar de selección. Las no enfocadas se atenúan modulando el color (0x80 es el neutro del GS), así que no hace falta dibujar ningún borde.
- [x] 5.3 **Reescalador de portadas en el EE**: `texLoadCover()` en `textures.c`. Filtro de caja a un tile fijo de 128×192 CT16 (48 KiB). La imagen nativa **nunca se llega a asignar como GSTEXTURE**, así que no puede subir a VRAM ni transitoriamente.

  **Decisión de implementación:** el filtro corre sobre las filas RGBA que devuelve libpng, **no sobre el `Mem` de una GSTEXTURE ya empaquetada**. Empaquetar es específico del GS —el T8 guarda la CLUT swizzleada y el T4 intercambia los nibbles— y filtrar ahí obligaría a deshacer las dos cosas. Por las filas de libpng, un solo camino sirve para CT32, CT24, gris y paleta.

  El caché sale gratis: las portadas ya pasan por `image_cache_t` (`texcache.c`), que cachea por entrada. Lo consumirá el `CardShelf` en 5.3b.
- [x] 5.3b `CardShelf` consume esos tiles, y dibuja una reserva con el título cuando aún no hay portada (las portadas entran por el hilo de IO, así que la fila no debe encogerse mientras llegan).

  **Cómo pide los tiles:** `image_cache_t` gana un campo `psm`. `GS_PSM_CT24` (el valor por defecto) significa «carga a tamaño nativo», que es lo que quiere todo elemento existente; `GS_PSM_CT16` pide el tile reescalado. Los tres backends (`bdm`/`eth`/`hdd`) ya recibían un `psm` que **ignoraban**: ahora lo propagan a `texDiscoverLoadPsm()`.

  **El `CardShelf` tiene caché propia, a propósito.** `initMutableImage()` deduplica cachés por patrón de arte, y un `ItemCover` en el mismo tema también usa `"COV"`. Compartirla mandaría a uno de los dos por el cargador equivocado: o el shelf recibiría portadas a tamaño nativo (justo lo que no cabe), o el `ItemCover` recibiría tiles de 128×192.
- [x] 5.4 Culling: sólo se recorre la página que OPL ya calcula (`menu->item->pagestart`), nunca la lista entera. Una biblioteca puede tener cientos de títulos; el shelf enseña un puñado.
- [ ] 5.5 **Regresión:** cargar un tema antiguo y confirmar que renderiza idéntico y que no reserva VRAM de blur.

  **Resuelto en el código, falta confirmarlo:** `rmBlurInit()` **ya no reserva nada**. El modo de vídeo se fija *antes* de cargar el tema (`opl.c`, `applyConfig`), así que en ese punto es imposible saber si habrá algún panel de cristal. La cadena se reclama **en el primer `rmBlurBackdrop()`**, de modo que un tema que no dibuja cristal nunca entra ahí y nunca paga los 224 KiB. Se comprueba mirando `KiB TEXMAN` en el overlay: con un tema antiguo debe seguir en **1856**, no en 1632.

**Verificable:** un tema de prueba con los dos elementos nuevos, más un tema antiguo intacto.

## 6. Tema PS5

- [x] 6.1 `themes/PS5/conf_theme.cfg` componiendo `CardShelf` + `FrostedPanel`. **El orden importa y no es cosmético:** `FrostedPanel` desenfoca lo que ya se haya dibujado en el frame, así que el fondo va antes del panel y el shelf después, para quedar nítido encima del cristal.
- [ ] 6.2 Wallpaper = portada enfocada, a pantalla completa, desenfocada, con velo en degradado para garantizar contraste del texto.
- [ ] 6.3 Transición del wallpaper al cambiar de foco.
- [ ] 6.4 No hacerlo el tema por defecto.

**Verificable:** el tema PS5 seleccionable y usable de punta a punta.

## 0. Medición previa — HECHA PARCIALMENTE

Se adelantó al resto porque podía invalidar el diseño. Y lo hizo: ver `design.md`, Decisión 4.

- [x] 0.1 Auditar quién consume VRAM. **Hallazgo:** OPL usa el TexManager de gsKit; la VRAM tras `CurrentPointer` es un *pool de streaming*, no memoria residente. El blur no puede "fallar al reservar" — encoge el pool.
- [x] 0.2 Medir los assets integrados. **Hallazgo:** suman 6,508 KiB en CT32, más que la eDRAM total. OPL los hace caber paletizando (`background.png` e `info.png` son 1024×512 T4 = 256 KiB en vez de 2 MiB).
- [x] 0.3 Build de OPL con `DEBUG=1` (overlay de VRAM activo). `OPNPS2LD.ELF` generado.
- [x] 0.4 **FIXED / TEXMAN reales confirmados en el overlay: 2,240 KiB / 1,856 KiB** (NTSC 640×448 CT24). Coincide con la contabilidad estática.
- [x] 0.5 **Dimensionar las portadas.** **Hallazgo decisivo:** OPL sube las portadas a resolución NATIVA (`textures.c:477`) con un límite de 720×512×4 = **1,440 KiB por textura** (`textures.c:102`). Una sola portada puede ocupar casi todo el pool. Siete a tamaño nativo = 3–10 MiB: **imposible**. Decisión: tile fijo **128×192 en CT16 (48 KiB)**, reescalado en el EE al cargar. Ver `design.md`, Decisión 5.
- [x] 0.6 **Validado.** Medido sobre `background.png` (1024×512 paletizado — más grande que el tope de 720×512 de una portada, así que es peor caso):

  | | PCSX2 |
  |---|---|
  | Decodificar el PNG | **152 ms** |
  | Filtro de caja | **37 ms** |

  **El reescalado es asumible, y por tres razones, no por una:**

  1. **No bloquea el render.** Las portadas se cargan con `ioPutRequest(IO_CACHE_LOAD_ART)` → hilo de IO (`ioman.c:200`), no el de dibujado. Los 37 ms no tiran ni un frame.
  2. **Es una fracción de lo que OPL ya paga.** Decodificar el PNG cuesta 152 ms *hoy*, sin tocar nada. El filtro añade un **+24%** a una operación que ya era cara y ya era asíncrona.
  3. **Ocurre una vez por portada**, y el `image_cache_t` lo cachea.

  Una portada real (512×720 ≈ 368k px, frente a 524k) sale proporcionalmente más barata.

  ⚠️ **Números de PCSX2, no de consola.** El emulador no modela fielmente la velocidad del EE y suele correr el código escalar *más rápido* que el hardware real, así que el filtro podría ser bastante más caro en una PS2 física. Da el orden de magnitud y confirma que la idea es viable; **no cierra la fase 7**. Si en hardware resultara caro, la salida obvia es muestrear el filtro cada 2 píxeles (4× más rápido, poca pérdida visible a 128×192).

## 7. Medición final (no opcional) — 🚧 BLOQUEADA: NO HAY CONSOLA FÍSICA

Hoy sólo hay PCSX2, y PCSX2 **no sirve** para nada de esta fase: no modela el coste real del GS ni la presión sobre el pool del TexManager, que son justo las dos cosas que pueden tumbar el diseño. Sin estos números la PR de la fase 8 no es defendible — un mantenedor de OPL preguntará por ellos lo primero.

- [ ] 7.1 Confirmar que el conjunto de trabajo del tema PS5 cabe en el pool encogido, en NTSC **y en PAL** (donde el margen es mucho menor).
- [ ] 7.2 Medir fps en PS2 física en NTSC y PAL, con y sin blur. Buscar **thrashing**: la caída se vería como pérdida de fps, no como fallo visual.
- [ ] 7.3 Probar en consola FAT y en SLIM.
- [ ] 7.4 Documentar los resultados en el propio cambio antes de archivarlo.

## 8. Upstream

- [ ] 8.1 Abrir la PR contra `ps2homebrew/Open-PS2-Loader` como cambio **aditivo**, remarcando que ningún tema existente se altera.
- [ ] 8.2 Adjuntar las mediciones de la fase 7. Sin números, la PR no es discutible.
