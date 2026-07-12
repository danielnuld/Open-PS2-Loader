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
rt1 (160×112, CT16S)
   ↓ 4 pasadas ping-pong rt1 ⇄ rt2, desplazando +0.5 téxel más cada vez
resultado (160×112)
```

Iterar un box blur converge a un gaussiano. Cada pasada trabaja a 160×112 sobre un chip de 2.4 Gpixel/s: el coste es ruido.

**Las pasadas van con `PrimAlphaEnable = OFF`.** Son copias, no composiciones. Si se dejara el blending activo, el canal alpha del framebuffer entraría en la ecuación — y en CT16S ese canal es de **un solo bit**, cuyo valor depende de detalles del formato que no controlamos. Apagarlo elimina toda esa clase de bugs.

**Anchos múltiplos de 64.** El GS exige que el `TBW` (Texture Buffer Width) sea múltiplo de 64 píxeles. 320 y 160 lo cumplen; 80 no. Por eso la pirámide para en /4 y el desenfoque se gana con iteraciones, no bajando más.

Los buffers son **CT16S aunque el framebuffer sea CT24**: el resultado se ve desenfocado y escalado, la pérdida de precisión de color es invisible, y cuesta la mitad.

## Decisión 4: presupuesto de VRAM y degradación

Éste es el riesgo real del cambio. La eDRAM son 4 MiB y OPL ya los usa.

| Modo | PSM | Framebuffers (×2) | Libre | Cadena de blur | % de lo libre |
|---|---|---|---|---|---|
| NTSC 640×448 | CT24 | 2,240 KiB | 1,856 KiB | 210 KiB | 11% |
| 480p 640×448 | CT24 | 2,240 KiB | 1,856 KiB | 210 KiB | 11% |
| PAL 640×512 | CT24 | 2,560 KiB | 1,536 KiB | 240 KiB | 16% |
| 704×480 | CT24 | 2,640 KiB | 1,456 KiB | 232 KiB | 16% |
| 720p / 1080i | CT16S | — | — | **deshabilitado** | no cabe |

Cuenta de la cadena en NTSC: `320·224·2 + 2·(160·112·2) = 143,360 + 71,680 = 215,040 B ≈ 210 KiB`.

**Pero ese 1.8 MiB "libre" ya lo consumen** el atlas de fuentes, el fondo del tema, los iconos y las portadas. Un tema pesado puede no dejar sitio.

Por eso:

1. La cadena de blur se reserva **después** de que el tema cargue sus texturas — es la última en pedir, y por tanto la primera en quedarse sin.
2. `gsKit_vram_alloc()` devuelve `GSKIT_ALLOC_ERROR` si no hay sitio. **Se comprueba.**
3. Si falla, `rmBlurAvailable()` devuelve falso y `rmDrawFrosted()` degrada a un rectángulo con tinte sólido. La UI sigue siendo perfectamente usable — sólo pierde el efecto.

Esto no es un plan B teórico: es el camino esperado en configuraciones con temas pesados, y hay que tratarlo como tal.

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
