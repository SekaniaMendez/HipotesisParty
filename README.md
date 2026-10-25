# Hipótesis Party

Prototipo de laberinto multijugador para la actividad del curso CI-0115.
Hasta **22 estudiantes** juegan desde sus celulares. La profesora controla su
personaje y el proyector sigue su recorrido. El director administra la partida.

## Qué funciona

- Sala con código de cinco caracteres, apodos y reconexión con la misma identidad.
- Cliente **raylib 5.5, C++ y WebAssembly/WebGL 2**, con teclado y botones táctiles.
- Servidor **FastAPI + WebSockets**, que valida movimientos, colisiones, roles,
  cadencia de movimiento, capturas, preguntas y puntuación.
- Generación DFS/BFS usando código C++ original de `treasure_labyrinth`, con
  pasillos adicionales para escapar y sus texturas originales.
- Vista limitada por distancia a través de los pasillos; los clientes reciben
  solamente casillas y personajes visibles. El director puede ver todo el mapa.
- Ocho estaciones de pistas por persona; +10 por estación resuelta.
- Capturas: desafío de 45 segundos, +20 si se acierta, −5 si se falla, mínimo 0.
  Al terminar, el estudiante reaparece lejos de la profesora con 8 s de protección.
- Preguntas de estación de 30 s; protección durante la pregunta y 8 s después.
  Una pista resuelta no puede volver a sumar puntos. Tras fallar, espera de 10 s.
- Pausa que congela reloj, preguntas y protecciones. Podio al cerrar o vencer el tiempo.
- Sonido opcional de pasos y capturas, activado por un botón del usuario.

## Ejecutar en desarrollo

Requisitos: Git, CMake, compilador C++20, Python 3.12 y [uv](https://docs.astral.sh/uv/).
El SDK de Emscripten y raylib se descargan **dentro del proyecto**, en `.tools/`.
El primer build web necesita conexión a Internet y espacio para el SDK.

```sh
uv sync
bash scripts/build-native.sh
bash scripts/setup-web.sh
bash scripts/dev.sh
```

Abrí [http://localhost:8000](http://localhost:8000). Para los próximos arranques
solo hace falta `bash scripts/dev.sh`. Después de editar `client/main.cpp`, ejecutá
`bash scripts/build-web.sh` y recargá el navegador. Después de editar el backend,
reiniciá `scripts/dev.sh`; no se activa recarga automática porque borraría las salas.

### Jugar desde celulares

1. Conectá la computadora y los celulares a la misma red Wi-Fi.
2. En la computadora, abrí `http://IP-DE-LA-COMPUTADORA:8000` en lugar de localhost.
3. Entrá por **Soy el director de la partida** y creá una sala.
4. Compartí la invitación con los estudiantes. Cada persona escribe su propio apodo.
5. Abrí **control de la profe** en su dispositivo y **proyector** en la pantalla pública.
6. Iniciá la partida desde el panel del director.

En macOS podés consultar la IP de Wi-Fi con `ipconfig getifaddr en0`. La interfaz
puede variar. Si el sistema pregunta por conexiones entrantes, permití el servidor
Python. Algunas redes de campus aíslan dispositivos: ensayá en la red del aula;
un punto de acceso propio o un servidor público con HTTPS serían alternativas.
Una invitación copiada desde localhost solo funciona en esa computadora.

Estudiantes y profesora se mueven con WASD/flechas o los botones direccionales.
En una estación marcada con `?`, el estudiante pulsa **Investigar** o la tecla E.
La profesora captura al compartir casilla con un estudiante sin protección.
Mientras este responde al desafío, la profesora espera; luego continúa su ruta.
El proyector siempre sigue a la profesora y no muestra las respuestas privadas.

Los enlaces de profesora, director y proyector tienen capacidades privadas en el
fragmento de la URL. Compartí cada uno solo con su participante. Al recargar se
recupera el personaje; abrir el mismo enlace en dos pestañas desconecta la anterior.
Para simular varios estudiantes en una computadora, usá perfiles de navegador
distintos: la misma sala y perfil restauran a su estudiante guardado.

## Arquitectura

```text
client/main.cpp                 Renderer raylib; recibe snapshots desde JavaScript
web/                            Entrada, controles táctiles, preguntas y paneles HTML
server/app.py                   HTTP, WebSockets, colas por conexión, tick de 10 Hz
server/game.py                  Estado autoritativo y reglas de la partida
server/content.json             Preguntas provisionales, opciones y explicación
native/maze_export.cpp          Adaptador CLI al generador C++ original
vendor/treasure_labyrinth/      Algoritmos originales y licencia MIT
scripts/                        Compilación, ejecución y prueba con 25 conexiones
```

Un solo proceso Uvicorn guarda las salas **en memoria**. No usar múltiples workers:
no comparten ese estado. Reiniciar el servidor elimina las partidas. El prototipo
no incluye persistencia, cuentas ni despliegue público. Las salas inactivas sin
conexiones se eliminan después de dos horas; máximo 20 salas por proceso.

FastAPI sirve la interfaz, los assets y los WebSockets desde el mismo origen, sin
Node ni un proceso separado para el frontend. El renderer no decide puntuación ni
posiciones: envía intención de movimiento y dibuja el resultado validado.

## Contenido del curso

Las preguntas actuales son **demostraciones de media, mediana, dispersión y
probabilidad**, todavía no una actividad validada de inferencia de hipótesis.
Editá `server/content.json` cuando esté disponible el temario y reiniciá el servidor.
Cada pregunta tiene `prompt`, cuatro `choices`, `answer` (índice 0–3) y `explanation`.

La evaluación por LLM y las respuestas abiertas quedan para una siguiente etapa;
este prototipo funciona sin API ni clave. El cliente nunca recibe la respuesta
correcta antes de responder. El misterio final tipo Clue también está pendiente.

## Verificación

```sh
uv run pytest -q
# Con scripts/dev.sh corriendo, prueba HTTP + 25 WebSockets reales:
uv run python scripts/smoke.py
```

Las pruebas cubren conectividad y reciprocidad de DFS/BFS, límite de 22 personas,
roles, visibilidad, movimientos, capturas, respuestas, repetición de puntuaciones,
pausa, límite de tiempo y reconexión. La prueba real abre 22 estudiantes, profesora,
director y proyector, mueve simultáneamente a los estudiantes y cierra la partida.
La simulación local no sustituye un ensayo con celulares físicos en la red del aula.

## Créditos

Basado en Treasure Labyrinth de Giovanni Mendez, licencia MIT. Se conservan
DFS/BFS, Node, utilidades y texturas. Ver
[origen y atribución](vendor/treasure_labyrinth/ORIGIN.md) y
[licencia original](vendor/treasure_labyrinth/LICENSE).
El proyecto fuente se conserva sin modificaciones.
