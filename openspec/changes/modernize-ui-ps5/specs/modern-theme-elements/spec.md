## ADDED Requirements

### Requirement: Compatibilidad con los temas existentes

Los tipos de elemento nuevos SHALL añadirse al registro `elementsType[]` sin alterar el orden, la semántica ni el comportamiento de los tipos ya existentes.

Un tema que no declare los tipos nuevos SHALL renderizar exactamente igual que antes del cambio.

#### Scenario: Tema antiguo sin modificar

- **WHEN** se carga un tema creado antes de este cambio
- **THEN** SHALL renderizar de forma idéntica a como lo hacía antes
- **AND** NO SHALL reservar VRAM para la cadena de desenfoque si ningún elemento la necesita

#### Scenario: Tipo de elemento desconocido

- **WHEN** un tema declara un tipo de elemento que la build no reconoce
- **THEN** SHALL ignorarse con un aviso en el log, como cualquier otro elemento desconocido
- **AND** el resto del tema SHALL cargarse normalmente

### Requirement: Elemento FrostedPanel

El sistema SHALL soportar un tipo de elemento `FrostedPanel` que dibuja una región mostrando el fondo desenfocado con un tinte translúcido encima.

El elemento SHALL aceptar los atributos comunes de posición, tamaño y alineación, más un color de tinte.

#### Scenario: Panel declarado en el tema

- **WHEN** un tema declara un `FrostedPanel` con posición, tamaño y color
- **THEN** SHALL dibujarse mostrando el fondo desenfocado en esa región
- **AND** SHALL respetar el sistema de coordenadas y escalado del tema

#### Scenario: Panel sin desenfoque disponible

- **WHEN** el desenfoque está deshabilitado
- **THEN** el `FrostedPanel` SHALL dibujarse como un rectángulo con su color de tinte

### Requirement: Elemento CardShelf

El sistema SHALL soportar un tipo de elemento `CardShelf` que presenta la lista de juegos como una fila horizontal de tarjetas, donde la tarjeta enfocada crece, se eleva y se destaca.

Las tarjetas SHALL usar el arte de portada que OPL ya gestiona. Las no enfocadas SHALL atenuarse.

#### Scenario: Navegación

- **WHEN** el usuario mueve el foco a la tarjeta siguiente
- **THEN** la fila SHALL desplazarse con una animación suave
- **AND** la tarjeta nueva SHALL crecer y elevarse
- **AND** la anterior SHALL volver a su tamaño base y atenuarse

#### Scenario: Portada no disponible

- **WHEN** un juego no tiene arte de portada descargado
- **THEN** SHALL dibujarse una tarjeta de reserva con el título del juego
- **AND** NO SHALL romperse el layout de la fila

#### Scenario: Sólo se dibuja lo visible

- **WHEN** la lista contiene cientos de juegos
- **THEN** SHALL dibujarse únicamente las tarjetas dentro o cerca de la pantalla
- **AND** SHALL mantenerse 60 fps

### Requirement: Las portadas del shelf se reescalan al cargar

OPL sube las portadas a VRAM a la **resolución nativa** del PNG, con un límite por textura de 720×512×4 = 1,440 KiB. Una sola portada puede así ocupar casi todo el pool del TexManager (1,856 KiB en NTSC).

El `CardShelf` dibuja varias portadas a la vez. Por tanto, NO SHALL usar las texturas de portada a resolución nativa.

Las portadas del shelf SHALL reescalarse en el EE al cargarse, a un tile fijo de **128×192**, y SHALL almacenarse en **`GS_PSM_CT16`** (48 KiB por portada).

El reescalado SHALL hacerse en el EE y NO mediante render-to-texture, para evitar subir la imagen nativa a VRAM ni siquiera de forma transitoria.

#### Scenario: Portada de alta resolución

- **WHEN** el arte de un juego es un PNG de 512×720
- **THEN** SHALL reescalarse a 128×192 CT16 antes de subirse a VRAM
- **AND** la textura nativa NUNCA SHALL residir en VRAM
- **AND** el coste en VRAM de esa portada SHALL ser de 48 KiB

#### Scenario: El conjunto de trabajo cabe en el pool

- **WHEN** el shelf muestra 7 portadas simultáneas con el desenfoque activo
- **THEN** el conjunto de trabajo de texturas SHALL caber en el pool del TexManager tanto en NTSC como en PAL
- **AND** NO SHALL producirse re-subida de texturas en cada frame

#### Scenario: El reescalado se cachea

- **WHEN** una portada ya reescalada vuelve a entrar en la ventana visible
- **THEN** SHALL reutilizarse el resultado cacheado
- **AND** NO SHALL volver a reescalarse
