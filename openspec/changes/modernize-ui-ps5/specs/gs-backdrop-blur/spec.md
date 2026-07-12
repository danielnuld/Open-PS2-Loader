## ADDED Requirements

### Requirement: Desenfoque del fondo sin shaders

El sistema SHALL producir una versión desenfocada del framebuffer actual usando únicamente las capacidades de función fija del Graphics Synthesizer (muestreo del framebuffer como textura, filtrado bilineal y render-to-texture), sin depender de shaders programables.

El desenfoque SHALL exponerse mediante `rmBlurBackdrop()`, que captura el framebuffer tal como está en el momento de la llamada y NO SHALL modificarlo.

#### Scenario: El fondo se desenfoca y el framebuffer no se altera

- **WHEN** se llama a `rmBlurBackdrop()` tras dibujar el fondo y antes de dibujar los paneles
- **THEN** queda disponible una textura con el contenido del framebuffer desenfocado
- **AND** el framebuffer conserva intacto su contenido original
- **AND** el destino de dibujado activo del GS queda restaurado al framebuffer

#### Scenario: El orden de dibujado se respeta al cambiar de destino

- **WHEN** la cadena de desenfoque reapunta el destino de dibujado a un buffer de VRAM
- **THEN** los primitivos encolados previamente SHALL haberse ejecutado ya contra el destino anterior
- **AND** ningún primitivo SHALL dibujarse en un destino distinto al que le correspondía

### Requirement: Presupuesto de VRAM verificado en runtime

La cadena de desenfoque SHALL reservar su VRAM sólo después de que el tema haya cargado sus texturas, y SHALL verificar que la reserva tuvo éxito antes de habilitarse.

El sistema NO SHALL asumir que la VRAM está disponible: los 4 MiB de eDRAM son el recurso escaso y un tema pesado puede agotarlos.

#### Scenario: No hay VRAM suficiente

- **WHEN** la reserva de los buffers de desenfoque falla por falta de VRAM
- **THEN** el desenfoque SHALL quedar deshabilitado
- **AND** la interfaz SHALL seguir renderizando correctamente
- **AND** los paneles de cristal SHALL caer a un tinte sólido sin desenfoque

#### Scenario: Modo de alta resolución

- **WHEN** el modo de vídeo activo es 720p o 1080i
- **THEN** el desenfoque SHALL estar deshabilitado sin intentar reservar VRAM

### Requirement: Panel de cristal esmerilado

El sistema SHALL exponer `rmDrawFrosted()`, que dibuja una región mostrando el fondo desenfocado con un tinte translúcido encima.

El fondo desenfocado SHALL dibujarse **sin alpha blending**, para no depender del canal alpha del framebuffer (que en CT16S es de un solo bit). El tinte SHALL dibujarse encima, ese sí compuesto.

#### Scenario: Panel sobre fondo desenfocado

- **WHEN** se llama a `rmDrawFrosted()` con una región y un color de tinte
- **THEN** la región SHALL mostrar el fondo desenfocado correspondiente a esas coordenadas de pantalla
- **AND** el tinte SHALL componerse encima con su alpha

#### Scenario: Desenfoque deshabilitado

- **WHEN** el desenfoque está deshabilitado (sin VRAM, o modo hi-res)
- **THEN** `rmDrawFrosted()` SHALL dibujar únicamente el tinte, como un rectángulo translúcido
- **AND** NO SHALL producir artefactos visuales ni leer VRAM sin reservar

### Requirement: Coste acotado por frame

La cadena de desenfoque SHALL operar a resolución reducida (máximo la mitad del ancho de pantalla en el primer nivel) y SHALL consumir menos del 20% del presupuesto de un frame a 60 Hz.

#### Scenario: El desenfoque no rompe los 60 Hz

- **WHEN** el desenfoque está activo en NTSC 640×448
- **THEN** la interfaz SHALL mantener 60 fps estables
