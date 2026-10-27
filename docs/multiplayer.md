# Partidas multijugador en la red local

El anfitrión mantiene una partida autoritativa compartida. La profesora y cada equipo abren el cliente desde su propio dispositivo; el servidor decide movimientos, respuestas, puntuaciones y qué información puede ver cada participante. Hay hasta seis equipos por sala, cada uno con su nombre, avatar, caso estadístico, diez glifos y cuatro pistas exclusivas.

## Iniciar una clase

Desde la raíz del repositorio, compilar el servidor nativo y el cliente web:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Debug
cmake --build build-web --parallel
```

Para probar todo en esta Mac:

```sh
python3 game-server/server.py
```

Abrir [el campus local](http://127.0.0.1:8001/). Para que entren los demás dispositivos de la misma red, iniciar el anfitrión así:

```sh
python3 game-server/server.py --host 0.0.0.0 --port 8001
```

Cada participante debe abrir `http://IP_DE_LA_MAC:8001/`, sustituyendo `IP_DE_LA_MAC` por la dirección local mostrada en los ajustes de red de macOS. Los dispositivos deben poder comunicarse entre sí: algunas redes de invitados aíslan a sus clientes. El programa no cambia la configuración del firewall.

La profesora crea una sala y comparte su código de seis caracteres. Cada equipo se une con un nombre distinto y elige nombre y aspecto para su avatar. La profesora inicia la expedición cuando están listos; una vez iniciada, se cierra la inscripción. Puede pausar, reanudar y caminar con su propio personaje.

El comando permite cambiar `--port`, `--worker` y `--web-dir`. Por defecto, el ejecutable es `build/game-server/SessionWorker` y los archivos del navegador están en `build-web/`. Este servidor reemplaza a `python3 -m http.server`: el servidor estático simple no implementa las salas ni las acciones del juego.

## Reglas de la expedición

- Cada equipo empieza sin movimientos. Responder una pregunta fácil da **cinco pasos si acierta y dos si falla**. Cada paso válido consume uno. Al agotarlos aparece otra pregunta fácil.
- Un glifo del equipo abre una pregunta difícil y bloquea el movimiento hasta responder. Se responde una vez; acertar suma **100 puntos**. Hay diez glifos por equipo.
- Las cuatro pistas del equipo se recogen al llegar a su celda. Otros equipos no reciben su texto ni sus ubicaciones exclusivas.
- La vista inmediata alcanza dos pasos a través de pasillos, respetando paredes. El mapa conserva las celdas ya descubiertas; los objetos y avatares ajenos se muestran solamente cuando son visibles.
- Cada equipo tiene un caso diferente y reúne la información para formular la hipótesis alternativa, calcular **z**, calcular **p** y decidir si rechaza H0.
- La entrega final se habilita después de responder los diez glifos y recoger las cuatro pistas. Cada criterio correcto suma **100 puntos** adicionales: máximo **1400 puntos** por equipo. Se acepta una diferencia de hasta `0.03` para z y `0.0005` para p; ingresar p con al menos tres decimales. La entrega final se registra una sola vez.
- La profesora ve el mapa completo, los avatares y las ubicaciones de los objetivos. Su personaje también respeta las paredes, sin consumir movimientos de los equipos. Puede desplazarse durante una pausa.

Cada sala vive en memoria. Detener el anfitrión o perder su worker termina esa partida; no hay base de datos ni recuperación persistente. Los tokens devueltos al crear o unirse identifican a ese participante mientras exista la sala. El servidor está pensado para una clase en LAN; no incluye cuentas, TLS ni servicio público de alojamiento.

## Protocolo HTTP

Todas las respuestas de la API son JSON con `Cache-Control: no-store`. Las solicitudes POST usan `Content-Type: application/json`, con un máximo de 16 KB y números finitos. Las llamadas del navegador deben venir del mismo origen que el cliente; no se habilita CORS abierto.

| Método y ruta | Cuerpo o parámetros | Resultado |
| --- | --- | --- |
| `POST /api/rooms` | `name`, `avatarName`; `seed` opcional | Crea sala y credencial de profesora |
| `POST /api/join` | `code`, `teamName`, `avatarName`, `avatarIndex` de 0 a 5 | Crea credencial de equipo antes de comenzar |
| `GET /api/state` | Query `code`, `token` | Estado filtrado para esa credencial |
| `POST /api/action` | `code`, `token`, `requestId`, `action` y campos específicos | Aplica una acción y devuelve el nuevo estado |

Crear o unirse devuelve `{ok, code, token, role, teamId, state}`. La profesora usa `role="teacher"` y `teamId=-1`; los equipos usan `role="team"` e IDs entre 0 y 5. Una consulta devuelve `{ok:true,state}` y una acción aceptada devuelve `{ok:true,result,state}`. Los errores tienen forma `{ok:false,error:{code,message}}`; el cliente debe mostrar el mensaje sin inventar un estado local exitoso.

Crear sala y unirse también admiten un `requestId` opcional generado por el cliente con aleatoriedad criptográfica: 128 bits en 32 caracteres hexadecimales o UUID. El cliente oficial lo envía y conserva al reintentar. Durante 15 minutos, repetir el mismo ID y cuerpo devuelve las mismas credenciales, sin crear otra sala o equipo; usar el ID con otro cuerpo o endpoint devuelve `request_conflict`, sin credenciales. Esta caché es independiente de las acciones y admite hasta 512 inscripciones recientes; al llenarse rechaza nuevas inscripciones con ID antes de mutar una sala. Los clientes antiguos que omitan el campo no tienen esta garantía de reintento.

| `action` | Campos adicionales | Rol |
| --- | --- | --- |
| `start`, `pause`, `resume` | Ninguno | Profesora |
| `teacher_move` | `x`, `z`, enteros de la celda adyacente | Profesora |
| `move` | `x`, `z`, enteros de la celda adyacente | Equipo |
| `easy_answer`, `hard_answer` | `questionId`, `choice` de 0 a 2 | Equipo |
| `final` | `alternative` de 0 a 2, `z`, `p`, `reject` booleano | Equipo |

Las alternativas finales tienen índices fijos: `0` significa `mu < mu0`, `1` significa `mu != mu0` y `2` significa `mu > mu0`. Usar las etiquetas `own.hypothesis.alternatives` para mostrarlas.

El cliente genera un `requestId` nuevo por intención de acción y conserva ese mismo ID cuando reintenta una solicitud. Se guardan las últimas 128 respuestas por credencial; repetir un ID todavía almacenado devuelve la misma respuesta, sin aplicar la acción otra vez. Reutilizarlo con otro contenido produce `request_conflict`. Los IDs anteriores siguen marcados y producen `stale_request`; en ese caso se consulta el estado actual. El límite por credencial es de 10 000 acciones en una sala, independiente de las consultas de estado.

## Estado que recibe el cliente

`state` contiene `revision`, `phase`, `roomCode`, `role`, `teamId`, `teacherName`, `width`, `height`, `seed`, `teacher`, `teams`, `cells`, `visible`, `objects`, `question`, `feedback` y `own`. `phase` es `lobby`, `playing`, `paused` o `finished`; el último estado aparece cuando todos los equipos entregaron.

- `revision` crece con cada cambio aceptado. Una respuesta de reintento puede contener una revisión antigua: el cliente conserva el estado con la revisión más nueva.
- `cells` contiene únicamente celdas descubiertas `{x,z,open}`. El bitmask `open` usa norte=1, este=2, sur=4 y oeste=8. `visible` contiene las celdas actualmente a la vista. La profesora recibe ambas listas completas.
- `seed` es `null` para equipos: no se entrega la semilla con la que podrían reconstruir el mapa oculto. La profesora sí recibe la semilla.
- `teams` contiene nombres, aspectos, puntuación, cantidad de glifos respondidos, cantidad de pistas y estado final. `cell` es `null` cuando otro equipo está fuera de la vista. `teacher` es `{x,z}` o `null` por el mismo criterio.
- `objects` contiene `{kind,id,owner,x,z,resolved}`, con `kind="glyph"` o `"clue"`. Los equipos reciben sus objetivos pendientes visibles. La profesora ve todas las ubicaciones, incluyendo objetivos ya resueltos marcados con `resolved=true`.
- `question` es `null` o `{id,kind,title,prompt,options}`. No contiene la respuesta correcta ni la explicación anticipada. `feedback` conserva el resultado de la última respuesta propia hasta el siguiente movimiento.
- `own` es `null` para la profesora. Para un equipo contiene movimientos, contadores, puntuación, pistas ya recogidas, título del caso, alternativas, `canSubmitFinal`, `finalSubmitted` y `assessment`. Los valores esperados de la entrega final aparecen únicamente después de entregarla.

La autorización se resuelve en Python a partir del token de sala; los IDs y roles recibidos del cliente no pueden cambiar al propietario de una acción. Un candado por sala serializa las operaciones del worker. Sus respuestas están filtradas antes de salir del servidor. El anfitrión sirve únicamente los archivos publicados del navegador y los assets de ejecución; no sirve archivos `.a`, `.o`, fuentes ni archivos de configuración de CMake.

## Verificación del servidor

Después de compilar `SessionWorker`:

```sh
python3 -m unittest discover -s game-server/tests -p 'test_*.py'
```

Para usar otro ejecutable:

```sh
HP_WORKER=/ruta/al/SessionWorker python3 -m unittest discover -s game-server/tests -p 'test_*.py'
```

Estas pruebas abren puertos temporales exclusivamente en `127.0.0.1`, lanzan workers reales y los cierran al terminar. Cubren dos equipos con credenciales independientes, niebla y pistas privadas, permisos de profesora, pausa, movimientos, preguntas, reintentos concurrentes, IDs retirados, validación de entrada, muerte del worker y una expedición completa con diez glifos, cuatro pistas y evaluación final. No requieren ventana ni GPU.

Los errores de worker se informan como HTTP 503 con `room_unavailable`. Los logs de acceso omiten los parámetros query y los cuerpos, por lo que no imprimen tokens. El anfitrión admite hasta 32 salas; al crear una nueva, elimina las salas que llevan más de ocho horas sin actividad.
