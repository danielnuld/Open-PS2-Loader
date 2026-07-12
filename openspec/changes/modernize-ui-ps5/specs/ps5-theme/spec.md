## ADDED Requirements

### Requirement: Tema PS5 incluido

OPL SHALL incluir un tema llamado `PS5` que componga `CardShelf` y `FrostedPanel` sobre el arte de portadas que OPL ya descarga.

El tema NO SHALL ser el predeterminado en esta iteración: el usuario lo elige explícitamente.

#### Scenario: Selección del tema

- **WHEN** el usuario selecciona el tema PS5 en los ajustes
- **THEN** la interfaz SHALL presentar la fila de tarjetas y los paneles de cristal
- **AND** SHALL poder volver a cualquier otro tema sin reiniciar

### Requirement: El fondo es la portada enfocada

El wallpaper SHALL ser el arte de la portada del juego enfocado, escalado a pantalla completa y desenfocado, con un velo en degradado que garantice contraste para el texto.

Es el comportamiento real de la PS5, y aprovecha arte que OPL ya tiene en memoria.

#### Scenario: Cambio de foco

- **WHEN** el foco pasa a otro juego
- **THEN** el wallpaper SHALL transicionar al arte del juego nuevo
- **AND** el fondo desenfocado que se ve a través de los paneles SHALL actualizarse en consecuencia

#### Scenario: Contraste del texto garantizado

- **WHEN** la portada enfocada es una imagen clara
- **THEN** el velo en degradado SHALL mantener legible el texto blanco de la interfaz

### Requirement: Degradación en hardware limitado

El tema SHALL renderizarse de forma correcta y legible aunque el desenfoque esté deshabilitado.

#### Scenario: Sin desenfoque

- **WHEN** el tema PS5 se carga y el desenfoque no está disponible
- **THEN** los paneles SHALL dibujarse con tinte sólido
- **AND** el wallpaper SHALL dibujarse nítido con el velo
- **AND** la interfaz SHALL seguir siendo utilizable
