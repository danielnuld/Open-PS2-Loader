# Modernizar la interfaz de OPL con un lenguaje visual tipo PS5

## Why

La interfaz de OPL es funcional pero visualmente ancla el proyecto en 2010: listas planas, sin profundidad, sin animación, sin desenfoque. La percepción generalizada es que la PS2 "no da para más". Es falsa.

Un prototipo independiente ([`ps2-modern-ui`](https://github.com/danielnuld/ps2-modern-ui)) ya demostró en hardware/PCSX2 que el Graphics Synthesizer puede componer una interfaz moderna a 60 Hz — incluido el **backdrop blur** ("cristal esmerilado"), que es el efecto que define el look de PS5 y que todo el mundo asume que requiere shaders. No los requiere: el GS puede samplear su propio framebuffer y encadenar taps bilineales. Cuesta una fracción de un frame.

OPL además ya tiene la infraestructura entera: gsKit, FreeType, un sistema de temas dirigido por datos, y descarga de arte de portadas. **No hace falta reescribir nada. Hace falta añadir tres piezas.**

## What Changes

- **Backdrop blur en `renderman`**: render-to-texture sobre gsKit (reapuntando `FRAME` a un buffer de VRAM), pirámide de reducción y pasadas de ping-pong bilineales. API nueva: `rmBlurBackdrop()` / `rmDrawFrosted()`.
- **Motor de animación**: interpolación con easing basada en tiempo, para foco, escala y desplazamiento. OPL hoy no tiene ninguna.
- **Elementos de tema nuevos**: `FrostedPanel` (panel de cristal) y `CardShelf` (fila de tarjetas con foco animado que crece y se eleva). Se registran en `elementsType[]` como cualquier otro elemento.
- **Tema `PS5` incluido**: usa los elementos nuevos y el arte de portadas que OPL ya descarga. El wallpaper es la portada enfocada, estirada y desenfocada — el comportamiento real de la PS5.
- **Degradación explícita**: en modos de alta resolución (720p/1080i) y cuando la VRAM no alcanza, el blur se desactiva y los paneles caen a un tinte sólido. La UI sigue funcionando.

**No hay cambios rompientes.** Todo es aditivo: los temas existentes no usan los elementos nuevos y siguen renderizando igual.

## Capabilities

### New Capabilities

- `gs-backdrop-blur`: desenfoque del fondo por render-to-texture en el GS, sin shaders, con presupuesto de VRAM explícito y degradación cuando no cabe.
- `ui-animation`: interpolación con easing basada en tiempo (no en frames) para propiedades de elementos de UI.
- `modern-theme-elements`: los tipos de elemento `FrostedPanel` y `CardShelf`, disponibles para cualquier tema.
- `ps5-theme`: el tema incluido que compone los anteriores.

### Modified Capabilities

Ninguna. No existen specs previas en `openspec/specs/`, y este cambio es puramente aditivo sobre el comportamiento actual.

## Impact

**Código afectado:**

- `src/renderman.c`, `include/renderman.h` — API de blur y render-to-texture. Es el cambio de más riesgo.
- `src/themes.c`, `include/themes.h` — dos entradas nuevas en `elementsType[]` con su `init`/`draw`.
- `src/menusys.c` — enganche del reloj de animación al bucle de dibujado.
- Nuevo: `src/rmblur.c` — la cadena de desenfoque, aislada del resto de `renderman`.
- Nuevo: `themes/PS5/` — el tema incluido.

**Coste en VRAM** (el recurso escaso: 4 MiB de eDRAM):

| Modo | Framebuffers | Libre para texturas | Cadena de blur | Margen |
|---|---|---|---|---|
| NTSC 640×448 CT24 | 2,240 KiB | 1,856 KiB | **210 KiB** | ~11% de lo libre |
| 480p 640×448 CT24 | 2,240 KiB | 1,856 KiB | **210 KiB** | ~11% de lo libre |
| PAL 640×512 CT24 | 2,560 KiB | 1,536 KiB | **240 KiB** | ~16% de lo libre |
| 720p / 1080i | — | — | **desactivado** | no cabe |

El blur usa CT16S (16 bpp) aunque el framebuffer sea CT24: el resultado se muestra desenfocado y a escala, así que la pérdida de precisión de color es imperceptible, y cuesta la mitad.

**Dependencias:** ninguna nueva. gsKit y FreeType ya están.

**Riesgo principal:** OPL ya carga portadas, iconos, fondo y atlas de fuentes en esos ~1.8 MiB. Los 210 KiB del blur pueden no caber con un tema pesado. Por eso la asignación es *condicional y verificada en runtime*, con degradación limpia. Ver `design.md`.
