# Verificación del prototipo — 23 de octubre de 2026

Flujo: crear sala desde navegador → generar laberinto con C++ original → conectar
roles mediante FastAPI/WebSockets → validar movimientos y preguntas → mostrar
estado en raylib y cerrar con podio.

| Límite verificado | Resultado | Evidencia |
|---|---|---|
| Compilación C++ original | Correcta | CMake construye `maze_export`; DFS y BFS conectan todas las casillas con pasillos recíprocos. |
| Cliente raylib web | Correcto | Emscripten 4.0.23 + raylib 5.5; se cargan JS, WASM y texturas originales. |
| Entrada y roles | Correctos | Creación de sala y entrada de un estudiante desde la interfaz; vistas de profesora y proyector conectadas. |
| Navegador → WebSocket → mapa | Correcto | Movimiento por botón direccional y cambio de mapa; consola sin errores durante la revisión. |
| 22 estudiantes simultáneos | Correcto | `scripts/smoke.py` abre 25 conexiones reales; todos los movimientos confirmados en 63,6 ms en la ejecución final local. |
| Pausa y reconexión | Correctas | Prueba real confirma pausa/reanudación y misma identidad tras reconectar, con 22 estudiantes registrados. |
| Preguntas y capturas | Correctas en pruebas de servidor | Se verifican puntuación, tiempos, escudos, respuesta privada y prevención de premios repetidos. La redacción de las preguntas requiere validación del curso. |
| Cierre → interfaz | Correcto | Cierre confirmado desde panel y podio visible; prueba real confirma estado final en todos los clientes. |
| Presentación móvil | Correcta en navegador | Vista de 390 × 844 con mapa y botones direccionales; no sustituye pruebas en dispositivos físicos. |

`uv run pytest -q`: **12 passed**. JavaScript pasa `node --check` y los cambios
pasan `git diff --check`. Starlette emite un aviso de deprecación de su adaptador
de pruebas HTTPX; no hay fallos de pruebas.

La prueba real final recibió 601 snapshots, confirmó reconexión con la misma
identidad y cierre en los 25 clientes. Estas mediciones son locales y no son una
garantía de latencia en Wi-Fi o Internet.

Capturas: [proyector](../artifacts/projector.jpg) y
[estudiante en pantalla móvil](../artifacts/student-mobile.jpg).

Pendiente fuera del alcance de este prototipo: ensayo en la red del aula con
celulares físicos, contenido de inferencia validado, evaluación por LLM, misterio
final, persistencia y alojamiento público.
