# Tareas

Cada fase termina en algo verificable **en hardware real**, no sólo en PCSX2. El emulador no modela el coste del GS ni la presión de VRAM.

## 1. Base: render-to-texture — CÓDIGO ESCRITO, SIN VERIFICAR

- [x] 1.1 Crear `src/rmblur.c` + `include/rmblur.h` con la superficie mínima: `rmBlurInit`, `rmBlurEnd`, `rmBlurBackdrop`, `rmBlurTexture`, `rmBlurAvailable`.
- [x] 1.2 Implementar el bind/restore del render target vía `ScreenBuffer[]` + `gsKit_setactive()`. **Vaciar la cola con `gsKit_queue_exec()` antes de cada cambio de destino** (ver design.md, Decisión 2). Se salva y restaura también `PSM`, que `gsKit_setactive()` mete en `FRAME`.
- [x] 1.3 Reservar los buffers CT16S con `gsKit_vram_alloc()` y **comprobar `GSKIT_ALLOC_ERROR`**. Si falla, dejar el módulo deshabilitado. **Van en `GSKIT_ALLOC_SYSBUFFER`**: `FRAME.FBP` direcciona en unidades de 8 KiB y `USERBUFFER` sólo alinea a 256 (ver design.md).
- [ ] 1.4 Prueba puntual: renderizar un color plano a un RT y volcarlo a pantalla. Confirma bind, restore y orden de cola.

**Estado:** `rmblur.c` compila limpio (sin avisos) y enlaza dentro de `OPNPS2LD.ELF`. Pero **nada llama todavía a `rmBlurBackdrop()`** — el enlazador se come esa función por falta de referencias. No se ha verificado ni un píxel. El punto de enganche es `rmDrawFrosted()`, fase 3.

**Verificable:** el RT se dibuja en pantalla y el resto del frame no se corrompe.

## 2. La cadena de desenfoque — CÓDIGO ESCRITO, SIN VERIFICAR

- [x] 2.1 Pirámide de reducción: framebuffer → 320×H/2 → 128×H/4, bilineal, `PrimAlphaEnable = OFF`.
- [x] 2.2 Cuatro pasadas de ping-pong. Los offsets **alternan signo** (`{+0.5, −0.5, +1.5, −1.5}`): la magnitud crece pero suman cero, así la imagen no se arrastra.
- [x] 2.3 Derivar los tamaños de los RT del modo de vídeo activo (las alturas salen de `gsGlobal->Height`). Anchos **múltiplos exactos de 64**.
- [x] 2.4 Deshabilitar el blur cuando `hires` esté activo (720p/1080i).

**Corrección de diseño (hallazgo al implementar):** el diseño decía que el segundo nivel era 160×112 y que 160 era múltiplo de 64. **No lo es.** gsKit escribe ese buffer con `FRAME.FBW = Width/64` (trunca → 2) y lo lee con `TEX0.TBW = ceil(Width/64)` (→ 3): stride de escritura 128, stride de lectura 192, contenido rasgado. El segundo nivel pasa a **128** de ancho. Ver design.md.

**Verificable:** una pantalla de prueba muestra el fondo desenfocado. Medir el coste con `rmEndFrame` y confirmar que quedan 60 fps.

## 3. `rmDrawFrosted()`

- [ ] 3.1 Dibujar el backdrop desenfocado en la región, **sin blending** (no depender del alpha de 1 bit del framebuffer).
- [ ] 3.2 Componer el tinte encima, ese sí con blending.
- [ ] 3.3 Camino de degradación: si `!rmBlurAvailable()`, dibujar sólo el tinte.

**Verificable:** un panel de cristal sobre el menú actual de OPL, sin tocar ningún tema.

## 4. Animación

- [ ] 4.1 `src/uianim.c` + `include/uianim.h`: suavizado exponencial independiente del framerate, todo en `float`.
- [ ] 4.2 Enganchar un delta de tiempo real al bucle de dibujado de `menusys.c`.
- [ ] 4.3 Compilar el módulo con `-Wdouble-promotion` y dejarlo **sin avisos**.

**Verificable:** una animación de 200 ms dura 200 ms tanto en PAL como en NTSC.

## 5. Elementos de tema

- [ ] 5.1 Añadir `FrostedPanel` **al final** de `elementsType[]` (no reordenar) con su `init`/`draw`.
- [ ] 5.2 Añadir `CardShelf` igual: fila horizontal, foco que crece y se eleva, no enfocadas atenuadas.
- [ ] 5.3 **Reescalador de portadas en el EE**: filtro de caja de la imagen nativa a un tile de 128×192 CT16, con caché. La imagen nativa NUNCA sube a VRAM. Es prerrequisito duro del `CardShelf` (ver 0.5).
- [ ] 5.3b `CardShelf` consume esos tiles. Reserva con el título cuando no haya portada.
- [ ] 5.4 Culling: dibujar sólo las tarjetas visibles o adyacentes.
- [ ] 5.5 **Regresión:** cargar un tema antiguo y confirmar que renderiza idéntico y que no reserva VRAM de blur.

**Verificable:** un tema de prueba con los dos elementos nuevos, más un tema antiguo intacto.

## 6. Tema PS5

- [ ] 6.1 `themes/PS5/conf_theme.cfg` componiendo `CardShelf` + `FrostedPanel`.
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
- [ ] 0.6 Validar el reescalado en el EE: medir el coste en ms de un filtro de caja sobre una portada de 720×512.

## 7. Medición final (no opcional)

- [ ] 7.1 Confirmar que el conjunto de trabajo del tema PS5 cabe en el pool encogido, en NTSC **y en PAL** (donde el margen es mucho menor).
- [ ] 7.2 Medir fps en PS2 física en NTSC y PAL, con y sin blur. Buscar **thrashing**: la caída se vería como pérdida de fps, no como fallo visual.
- [ ] 7.3 Probar en consola FAT y en SLIM.
- [ ] 7.4 Documentar los resultados en el propio cambio antes de archivarlo.

## 8. Upstream

- [ ] 8.1 Abrir la PR contra `ps2homebrew/Open-PS2-Loader` como cambio **aditivo**, remarcando que ningún tema existente se altera.
- [ ] 8.2 Adjuntar las mediciones de la fase 7. Sin números, la PR no es discutible.
