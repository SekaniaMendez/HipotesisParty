# Hipótesis Party

Juego multijugador para una actividad de retroalimentación del curso **CI-0115**. La profesora dirige una sala compartida y cada equipo entra desde su propio dispositivo. Exploran un laberinto 3D, responden preguntas y reúnen evidencia exclusiva para resolver una hipótesis estadística diferente.

El juego tiene un **servidor autoritativo para red local**, cliente de navegador y cliente nativo para la Mac anfitriona. El arte 3D es original y estilizado; la profesora y los estudiantes son personajes ficticios. Las partidas se mantienen en memoria: todavía no hay cuentas, almacenamiento persistente ni alojamiento público integrado.

## Qué incluye

- Hasta seis equipos por sala, con nombre de equipo, nombre visible del avatar y seis apariencias de estudiantes: tres mujeres y tres hombres.
- Avatar propio de la profesora, también con nombre visible; controla el inicio, la pausa y la reanudación de la clase y puede recorrer el campus.
- Laberinto de 13 × 13 celdas generado por DFS aleatorio con retroceso iterativo. Los pasos respetan paredes; las rutas del cliente usan únicamente el mapa conocido.
- Niebla de exploración por equipo: se ven dos pasos a través de pasillos y se conserva el mapa descubierto. La profesora tiene una vista completa; los equipos reciben solamente los objetos y avatares que pueden ver.
- Preguntas fáciles que conceden **cinco movimientos al acertar o dos al fallar**. Cada paso consume uno; al agotarlos aparece otra pregunta.
- **Diez glifos difíciles por equipo**, respondidos una sola vez. Cada acierto suma 100 puntos.
- **Cuatro pistas exclusivas por equipo**, con datos y contexto de un caso propio. La entrega final evalúa H1, z, p y la decisión sobre H0: 100 puntos por criterio, hasta **400 puntos adicionales** y **1400 puntos en total**.
- Cámara orbital, vista aérea, minimapa, expediente de pistas, clasificación compartida y resultados con podios.
- Carga incremental de modelos, animaciones y fuentes con porcentaje, registro de éxitos/fallos y reintento.
- Siete personajes animados con `Idle`, `Walk` y `Celebrate`, más piso, paredes, columnas, podios y glifos hechos en Blender.

La carga de assets ocurre en el hilo principal, un archivo por cuadro; un archivo grande puede retrasar ese cuadro. Blender se necesita para editar o regenerar los modelos, **no para jugar**.

## Compilar y abrir una partida

Requisitos: compilador **C++20**, **CMake 4.0 o posterior**, **raylib 6.2**, **libcurl** para el cliente nativo, **Python 3.9 o posterior** para el anfitrión y **Emscripten** para el cliente web. La primera configuración web obtiene raylib 6.2 desde su repositorio.

Desde la raíz del proyecto:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Debug
cmake --build build-web --parallel
python3 game-server/server.py
```

Abrir [el campus local](http://127.0.0.1:8001/). En otra terminal también se puede ejecutar el cliente de escritorio:

```sh
./build/HypotesisParty
```

El cliente nativo se conecta a `127.0.0.1:8001`; se puede cambiar mediante la variable `HP_SERVER_URL`, por ejemplo `HP_SERVER_URL=http://192.168.1.20:8001 ./build/HypotesisParty`. El navegador utiliza el mismo anfitrión desde el que se abrió la página. El servidor debe permanecer activo durante la partida. **`python3 -m http.server` no permite jugar:** solo sirve archivos y no implementa la API multijugador.

La compilación nativa copia los assets de ejecución a `build/assets/`; deben permanecer junto al ejecutable. La distribución web utiliza `HypotesisParty.html`, `.js`, `.wasm` y `.data`, generados en `build-web/`. La página `web/shell.html` muestra la descarga de Emscripten antes de que empiece la carga incremental interna. El `.blend` no forma parte del paquete web.

## Compartir la clase por LAN

Para aceptar conexiones de los demás dispositivos de la misma red:

```sh
python3 game-server/server.py --host 0.0.0.0 --port 8001
```

Cada dispositivo abre `http://IP_DE_LA_MAC:8001/`, usando la dirección local de la Mac anfitriona indicada en los ajustes de red.

1. La profesora elige **Soy la profesora**, escribe el nombre de su avatar y crea la sala.
2. Comparte la dirección del campus y el código de seis caracteres.
3. Cada equipo elige **Soy estudiante**, escribe su nombre de avatar, nombre de equipo y código; luego selecciona una apariencia y entra.
4. La profesora pulsa **Iniciar partida** cuando todos estén listos. Después del inicio no se agregan equipos nuevos.
5. Cada equipo explora, reúne sus cuatro pistas, responde los diez glifos y entrega su resolución en **Mi hipótesis**.

Los navegadores conservan la credencial de sala en `sessionStorage`, por lo que la misma pestaña puede restaurarla al recargar mientras siga existiendo la sala. La profesora dispone de clasificación y controles de clase; cada equipo conserva su propia información y puntuación. Los detalles operativos y el protocolo están en [Multijugador](docs/multiplayer.md).

## Controles

| Contexto | Control | Acción |
| --- | --- | --- |
| Pantalla de carga | `Enter` o botón | Entrar al campus después de cargar |
| Acceso | Clic y teclado | Elegir rol y completar nombre, equipo y código |
| Acceso, sin campo activo | `←` / `→`, `1`–`6` o botones | Elegir apariencia de estudiante |
| Acceso | `Enter` | Crear sala o unirse con los datos escritos |
| Sala de espera | Botón de la profesora | Iniciar la expedición |
| Exploración | `WASD` / flechas | Caminar por celdas: norte/sur/oeste/este del mapa |
| Exploración | Clic en el piso conocido | Seguir una ruta válida, paso a paso |
| Equipo | `H` | Buscar una ruta conocida hacia un objetivo visible o un borde de exploración |
| Exploración | `Q` / `E`, rueda | Girar cámara y ajustar distancia |
| Exploración | `Tab` | Alternar vista aérea |
| Equipo | `J` | Abrir/cerrar **Mi hipótesis** y sus pistas |
| Profesora | `J` | Ver clasificación de la clase |
| Exploración | `Esc` | Cerrar el expediente o abrir el menú local |
| Pregunta | `1`, `2`, `3` o clic | Elegir respuesta |
| Respuesta registrada | `Enter` / espacio o botón | Continuar |
| Entrega final | Campos y botones del expediente | Indicar H1, z, p y decisión; enviar una vez |
| Profesora | Botones de clase | Pausar o reanudar para todos |

Las preguntas bloquean el movimiento hasta responder y continuar. El menú local no pausa la clase para los demás. La entrega final se habilita al completar los diez glifos y reunir las cuatro pistas; ingresar z con dos decimales y p con al menos tres. Se aceptan diferencias de hasta `0.03` para z y `0.0005` para p.

## Verificación

Las pruebas del núcleo y de las bases de interfaz no necesitan una ventana:

```sh
ctest --test-dir build --output-on-failure
python3 tools/validate_assets.py
```

Las pruebas de servidor abren puertos temporales solo en `127.0.0.1` y usan workers C++ reales; no necesitan un anfitrión ya iniciado ni una GPU:

```sh
python3 -m unittest discover -s game-server/tests -p 'test_*.py'
```

Cubren equipos independientes, permisos, niebla y pistas privadas, reglas de movimiento, preguntas, pausa, reintentos concurrentes, validación de entrada, fallos del worker y una expedición completa con diez glifos, cuatro pistas y evaluación final.

Las comprobaciones gráficas requieren una sesión de macOS con OpenGL:

```sh
./build/raylib-client/ModelAssetsTests
```

`ModelAssetsTests` carga los 19 recursos 3D y comprueba los 21 clips de los siete personajes, sus poses, geometría y dibujo con raylib real.

Con el servidor activo en `127.0.0.1:8001`, ejecutar:

```sh
./build/HypotesisParty --smoke-test
```

Esta prueba gráfica crea una sala y un equipo, inicia mediante la profesora, verifica que el equipo recibe un mapa limitado y responde una pregunta fácil con recompensa de dos o cinco pasos. Termina con `SMOKE: PASS` y código cero al completar ese flujo. Las pruebas HTTP, por separado, verifican la progresión completa. Las capturas del smoke se guardan junto al ejecutable, normalmente `build/`: `smoke-loading.png`, `smoke-menu.png`, `smoke-exploration.png`, `smoke-event.png` y `smoke-fog.png`.

## Assets, tipografía y autoría

El inventario, medidas, rigs e instrucciones de edición están en [Assets 3D de la academia](docs/assets.md). Los GLB exportados están en `assets/models/`; la escena editable está en `assets/source/academy.blend`.

Para regenerarlos en esta Mac:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python tools/generate_assets.py
python3 tools/validate_assets.py
cmake --build build --parallel
cmake --build build-web --parallel
```

El generador sobrescribe sus propios modelos y escena fuente: guardar cualquier edición manual en otro `.blend` antes de ejecutarlo. Los personajes usan 16 huesos y pesos rígidos por pieza; no incluyen expresiones faciales ni deformación anatómica avanzada.

Los títulos usan **Playfair Display**, reutilizada de `RaylibGames/Caeli`, y los botones y textos usan **DejaVu Sans**, disponible en `TopoTools`. Se conservan sus licencias en [OFL de Playfair Display](assets/fonts/OFL_PlayfairDisplay.txt) y [licencia de DejaVu](assets/fonts/LICENSE_DEJAVU).

La UI y el ciclo de escenas adaptan componentes de `treasure_labyrinth`; consultar [avisos de terceros](docs/third-party-notices.txt). El protocolo C++ utiliza una copia local de nlohmann JSON con su [licencia MIT](third_party/nlohmann/LICENSE.MIT).

## Arquitectura

| Ubicación | Responsabilidad |
| --- | --- |
| `core/Maze.cpp` | Generación determinista, paredes, vecinos, distancias y rutas |
| `core/TeamSession.cpp` | Reglas autoritativas, preguntas, pistas exclusivas, visibilidad y evaluación final |
| `core/include/EventBus.h` | Eventos tipados del núcleo, independientes de raylib |
| `game-server/SessionWorker.cpp` | Una partida C++ por sala y serialización filtrada por participante |
| `game-server/server.py` | HTTP, salas, tokens, permisos, reintentos y distribución del cliente web |
| `raylib-client/SessionClient.cpp` | Transporte asíncrono: `fetch` en navegador y libcurl en escritorio |
| `raylib-client/PartyGame.cpp` | Acceso, entrada, escenas y render del estado autorizado recibido |
| `raylib-client/SceneManager.cpp` | Registro de escenas y transiciones diferidas `Enter/Exit` |
| `raylib-client/PartyUI.cpp` | Botones, textos, paneles y escalado |
| `raylib-client/ResourceManager.cpp` | Cola de carga, historial y propiedad de recursos CPU/GPU |
| `assets/shaders/` | Iluminación GLSL 330 de escritorio y GLSL 100 de WebGL |
| `tools/` | Generación con Blender y validación de GLB |

`TeamSession` publica eventos como `TeamMoved`, `TeamQuestionAnswered`, `TeamClueCollected`, `TeacherMoved` y `TeamFinalSubmitted`. El worker procesa sus cambios; el cliente consulta snapshots con revisión y nunca decide puntuaciones ni descubre celdas por su cuenta. Las transiciones de escena se aplican fuera del callback activo.

Las preguntas y explicaciones están en `easyBank` y `hardBank`, y los casos estadísticos en `cases`, dentro de **`core/TeamSession.cpp`**. Los seis casos actuales usan una prueba Z para una media con desviación poblacional conocida, e incluyen alternativas de una y dos colas. Las pistas se construyen en `ClueTexts`. Esas reglas se ejecutan en el servidor y no se envían al navegador como un banco de respuestas; los snapshots incluyen únicamente la pregunta activa y la evidencia ya descubierta por el equipo.
