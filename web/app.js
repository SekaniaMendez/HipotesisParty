/* Plain browser UI bridges FastAPI snapshots into the raylib WebAssembly renderer. */
"use strict";
const $ = id => document.getElementById(id);
let session = null, socket = null, state = null, engine = null, enginePromise = null;
let retry = 0, retryTimer, toastTimer, heldTimer, activeDirection;
let lastQuestion = null, lastFeedback = null, lastEvent = -1, lastTeacherCell = null;
let audio = null, sound = false;
const roleLabels = {student: "INVESTIGADOR", teacher: "LA PROFE", host: "DIRECTOR DE PARTIDA", projector: "CÁMARA DE LA PROFE"};
const titles = {student: "La evidencia te espera.", teacher: "El pasillo es tuyo.", host: "Una sala. Muchas sospechas.", projector: "La profe viene en camino."};
const sessionKey = (code, role) => `hp:${code}:${role}`;

function toast(message) {
  $("toast").textContent = message;
  $("toast").hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => $("toast").hidden = true, 4500);
}
async function api(path, body) {
  const response = await fetch(path, {method: "POST", headers: {"Content-Type": "application/json"}, body: JSON.stringify(body)});
  const result = await response.json();
  if (!response.ok) throw new Error(typeof result.detail === "string" ? result.detail : "Revisá los datos e intentá otra vez.");
  return result;
}
function send(data) {
  if (socket?.readyState === WebSocket.OPEN) socket.send(JSON.stringify(data));
}
function linkFor(role, token) {
  const url = new URL(location.origin);
  url.hash = new URLSearchParams({room: session.code, role, token}).toString();
  return url.href;
}
function setHostLinks() {
  if (!session.teacher_token) return;
  $("teacher-link").href = linkFor("teacher", session.teacher_token);
  $("projector-link").href = linkFor("projector", session.projector_token);
}
async function initRenderer() {
  if (enginePromise) return enginePromise;
  enginePromise = (async () => {
    if (typeof createHypotesisGame !== "function") throw new Error("Falta compilar raylib para web. Ejecutá bash scripts/setup-web.sh y recargá.");
    const module = await createHypotesisGame({
      canvas: $("canvas"),
      locateFile: path => `/static/build/${path}`,
      print: () => {},
      printErr: message => console.warn(message),
      onAbort: reason => toast(`No se pudo iniciar el mapa: ${reason}`),
    });
    engine = module;
    $("renderer-status").hidden = true;
    const resize = () => {
      const rect = $("map-frame").getBoundingClientRect();
      // CSS-sized backing buffer bounds memory use on high-DPI mobile screens.
      module._hp_resize(Math.max(1, Math.floor(rect.width)), Math.max(1, Math.floor(rect.height)));
    };
    new ResizeObserver(resize).observe($("map-frame"));
    resize();
    if (state) paintMap(state);
    return module;
  })().catch(error => {
    $("renderer-status").textContent = error.message;
    console.error(error);
  });
  return enginePromise;
}
function paintMap(s) {
  if (!engine) return;
  engine._hp_map(s.width, s.height);
  s.paths.forEach((mask, i) => engine._hp_tile(i, mask));
  engine._hp_players_clear();
  for (const p of s.players) {
    engine._hp_player(parseInt(p.id.slice(0, 7), 16), p.cell, p.role === "teacher" ? 1 : 0,
      p.id === s.self_id ? 1 : 0, p.protected ? 1 : 0, p.busy ? 1 : 0);
  }
  engine._hp_focus(s.focus, s.role === "host" ? 1 : 0);
  engine._hp_stations_clear();
  s.stations.forEach(station => engine._hp_station(station.cell, station.solved ? 1 : 0));
}
function connect() {
  clearTimeout(retryTimer);
  const scheme = location.protocol === "https:" ? "wss:" : "ws:";
  socket = new WebSocket(`${scheme}//${location.host}/ws/${session.code}?token=${encodeURIComponent(session.token)}`);
  $("connection-text").textContent = retry ? "Reconectando…" : "Conectando…";
  socket.onopen = () => {
    retry = 0;
    $("connection").classList.remove("offline");
    $("connection-text").textContent = "En línea";
  };
  socket.onmessage = event => {
    const message = JSON.parse(event.data);
    if (message.type === "error") return toast(message.message);
    state = message;
    updateUI(message);
    paintMap(message);
  };
  socket.onclose = event => {
    stopMove();
    $("connection").classList.add("offline");
    $("connection-text").textContent = "Sin conexión";
    if ([1008, 4001].includes(event.code)) {
      toast(event.code === 4001 ? "Esta sesión se abrió en otra pestaña. Cerrá esta vista." : "La sala ya no está disponible. Volvé al inicio para entrar de nuevo.");
      return;
    }
    retryTimer = setTimeout(connect, Math.min(500 * 2 ** retry++, 4000));
  };
}
function enter(data) {
  session = data;
  localStorage.setItem(sessionKey(data.code, data.role), JSON.stringify(data));
  history.replaceState(null, "", `/#${new URLSearchParams({room: data.code, role: data.role, token: data.token})}`);
  $("landing").hidden = true;
  $("game").hidden = false;
  window.scrollTo(0, 0);
  document.body.className = data.role;
  $("game-code").textContent = $("big-code").textContent = data.code;
  $("role-label").textContent = roleLabels[data.role];
  $("view-title").textContent = titles[data.role];
  $("host-tools").hidden = data.role !== "host";
  $("student-stats").hidden = data.role !== "student";
  $("teacher-info").hidden = data.role !== "teacher";
  $("controls").hidden = !["student", "teacher"].includes(data.role);
  $("interact").hidden = data.role !== "student";
  $("map-caption").textContent = data.role === "host" ? "MAPA DEL DIRECTOR" : data.role === "projector" ? "SIGUIENDO A LA PROFE" : "EXPLORACIÓN EN VIVO";
  if (data.role === "teacher") $("control-hint").textContent = "WASD, flechas o botones para elegir tu ruta";
  setHostLinks();
  connect();
  initRenderer();
}
function makeRanking(s) {
  $("ranking").replaceChildren();
  let previous = null, position = 0;
  s.ranking.forEach((p, i) => {
    const key = `${p.score}:${p.clues}`;
    if (key !== previous) position = i + 1;
    previous = key;
    const li = document.createElement("li");
    if (!p.connected) li.classList.add("offline");
    if (p.id === s.self_id) li.classList.add("self");
    const place = document.createElement("span"); place.className = "place"; place.textContent = String(position).padStart(2, "0");
    const name = document.createElement("span"); name.className = "name"; name.textContent = `${p.name}${p.id === s.self_id ? " (vos)" : ""}`;
    const points = document.createElement("span"); points.className = "points"; points.textContent = `${p.score} pts`;
    li.append(place, name, points);
    $("ranking").append(li);
  });
  $("empty-ranking").hidden = s.ranking.length > 0;
}
function updateUI(s) {
  $("timer").textContent = `${String(Math.floor(s.remaining / 60)).padStart(2, "0")}:${String(s.remaining % 60).padStart(2, "0")}`;
  $("timer").parentElement.classList.toggle("urgent", s.remaining < 60);
  $("score").textContent = s.score;
  $("clues").firstChild.textContent = s.clues;
  $("player-count").textContent = `${s.student_count} / 22`;
  const isLobby = s.status === "lobby", paused = s.status === "paused", finished = s.status === "finished";
  $("phase-overlay").hidden = !isLobby && !paused;
  $("phase-label").textContent = isLobby ? "SALA DE ESPERA" : "PARTIDA EN PAUSA";
  $("phase-title").textContent = isLobby ? "La profe está por llegar." : "Un respiro en el pasillo.";
  $("phase-copy").textContent = isLobby ? `${s.student_count} investigadores en la sala. El director dará la salida.` : "El reloj y las preguntas esperan hasta que el director continúe.";
  $("start").hidden = !isLobby;
  $("start").disabled = !s.student_count;
  $("pause").hidden = isLobby || finished;
  $("pause").textContent = paused ? "Continuar partida" : "Pausar";
  $("finish").hidden = isLobby || finished;
  $("protection").hidden = s.role !== "student" || !s.protected_seconds || s.status !== "running";
  $("protection-seconds").textContent = s.protected_seconds;
  $("teacher-note").textContent = !s.teacher_connected ? "La profe todavía no se conectó." : s.teacher_busy ? "Desafío en curso. Al terminar podrás continuar." : "Buscando investigadores…";
  const self = s.players.find(p => p.id === s.self_id);
  const station = self && s.stations.find(st => st.cell === self.cell);
  $("interact").disabled = !station || station.solved || s.status !== "running" || Boolean(s.question);
  $("interact").textContent = station?.solved ? "✓ Pista recuperada" : "◇ Investigar";
  if (lastEvent !== s.event_serial) {
    $("event").textContent = s.event;
    if (lastEvent >= 0 && s.event.includes("encontró")) beep(160, .35);
    lastEvent = s.event_serial;
  }
  const teacher = s.players.find(p => p.role === "teacher");
  if (teacher && lastTeacherCell !== null && teacher.cell !== lastTeacherCell) beep(90, .055);
  if (teacher) lastTeacherCell = teacher.cell;
  // Ranking changes less often than position snapshots.
  const rankKey = JSON.stringify(s.ranking);
  if (updateUI.rankKey !== rankKey) { makeRanking(s); updateUI.rankKey = rankKey; }
  updateQuestion(s);
  if (finished && $("podium").hidden) {
    $("podium").hidden = false;
    $("podium-list").replaceChildren();
    let previous = null, place = 0;
    s.ranking.forEach((p, i) => {
      const key = `${p.score}:${p.clues}`;
      if (key !== previous) place = i + 1;
      previous = key;
      if (place > 3) return;
      const column = document.createElement("div");
      const n = document.createElement("strong"); n.textContent = `${place}º`;
      const name = document.createElement("span"); name.textContent = p.name;
      const score = document.createElement("small"); score.textContent = `${p.score} puntos · ${p.clues} pistas`;
      column.append(n, name, score); $("podium-list").append(column);
    });
  }
}
function updateQuestion(s) {
  const dialog = $("question-dialog");
  if (s.question) {
    const q = s.question;
    $("question-time").textContent = `${q.seconds}s${s.status === "paused" ? " · pausa" : ""}`;
    if (lastQuestion !== q.id) {
      lastQuestion = q.id; lastFeedback = null;
      if ($("feedback-dialog").open) $("feedback-dialog").close();
      $("question-kind").textContent = q.kind === "hard" ? "¡TE ENCONTRÓ LA PROFE!" : "ESTACIÓN DE PISTAS";
      $("question-prompt").textContent = q.prompt;
      $("answer-choices").replaceChildren();
      q.choices.forEach((text, index) => {
        const button = document.createElement("button"); button.type = "button";
        const letter = document.createElement("span"); letter.textContent = String.fromCharCode(65 + index);
        button.append(letter, document.createTextNode(text));
        button.onclick = () => {
          if (state.status !== "running") return toast("La partida está en pausa.");
          send({type: "answer", question_id: q.id, choice: index});
        };
        $("answer-choices").append(button);
      });
    }
    if (!dialog.open) dialog.showModal();
  } else {
    if (dialog.open) dialog.close();
    lastQuestion = null;
    if (s.feedback && JSON.stringify(s.feedback) !== lastFeedback && s.status !== "finished") {
      lastFeedback = JSON.stringify(s.feedback);
      $("feedback-label").textContent = s.feedback.timeout ? "SE ACABÓ EL TIEMPO" : "RESULTADO DEL DESAFÍO";
      $("feedback-title").textContent = s.feedback.correct ? `¡Bien pensado! +${s.feedback.points} puntos` : "Una pista para la próxima.";
      $("feedback-copy").textContent = s.feedback.explanation;
      if (!$("feedback-dialog").open) $("feedback-dialog").showModal();
    }
  }
  if (s.status === "finished") {
    if (dialog.open) dialog.close();
    if ($("feedback-dialog").open) $("feedback-dialog").close();
  }
}
function startMove(direction) {
  stopMove();
  if (!state || !["student", "teacher"].includes(state.role)) return;
  activeDirection = direction;
  const move = () => {
    if (state.status === "running" && !state.question && !$("feedback-dialog").open)
      send({type: "move", direction});
  };
  move();
  heldTimer = setInterval(move, state.role === "teacher" ? 250 : 190);
}
function stopMove() { clearInterval(heldTimer); heldTimer = null; activeDirection = null; }
const keys = {ArrowUp: "up", w: "up", W: "up", ArrowRight: "right", d: "right", D: "right", ArrowDown: "down", s: "down", S: "down", ArrowLeft: "left", a: "left", A: "left"};
document.addEventListener("keydown", event => {
  if (!session || ["INPUT", "SELECT", "TEXTAREA"].includes(event.target.tagName)) return;
  if (keys[event.key]) {
    event.preventDefault();
    if (!event.repeat) startMove(keys[event.key]);
  }
  if (event.key.toLowerCase() === "e" && !event.repeat && state?.role === "student") send({type: "interact"});
});
document.addEventListener("keyup", event => { if (keys[event.key] === activeDirection) stopMove(); });
window.addEventListener("blur", stopMove);
document.addEventListener("visibilitychange", () => { if (document.hidden) stopMove(); });
document.querySelectorAll("[data-direction]").forEach(button => {
  button.addEventListener("pointerdown", event => { event.preventDefault(); button.setPointerCapture(event.pointerId); startMove(button.dataset.direction); });
  for (const name of ["pointerup", "pointercancel", "lostpointercapture"]) button.addEventListener(name, stopMove);
  button.addEventListener("contextmenu", event => event.preventDefault());
});
$("interact").onclick = () => send({type: "interact"});
$("start").onclick = () => send({type: "start"});
$("pause").onclick = () => send({type: state.status === "paused" ? "resume" : "pause"});
$("finish").onclick = () => $("finish-dialog").showModal();
$("finish-confirm").onclick = () => { $("finish-dialog").close(); send({type: "finish"}); };
$("finish-cancel").onclick = () => $("finish-dialog").close();
$("question-dialog").addEventListener("cancel", event => event.preventDefault());
function closeFeedback() { $("feedback-dialog").close(); send({type: "dismiss_feedback"}); }
$("feedback-close").onclick = closeFeedback;
$("feedback-dialog").addEventListener("cancel", event => { event.preventDefault(); closeFeedback(); });
$("join-form").onsubmit = async event => {
  event.preventDefault(); $("join-button").disabled = true;
  const code = $("room-code").value.trim().toUpperCase();
  try {
    // A saved capability restores this participant instead of consuming another slot.
    const saved = localStorage.getItem(sessionKey(code, "student"));
    const data = saved ? JSON.parse(saved) : await api(`/api/rooms/${encodeURIComponent(code)}/join`, {name: $("nickname").value.trim()});
    enter(data);
  } catch (error) { toast(error.message); }
  finally { $("join-button").disabled = false; }
};
$("create-form").onsubmit = async event => {
  event.preventDefault();
  const button = event.target.querySelector("button"); button.disabled = true;
  try { enter(await api("/api/rooms", {minutes: Number($("minutes").value)})); }
  catch (error) { toast(error.message); }
  finally { button.disabled = false; }
};
$("copy-invite").onclick = async () => {
  const invitation = `${location.origin}/?sala=${session.code}`;
  try { await navigator.clipboard.writeText(invitation); toast("Invitación copiada."); }
  catch { toast(`Invitación: ${invitation}`); }
};
$("fullscreen-button").onclick = async () => {
  try {
    if (document.fullscreenElement) await document.exitFullscreen();
    else if (document.documentElement.requestFullscreen) await document.documentElement.requestFullscreen();
    else toast("Usá las opciones de pantalla de tu navegador.");
  } catch { toast("El navegador no permitió pantalla completa."); }
};
$("sound-button").onclick = () => {
  const Audio = window.AudioContext || window.webkitAudioContext;
  if (!Audio) return toast("Este navegador no tiene audio disponible.");
  if (!audio) audio = new Audio();
  audio.resume(); sound = !sound;
  $("sound-button").style.color = sound ? "var(--gold)" : "";
  $("sound-button").setAttribute("aria-label", sound ? "Desactivar sonido" : "Activar sonido");
  beep(300, .1);
};
function beep(frequency, duration) {
  if (!sound || !audio || document.hidden) return;
  const oscillator = audio.createOscillator(), gain = audio.createGain();
  oscillator.type = "sine"; oscillator.frequency.value = frequency;
  gain.gain.setValueAtTime(.06, audio.currentTime); gain.gain.exponentialRampToValueAtTime(.001, audio.currentTime + duration);
  oscillator.connect(gain); gain.connect(audio.destination);
  oscillator.start(); oscillator.stop(audio.currentTime + duration);
}
const hash = new URLSearchParams(location.hash.slice(1));
const code = hash.get("room"), role = hash.get("role"), token = hash.get("token");
if (code && token && roleLabels[role]) {
  let saved = null;
  try { saved = JSON.parse(localStorage.getItem(sessionKey(code, role))); } catch {}
  enter({...saved, code, role, token});
} else {
  const invite = new URLSearchParams(location.search).get("sala");
  if (invite) $("room-code").value = invite.toUpperCase().slice(0, 5);
}
