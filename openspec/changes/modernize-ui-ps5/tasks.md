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
- [x] 6.2 Wallpaper = portada enfocada, a pantalla completa, desenfocada, con velo en degradado. Elemento nuevo `CoverWallpaper`.

  **Cómo se desenfoca sin maquinaria nueva:** la cadena samplea el **framebuffer**. Si el wallpaper dibuja primero la portada estirada, el framebuffer *ya la contiene* — así que un `rmDrawFrosted()` a pantalla completa **es** el wallpaper desenfocado. Y cuando no hay blur, `rmDrawFrosted` degrada a sólo el tinte, dejando la portada nítida debajo: exactamente el fallback que pide la spec, sin escribir una línea para ello.

  El velo es un degradado (`rmDrawRectGradient`, un solo quad gouraud — el GS interpola el color entre vértices gratis, así que cuesta lo mismo que un rectángulo plano). Va más oscuro abajo, que es donde se apoya el título.

  Se estira un tile de 128×192 a pantalla completa, y da igual: acaba desenfocado. Eso mantiene la portada nativa (hasta 1.440 KiB) fuera de la VRAM.
- [x] 6.3 Transición del wallpaper al cambiar de foco: crossfade de 350 ms con `uiEaseInOutCubic`.

  Hizo falta `rmDrawPixmapBlend()`: `rmDrawQuad` decide el alpha **según el formato** y sólo lo activa para CT32, así que un tile CT16 era literalmente imposible de fundir. La variante nueva fuerza el blending y deja que el alpha del color mande.

  **El crossfade guarda punteros a `GSTEXTURE`, no a items del menú.** Los `GSTEXTURE` de una caché viven lo que vive la caché; un item de submenú puede liberarse bajo tus pies cuando se reconstruye la lista de dispositivos. En el peor caso la entrada saliente se recicla a mitad del fundido y se mezcla el arte equivocado durante unos cientos de ms — pero nunca puede quedar colgado.
- [x] 6.4 No es el tema por defecto: el usuario copia `themes/PS5/` a **`<dispositivo>/THM/thm_PS5/`** y lo elige a mano.

  **El prefijo `thm_` no es opcional.** `thmReadEntry()` (`themes.c:1553`) sólo considera tema un directorio cuyo nombre contiene `thm_`, y construye el nombre a mostrar desde `name + 4`. Una carpeta llamada `PS5` a secas se ignora en el escaneo: el tema no aparece en Ajustes y no hay ningún mensaje de error que lo explique. Con `thm_PS5`, el nombre que sale en la lista es `PS5`.

**Verificable:** el tema PS5 seleccionable y usable de punta a punta.

**Ajuste al integrarlo:** `validateBackgroundElems()` antepone un `Background` por defecto si el primer elemento no lo es. Un `CoverWallpaper` **es** el fondo, así que ahora cuenta como tal; sin eso, OPL dibujaba debajo un fondo a pantalla completa que quedaba tapado del todo, más una caché que no leía nadie.

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

## 7-bis. Puesta en marcha en consola física — ✅ ARRANCA

El supuesto de la fase 7 («no hay consola y no la va a haber») dejó de valerse: el tema se probó en una PS2 real sobre USB. Tres cuelgues duros seguidos, ninguno en el código nuevo de GS — los tres eran punteros nulos que el tema PS5 fue el primero en destapar.

- [x] 7b.1 **La carpeta necesita el prefijo `thm_`.** `thmReadEntry()` (`themes.c:1553`) sólo reconoce como tema los directorios cuyo nombre lo lleva, y toma el nombre a mostrar desde `name + 4`. Sin él, el escaneo la ignora en silencio: el tema no sale en Ajustes y no hay ningún aviso. La tarea 6.4 decía `THM/PS5/` y estaba mal.
- [x] 7b.2 **`validateItemsList()` no rellenaba el hueco del tema.** Fabricaba el `ItemsList` por defecto y lo enlazaba en `mainElems`, pero `list` era una copia por valor, así que `theme->gamesItemsList` seguía a NULL. `menusys.c:654` y cuatro sitios más leen `gTheme->itemsList->extended` sin comprobarlo, desde `menuNextV`/`menuPrevV`/`menuNextH`/`menuPrevH`. Ahora se pasa por dirección. **Fallo latente de upstream**: cualquier tema sin `ItemsList` cuelga OPL.
- [x] 7b.3 **El culpable del cuelgue al seleccionar el tema: `use_default=0` sin traer imágenes.** Con ese flag `thmLoadResource()` no cae de vuelta al arte interno, así que las 40+ texturas quedan con `Mem = NULL` y `thmGetTexture()` devuelve NULL para todo. `guiAlignMenuHints()` y `guiAlignSubMenuHints()` (`gui.c`) lo desreferenciaban sin comprobar — y además dividían por `iconTex->Height`. Eran las **dos únicas** excepciones del código: el resto de sitios ya comprobaban. `diaRenderUI()` llama a la segunda, y la pantalla de Ajustes *es* un diálogo, así que el cuelgue ocurría en el mismo menú desde el que eliges el tema. Corregidas ambas, y el tema pasa a `use_default=1`.

**Lo que sí funciona en hardware, confirmado en pantalla:** el tema carga, el `CardShelf` dibuja las tarjetas (portada real cuando hay `_COV`, placa con el título cuando no), el `ItemText`, el `HintText` y la composición general.

## 7-ter. Ajustes tras verlo en la tele

- [x] 7c.1 **El fondo ya no es la portada desenfocada.** Estirar un tile de 128×192 a 640×480 y desenfocarlo daba una mancha de color. `CoverWallpaper` gana `_pattern` (por defecto `BG`, el arte por juego a tamaño nativo) y `_blur` (por defecto 0). El desenfoque sólo tenía sentido mientras la fuente era una portada estirada 5×: con arte de pantalla completa, desenfocar tira justo el detalle que ese arte tiene. El cristal se queda donde le toca, en la franja del `FrostedPanel`. `_pattern=COV` recupera el comportamiento anterior y reactiva el blur solo.
- [x] 7c.2 **El estante se centra en el foco.** Antes la página arrancaba pegada al borde izquierdo y con pocos juegos dejaba un hueco enorme a la derecha. Ahora la fila se desliza para que la tarjeta enfocada quede en el centro del estante, usando el foco **animado**, así que se desplaza en vez de saltar.
- [x] 7c.3 **Izquierda/derecha recorre los juegos.** Un `CardShelf` reparte los items en X, así que `theme->horizontalItems` invierte los ejes de la cruceta en `menuHandleInputMain()`: izquierda/derecha camina la lista y arriba/abajo cambia de dispositivo. Los temas sin estante conservan el mapeo original.
- [x] 7c.4 **Iconos de botones monocromos.** Los internos de OPL son los de PS2: rellenos y con código de color. Los de PlayStation moderna son contornos blancos. `themes/PS5/make_icons.py` los genera (círculo, cruz, triángulo, cuadrado, más *Create* y *Options*), dibujados a 8× y reducidos con LANCZOS porque OPL los pinta a unos 20 px en un CRT.

## 7. Medición final — ❌ NO LA PODEMOS HACER

**No hay consola física, y no la va a haber.** El plan original decía «no se pasa a la siguiente fase sin que la anterior corra en hardware real». Esa condición ya no se puede cumplir, así que hay que decir con qué se sustituye en vez de fingir que sigue pendiente.

**Lo que sí se cerró sin hardware:**

- [x] 7.1 **Presupuesto de relleno, calculado.** El relleno son píxeles escritos por frame; no depende de nada que PCSX2 falsee. La cadena de desenfoque cuesta **143,360 px = media pantalla**, o sea entre el **0.7% y el 1.7%** del presupuesto de un frame a 60 Hz según lo que se asuma que rinde el GS. El requisito de la spec (`<20%`) se cumple con un margen enorme. Ver design.md.
- [x] 7.2 **VRAM, medida en PCSX2 y cuadrada con la teoría.** `1632 KiB TEXMAN` = `4096 − 2240 (framebuffers) − 224 (cadena)`. La VRAM sí es fiable en el emulador: es contabilidad de direcciones, no de rendimiento.
- [x] 7.3 **Recortado el desperdicio obvio.** En estado estable el wallpaper ya no dibuja la capa saliente del crossfade: se ahorra una pantalla completa de relleno en casi todos los frames.

**Lo que queda genuinamente abierto, y hay que decirlo en la PR:**

- [ ] 7.4 **Fps reales en consola.** El tema completo apila ≈6.1 pantallas de relleno por frame. Entre el 9% y el 21% del frame según lo que rinda de verdad el GS con bilineal y blending. **Esto no es el blur** —el blur es el 1.7%— **es la composición del tema.** Sólo se puede medir en hardware.
- [ ] 7.5 **Thrashing del TexManager.** El conjunto de trabajo (~1 MiB) cabe en el pool encogido (1632 KiB) con holgura sobre el papel, pero re-subidas por DMA por frame se manifiestan como pérdida de fps, no como fallo visual. No se ve en PCSX2.
- [ ] 7.6 **PAL** (pool más estrecho: 1312 KiB) y **consolas FAT vs SLIM**.

## 8. Upstream — PR en borrador, pidiendo testers

El camino honesto: la comunidad de OPL **sí** tiene hardware. Una PR en borrador con instrucciones de build y unas preguntas concretas vale más que un cambio no publicado que nadie puede validar.

- [x] 8.1 PR abierta **en el fork** (`danielnuld/Open-PS2-Loader#1`, `master ← feature/ps5-ui`, en borrador), no contra `ps2homebrew`. Es el paso previo: revisar en casa antes de exponerlo. Redactada en inglés justamente para poder re-apuntarla a upstream sin reescribirla.
- [x] 8.2 **Estado de las pruebas, explícito y arriba del todo.** Verificado en PCSX2; **no verificado en hardware**. Lleva el presupuesto de relleno calculado, la medición de VRAM y las tres preguntas abiertas (7.4, 7.5, 7.6), dejando claro que **el riesgo no es el blur** (1.7% del frame) sino la composición del tema (~6.1 pantallas de relleno).
- [x] 8.3 Instrucciones de `make DEBUG=1`: el overlay trae fps, `KiB TEXMAN` y los tiempos del reescalador, que es justo lo que un tester necesita para contestar.

**Pendiente antes de apuntar a upstream:** los mensajes de commit están en español. Para `ps2homebrew` habría que reescribirlos en inglés.
