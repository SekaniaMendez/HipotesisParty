#include "TeamSession.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <queue>
#include <random>
#include <sstream>
#include <stdexcept>

namespace hp {
namespace {
struct QuestionSpec {
  const char* title;
  const char* prompt;
  std::array<const char*, 3> options;
  int correct;
  const char* explanation;
};

const std::array<QuestionSpec, 12> easyBank{{
    {"Hipótesis nula", "¿Qué expresión corresponde a una hipótesis nula para una media?",
     {"H0: mu = 50", "H0: mu > 50", "H0: mu != 50"}, 0,
     "La hipótesis nula fija el valor de referencia de la media poblacional."},
    {"Parámetro", "¿Qué representa mu en una prueba sobre una media?",
     {"La media de la muestra", "La media de la población", "El tamaño de la muestra"}, 1,
     "Mu es la media poblacional; x̄ representa la media muestral."},
    {"Estadístico", "Con sigma poblacional conocida y condiciones normales, ¿qué prueba usamos?",
     {"Una prueba chi-cuadrado", "Una prueba de rangos", "Una prueba z para la media"}, 2,
     "Con sigma conocida, z = (x̄ - mu0) / (sigma / sqrt(n))."},
    {"Decisión", "Si p = 0.02 y alfa = 0.05, ¿qué decisión corresponde?",
     {"Rechazar H0", "Aumentar alfa a 0.10", "Demostrar que H0 es cierta"}, 0,
     "Se rechaza H0 cuando p es menor o igual que el nivel alfa elegido previamente."},
    {"Tamaño de muestra", "Si cuadruplicamos n manteniendo sigma, ¿qué pasa con el error estándar?",
     {"Se cuadruplica", "Se reduce a la mitad", "No cambia"}, 1,
     "El error estándar sigma/sqrt(n) se divide por sqrt(4), es decir, por dos."},
    {"Significancia", "¿Qué controla alfa en una prueba de hipótesis?",
     {"La probabilidad de que H0 sea verdadera", "El porcentaje de datos normales",
      "La probabilidad de rechazar H0 cuando es verdadera"}, 2,
     "Alfa controla el error de tipo I bajo H0; no es la probabilidad de H0."},
    {"Cola derecha", "Si se investiga un aumento de la media sobre 30, ¿cuál es H1?",
     {"mu > 30", "mu < 30", "mu = 30"}, 0,
     "Un aumento corresponde a una alternativa de cola derecha: mu > 30."},
    {"Muestra", "Se miden 49 personas independientes. ¿Cuánto vale n?",
     {"7", "49", "2401"}, 1,
     "n es el número de observaciones: 49. Su raíz cuadrada es siete."},
    {"Error estándar", "Con sigma = 12 y n = 36, ¿cuál es el error estándar?",
     {"72", "0.3333", "2"}, 2,
     "sigma/sqrt(n) = 12/6 = 2."},
    {"Valor p", "¿Cuál interpretación del valor p es correcta?",
     {"Probabilidad, bajo H0, de un resultado al menos tan extremo",
      "Probabilidad de que H0 sea verdadera", "Probabilidad de haber cometido un error"}, 0,
     "El valor p se calcula suponiendo H0; no asigna una probabilidad a la hipótesis."},
    {"Dos colas", "¿Qué alternativa corresponde a detectar cualquier cambio respecto de 100?",
     {"mu > 100", "mu != 100", "mu < 100"}, 1,
     "Cualquier cambio incluye aumentos y disminuciones: la alternativa es bilateral."},
    {"Evidencia insuficiente", "Si p = 0.08 y alfa = 0.05, ¿qué conclusión es adecuada?",
     {"H0 queda demostrada", "Rechazamos H0", "No rechazamos H0"}, 2,
     "p supera alfa: no hay evidencia suficiente para rechazar H0; no se demuestra H0."},
}};

const std::array<QuestionSpec, 12> hardBank{{
    {"Estandarización", "H0: mu = 50; sigma conocida = 12; n = 36; x̄ = 53. ¿Cuánto vale z?",
     {"1.50", "0.25", "3.00"}, 0,
     "El error estándar es 12/sqrt(36)=2; z=(53-50)/2=1.50."},
    {"Valor p bilateral", "Una prueba bilateral obtiene z = -2.33. Phi(-2.33)=0.0099. ¿Cuál es p?",
     {"0.0099", "0.0198", "0.9802"}, 1,
     "En una alternativa bilateral se suman ambas colas: p=2×0.0099=0.0198."},
    {"Intervalo de confianza", "x̄ = 100, sigma = 15 y n = 36. Usando z*=1.96, ¿cuál es el IC de 95% para mu?",
     {"[94.00, 106.00]", "[97.55, 102.45]", "[95.10, 104.90]"}, 2,
     "El margen es 1.96×15/sqrt(36)=4.90; el intervalo es 100±4.90."},
    {"Diseño de muestra", "Sigma = 10. Se desea un margen máximo de 2 con 95% de confianza (z*=1.96). ¿Cuál es el n mínimo?",
     {"96", "97", "100"}, 1,
     "n ≥ (1.96×10/2)^2=96.04; se redondea hacia arriba a 97."},
    {"Nivel exigente", "H1: mu > mu0. Se obtiene z=1.80, Phi(1.80)=0.9641 y alfa=0.01. ¿Qué corresponde?",
     {"No rechazar: p=0.0359 > 0.01", "Rechazar: p=0.0359 < 0.05",
      "No rechazar: p=0.9641"}, 0,
     "La cola derecha es 1-0.9641=0.0359. La comparación es con alfa=0.01, no con 0.05."},
    {"Cola izquierda", "H1: mu < 40; sigma=4; n=16; x̄=39. Phi(-1)=0.1587. ¿Cuál pareja (z,p) es correcta?",
     {"(-4, 0.1587)", "(-1, 0.8413)", "(-1, 0.1587)"}, 2,
     "SE=4/sqrt(16)=1; z=(39-40)/1=-1. La alternativa izquierda usa Phi(-1)=0.1587."},
    {"Pruebas múltiples", "Se realizan 20 pruebas independientes, todas con H0 verdadera y alfa=0.05. ¿Probabilidad de al menos un falso rechazo?",
     {"0.3585", "0.6415", "1.0000"}, 1,
     "P(ningún falso rechazo)=0.95^20≈0.3585. El complemento es 1-0.95^20≈0.6415."},
    {"Precisión comparada", "Sigma=18. Aumentamos n de 36 a 81. ¿Cuál es la razón SE_nuevo / SE_anterior?",
     {"0.4444", "0.6667", "1.5000"}, 1,
     "Antes SE=18/6=3; después SE=18/9=2. La razón es 2/3≈0.6667."},
    {"Confianza del 99%", "x̄=20, sigma=4, n=64. Usando z*=2.576, ¿cuál es el IC del 99%?",
     {"[18.712, 21.288]", "[17.424, 22.576]", "[19.356, 20.644]"}, 0,
     "SE=4/8=0.5; margen=2.576×0.5=1.288; IC=20±1.288."},
    {"Elegir la cola", "z=1.96, Phi(1.96)=0.975 y alfa=0.03. Compare H1:mu>mu0 con H1:mu!=mu0, elegidas antes de observar datos.",
     {"Ambas rechazan H0", "Ninguna rechaza H0", "Derecha rechaza; bilateral no"}, 2,
     "Cola derecha: p=0.025<0.03. Bilateral: p=0.050>0.03. La alternativa debe definirse previamente."},
    {"Potencia", "Prueba derecha H0:mu=50, sigma=6, n=36, alfa=0.05; z_crítico=1.645. Si mu real=52, ¿potencia aproximada? Phi(0.355)=0.6387.",
     {"0.3613", "0.6387", "0.9500"}, 1,
     "SE=1; se rechaza si x̄>51.645. Bajo mu=52, z=(51.645-52)/1=-0.355; potencia=P(Z>-0.355)=0.6387."},
    {"Tamaño para potencia", "En una prueba derecha, sigma=10, cambio a detectar=5, alfa=0.05 (z=1.645) y potencia=0.80 (z=0.842). ¿n mínimo según la aproximación normal?",
     {"25", "24", "100"}, 0,
     "n≥[(1.645+0.842)×10/5]^2=24.740676; el mínimo entero es 25."},
}};

struct Case {
  const char* title;
  const char* context;
  double mu0;
  int alternative;
  int n;
  double mean;
  double sigma;
  double alpha;
};

const std::array<Case, 6> cases{{
    {"El laboratorio del café", "Una empacadora sospecha que sus bolsas contienen menos café que los 500 g declarados.",
     500, 0, 36, 492, 18, 0.05},
    {"La tutoría experimental", "Se investiga si una nueva tutoría aumenta la nota media por encima de 70 puntos.",
     70, 2, 64, 73, 12, 0.01},
    {"La calibración del taller", "Una máquina debe producir piezas de 100 mm. Se desea detectar cualquier cambio de la media, en ambas direcciones.",
     100, 1, 25, 104, 10, 0.05},
    {"La batería de larga duración", "Se investiga si un nuevo proceso aumenta la duración media por encima de 1200 horas.",
     1200, 2, 100, 1212, 60, 0.05},
    {"La ruta de las entregas", "Se investiga si una ruta reduce el tiempo medio de entrega por debajo de 30 minutos.",
     30, 0, 49, 29, 7, 0.01},
    {"El sensor del invernadero", "Un sensor debería registrar una media de 20 grados. Se busca detectar cualquier desviación de la media en ambas direcciones.",
     20, 1, 36, 20.5, 3, 0.01},
}};

std::string Number(double value) {
  std::ostringstream out;
  out.precision(10);
  out << value;
  return out.str();
}

std::string CleanName(std::string value) {
  const auto blank = [](unsigned char c) { return std::isspace(c) != 0; };
  const auto begin = std::find_if_not(value.begin(), value.end(), blank);
  const auto end = std::find_if_not(value.rbegin(), value.rend(), blank).base();
  if (begin >= end) return {};
  value = std::string(begin, end);
  if (value.size() > 64 || std::any_of(value.begin(), value.end(),
      [](unsigned char c) { return c < 32 || c == 127; })) return {};
  return value;
}

std::string FoldAscii(std::string value) {
  for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return value;
}

Question MakeQuestion(const QuestionSpec& spec, int id, bool hard) {
  return {id, hard, spec.title, spec.prompt,
          {spec.options[0], spec.options[1], spec.options[2]},
          spec.correct, spec.explanation};
}

std::array<std::string, 4> ClueTexts(const Case& problem) {
  const char* tail = problem.alternative == 0 ? "izquierda" :
                     problem.alternative == 2 ? "derecha" : "bilateral";
  return {
      std::string(problem.context) + " H0: mu = " + Number(problem.mu0) +
          ". Formulen H1 a partir de la dirección de la pregunta.",
      "La muestra contiene n = " + std::to_string(problem.n) +
          " observaciones y su media es x̄ = " + Number(problem.mean) + ".",
      "La desviación poblacional sigma = " + Number(problem.sigma) +
          " es conocida. Se fijó alfa = " + Number(problem.alpha) +
          " antes de observar los datos.",
      std::string("Usen una prueba z de cola ") + tail +
          ". Las observaciones son una muestra aleatoria independiente de una población normal. "
          "SE = sigma/sqrt(n); z = (x̄ - mu0)/SE. Comparen el valor p con alfa."};
}
}  // namespace

TeamSession::TeamSession(std::uint32_t seed, int width, int height)
    : maze_(width, height, seed) {
  if (width * height < 15) {
    throw std::invalid_argument("TeamSession requires at least fifteen maze cells");
  }
}

bool TeamSession::ValidTeam(int teamId) const {
  return teamId >= 0 && static_cast<std::size_t>(teamId) < teams_.size();
}

const Team* TeamSession::FindTeam(int teamId) const {
  return ValidTeam(teamId) ? &teams_[teamId] : nullptr;
}

int TeamSession::AddTeam(std::string name, std::string avatarName, int avatarIndex) {
  name = CleanName(std::move(name));
  avatarName = CleanName(std::move(avatarName));
  if (name.empty() || avatarName.empty() || avatarIndex < 0 || avatarIndex > 5 ||
      teams_.size() >= 6 ||
      std::any_of(teams_.begin(), teams_.end(), [&](const Team& team) {
        return FoldAscii(team.name) == FoldAscii(name);
      })) return -1;

  std::vector<bool> occupied(maze_.Width() * maze_.Height(), false);
  occupied[0] = true;
  for (const auto& glyph : glyphs_) occupied[Index(glyph.cell)] = true;
  for (const auto& clue : clues_) occupied[Index(clue.cell)] = true;
  if (std::count(occupied.begin(), occupied.end(), false) < 14) return -1;

  const int teamId = static_cast<int>(teams_.size());
  // Each team's objectives are separated by graph distance, not Euclidean
  // distance through walls. Other teams' objectives never occupy these cells.
  std::vector<Cell> locations;
  auto minimumDistance = maze_.Distances({0, 0});
  std::mt19937 random(maze_.Seed() ^ (0x9e3779b9u * (teamId + 1u)));
  for (int i = 0; i < 14; ++i) {
    int best = -1;
    int bestDistance = -1;
    const int offset = static_cast<int>(random() % occupied.size());
    for (int j = 0; j < static_cast<int>(occupied.size()); ++j) {
      const int index = (j + offset) % static_cast<int>(occupied.size());
      if (!occupied[index] && minimumDistance[index] > bestDistance) {
        best = index;
        bestDistance = minimumDistance[index];
      }
    }
    const Cell chosen{best % maze_.Width(), best / maze_.Width()};
    locations.push_back(chosen);
    occupied[best] = true;
    const auto distances = maze_.Distances(chosen);
    for (std::size_t j = 0; j < minimumDistance.size(); ++j) {
      minimumDistance[j] = std::min(minimumDistance[j], distances[j]);
    }
  }

  Team team;
  team.id = teamId;
  team.name = std::move(name);
  team.avatarName = std::move(avatarName);
  team.avatarIndex = avatarIndex;
  teams_.push_back(std::move(team));
  Progress progress;
  progress.discovered.resize(occupied.size(), false);
  progress.visible.resize(occupied.size(), false);
  progress_.push_back(std::move(progress));

  for (int i = 0; i < 10; ++i) {
    const int questionIndex = static_cast<int>(questions_.size());
    questions_.push_back(MakeQuestion(hardBank[(teamId * 2 + i) % hardBank.size()],
                                      nextQuestionId_++, true));
    glyphs_.push_back({static_cast<int>(glyphs_.size()), teamId,
                      locations[i], questionIndex, false});
  }
  const auto texts = ClueTexts(cases[teamId]);
  constexpr std::array<const char*, 4> titles{
      "La pregunta y la referencia", "Los datos observados",
      "La variabilidad y el umbral", "El método y los supuestos"};
  for (int i = 0; i < 4; ++i) {
    clues_.push_back({static_cast<int>(clues_.size()), teamId,
                      locations[10 + i], titles[i], texts[i], false});
  }
  Reveal(teamId);
  QueueEasy(teamId);
  events_.Publish(TeamJoined{teamId});
  return teamId;
}

void TeamSession::Reveal(int teamId) {
  auto& state = progress_[teamId];
  std::fill(state.visible.begin(), state.visible.end(), false);
  std::queue<std::pair<Cell, int>> queue;
  queue.push({teams_[teamId].cell, 0});
  state.visible[Index(teams_[teamId].cell)] = true;
  while (!queue.empty()) {
    const auto [current, distance] = queue.front();
    queue.pop();
    state.discovered[Index(current)] = true;
    if (distance == 2) continue;
    for (const Cell next : maze_.Neighbors(current)) {
      if (state.visible[Index(next)]) continue;
      state.visible[Index(next)] = true;
      queue.push({next, distance + 1});
    }
  }
}

bool TeamSession::IsDiscovered(int teamId, Cell cell) const {
  return ValidTeam(teamId) && maze_.IsOpen(cell) && progress_[teamId].discovered[Index(cell)];
}

bool TeamSession::IsVisible(int teamId, Cell cell) const {
  return ValidTeam(teamId) && maze_.IsOpen(cell) && progress_[teamId].visible[Index(cell)];
}

std::vector<Cell> TeamSession::KnownRoute(int teamId, Cell from, Cell to) const {
  if (!IsDiscovered(teamId, from) || !IsDiscovered(teamId, to)) return {};
  std::vector<int> parents(maze_.Width() * maze_.Height(), -1);
  std::queue<Cell> queue;
  queue.push(from);
  parents[Index(from)] = Index(from);
  while (!queue.empty()) {
    const Cell current = queue.front();
    queue.pop();
    if (current == to) break;
    for (const Cell next : maze_.Neighbors(current)) {
      if (!IsDiscovered(teamId, next) || parents[Index(next)] != -1) continue;
      parents[Index(next)] = Index(current);
      queue.push(next);
    }
  }
  if (parents[Index(to)] == -1) return {};
  std::vector<Cell> path;
  for (int at = Index(to);; at = parents[at]) {
    path.push_back({at % maze_.Width(), at / maze_.Width()});
    if (at == Index(from)) break;
  }
  std::reverse(path.begin(), path.end());
  return path;
}

void TeamSession::QueueEasy(int teamId) {
  auto& state = progress_[teamId];
  if (state.easyQuestionIndex >= 0 || teams_[teamId].finalSubmitted) return;
  state.easyQuestionIndex = static_cast<int>(questions_.size());
  const auto index = (teamId + teams_[teamId].easyAnswered) % easyBank.size();
  questions_.push_back(MakeQuestion(easyBank[index], nextQuestionId_++, false));
}

const Question* TeamSession::PendingQuestion(int teamId) const {
  if (!ValidTeam(teamId) || teams_[teamId].finalSubmitted) return nullptr;
  const auto& state = progress_[teamId];
  if (state.pendingGlyphIndex >= 0) {
    return &questions_[glyphs_[state.pendingGlyphIndex].questionIndex];
  }
  return teams_[teamId].moves == 0 && state.easyQuestionIndex >= 0
             ? &questions_[state.easyQuestionIndex] : nullptr;
}

void TeamSession::CollectAt(int teamId) {
  auto& team = teams_[teamId];
  for (auto& clue : clues_) {
    if (clue.owner == teamId && !clue.collected && clue.cell == team.cell) {
      clue.collected = true;
      team.clues.push_back(clue.id);
      events_.Publish(TeamClueCollected{teamId, clue.id});
    }
  }
  for (int i = 0; i < static_cast<int>(glyphs_.size()); ++i) {
    const auto& glyph = glyphs_[i];
    if (glyph.owner == teamId && !glyph.answered && glyph.cell == team.cell) {
      progress_[teamId].pendingGlyphIndex = i;
      break;
    }
  }
}

bool TeamSession::MoveTeam(int teamId, Cell destination) {
  if (!ValidTeam(teamId)) return false;
  auto& team = teams_[teamId];
  if (team.finalSubmitted || team.moves <= 0 ||
      progress_[teamId].pendingGlyphIndex >= 0 || !maze_.CanMove(team.cell, destination)) return false;
  team.cell = destination;
  --team.moves;
  Reveal(teamId);
  CollectAt(teamId);
  if (team.moves == 0) QueueEasy(teamId);
  events_.Publish(TeamMoved{teamId, destination});
  return true;
}

AnswerResult TeamSession::AnswerQuestion(int teamId, int expectedQuestionId, int optionIndex) {
  if (!ValidTeam(teamId) || optionIndex < 0 || optionIndex > 2) return {};
  const auto* question = PendingQuestion(teamId);
  if (!question || question->id != expectedQuestionId) return {};
  AnswerResult result;
  result.accepted = true;
  result.correct = question->correct == optionIndex;
  result.explanation = question->explanation;
  const bool hard = question->hard;
  auto& team = teams_[teamId];
  auto& state = progress_[teamId];
  if (hard) {
    glyphs_[state.pendingGlyphIndex].answered = true;
    state.pendingGlyphIndex = -1;
    ++team.hardAnswered;
    result.scoreAwarded = result.correct ? 100 : 0;
    team.score += result.scoreAwarded;
  } else {
    state.easyQuestionIndex = -1;
    ++team.easyAnswered;
    result.movesGranted = result.correct ? 5 : 2;
    team.moves = result.movesGranted;
  }
  events_.Publish(TeamQuestionAnswered{teamId, expectedQuestionId, result.correct, result.scoreAwarded});
  return result;
}

bool TeamSession::MoveTeacher(Cell destination) {
  if (!maze_.CanMove(teacher_, destination)) return false;
  teacher_ = destination;
  events_.Publish(TeacherMoved{destination});
  return true;
}

FinalChallenge TeamSession::Challenge(int teamId) const {
  if (!ValidTeam(teamId)) return {};
  return {cases[teamId].title, {"mu < mu0", "mu != mu0", "mu > mu0"}};
}

bool TeamSession::CanSubmitFinal(int teamId) const {
  return ValidTeam(teamId) && !teams_[teamId].finalSubmitted &&
         teams_[teamId].hardAnswered == 10 && teams_[teamId].clues.size() == 4;
}

FinalAssessment TeamSession::SubmitFinal(int teamId, int alternativeIndex,
                                        double z, double p, bool reject) {
  if (!CanSubmitFinal(teamId) || alternativeIndex < 0 || alternativeIndex > 2 ||
      !std::isfinite(z) || !std::isfinite(p) || p < 0 || p > 1) return {};
  const auto& problem = cases[teamId];
  const double expectedZ = (problem.mean - problem.mu0) / (problem.sigma / std::sqrt(problem.n));
  const double sqrt2 = std::sqrt(2.0);
  const double expectedP = problem.alternative == 0 ? 0.5 * std::erfc(-expectedZ / sqrt2) :
                           problem.alternative == 2 ? 0.5 * std::erfc(expectedZ / sqrt2) :
                                                      std::erfc(std::abs(expectedZ) / sqrt2);
  FinalAssessment result;
  result.accepted = true;
  result.expectedAlternative = problem.alternative;
  result.expectedZ = expectedZ;
  result.expectedP = expectedP;
  result.expectedReject = expectedP <= problem.alpha;
  result.criteria = {alternativeIndex == problem.alternative,
                     std::abs(z - expectedZ) <= 0.03 + 1e-12,
                     std::abs(p - expectedP) <= 0.0005 + 1e-12,
                     reject == result.expectedReject};
  result.scoreAwarded = 100 * static_cast<int>(std::count(result.criteria.begin(), result.criteria.end(), true));
  teams_[teamId].score += result.scoreAwarded;
  teams_[teamId].finalSubmitted = true;
  progress_[teamId].final = result;
  events_.Publish(TeamFinalSubmitted{teamId, result.scoreAwarded});
  return result;
}

const FinalAssessment* TeamSession::FinalResult(int teamId) const {
  return ValidTeam(teamId) && progress_[teamId].final ? &*progress_[teamId].final : nullptr;
}

}  // namespace hp
