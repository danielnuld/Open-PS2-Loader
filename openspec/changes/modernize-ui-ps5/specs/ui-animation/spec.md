## ADDED Requirements

### Requirement: Interpolación basada en tiempo

El sistema SHALL interpolar propiedades de la interfaz en función del **tiempo transcurrido**, no del número de frames.

Esto es obligatorio porque OPL corre a 50 Hz (PAL) y 60 Hz (NTSC), y además puede perder frames. Una animación indexada por frames correría a distinta velocidad según la región.

#### Scenario: Misma duración en PAL y NTSC

- **WHEN** una animación de 200 ms se ejecuta en modo PAL (50 Hz) y en modo NTSC (60 Hz)
- **THEN** SHALL tardar 200 ms en ambos casos

#### Scenario: Frames perdidos

- **WHEN** el renderizado pierde frames
- **THEN** la animación SHALL seguir avanzando según el tiempo real transcurrido
- **AND** NO SHALL ralentizarse

### Requirement: Aritmética en punto flotante de precisión simple

Toda la aritmética de animación SHALL usar `float`. El código NO SHALL introducir variables ni literales de tipo `double`.

El R5900 no tiene doble precisión en hardware: cada operación con `double` se emula por software vía libgcc, con un coste de uno a dos órdenes de magnitud.

#### Scenario: Sin doubles en el bucle de animación

- **WHEN** se compila el módulo de animación con `-Wdouble-promotion`
- **THEN** NO SHALL emitirse ningún aviso de promoción a `double`

### Requirement: Easing exponencial hacia un objetivo

El sistema SHALL proveer una interpolación con suavizado exponencial hacia un valor objetivo, independiente del framerate.

#### Scenario: Convergencia al objetivo

- **WHEN** una propiedad animada tiene un valor objetivo distinto al actual
- **THEN** SHALL aproximarse a él de forma monótona y desacelerando
- **AND** SHALL considerarse asentada cuando la diferencia caiga bajo un umbral

#### Scenario: Cambio de objetivo a mitad de animación

- **WHEN** el objetivo cambia antes de que la animación termine
- **THEN** SHALL redirigirse suavemente desde el valor actual, sin saltos
