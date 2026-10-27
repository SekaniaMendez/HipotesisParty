// Sustitutos de Raylib para verificar cargas y liberaciones sin ventana ni
// audio.
#include <raylib.h>
#include <rlgl.h>

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

#include "ResourceManager.h"

namespace {
enum Kind { TextureKind, SoundKind, MusicKind, FontKind };
std::array<int, 4> loads{};
std::array<int, 4> unloads{};
bool windowReady = true;
bool audioReady = true;
unsigned int nextTextureId = 2;
int defaultFontUnloads = 0;
int requestedFontSize = 0;
bool requestedSpanishGlyphs = false;
std::vector<Rectangle> rectangles;
std::vector<std::string> labels;
Rectangle glyphRectangle{};
GlyphInfo glyph{};
int musicContext = 0;
int modelLoads = 0;
int modelUnloads = 0;
int animationLoads = 0;
int animationUnloads = 0;
int defaultTextureUnloads = 0;
int duplicateTextureUnloads = 0;
std::unordered_set<unsigned int> releasedTextureIds;

void Check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

Texture2D MakeTexture() {
  return {nextTextureId++, 64, 64, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
}
}  // namespace

bool FileExists(const char* path) { return std::string(path) != "missing"; }
bool IsWindowReady() { return windowReady; }
bool IsAudioDeviceReady() { return audioReady; }

Texture2D LoadTexture(const char* path) {
  ++loads[TextureKind];
  return std::string(path) == "invalid" ? Texture2D{} : MakeTexture();
}
bool IsTextureValid(Texture2D texture) {
  return texture.id != 0 && texture.width > 0 && texture.height > 0;
}
void UnloadTexture(Texture2D texture) {
  if (texture.id == 0) return;
  if (texture.id == 1) ++defaultTextureUnloads;
  if (!releasedTextureIds.insert(texture.id).second) ++duplicateTextureUnloads;
  ++unloads[TextureKind];
}

unsigned int rlGetTextureIdDefault() { return 1; }

Model LoadModel(const char* path) {
  ++modelLoads;
  Model model{};
  model.meshCount = std::string(path) == "invalid" ? 0 : 1;
  model.meshes = new Mesh[1]{};
  model.meshMaterial = new int[1]{};
  model.materialCount = 3;
  model.materials = new Material[3]{};
  for (int i = 0; i < 2; ++i) {
    model.materials[i].maps = new MaterialMap[MATERIAL_MAP_BRDF + 1]{};
  }
  // Una textura usada varias veces, otra distinta y la textura de Raylib.
  const Texture2D shared = MakeTexture();
  model.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = shared;
  model.materials[0].maps[MATERIAL_MAP_NORMAL].texture = shared;
  model.materials[1].maps[MATERIAL_MAP_ALBEDO].texture = shared;
  model.materials[1].maps[MATERIAL_MAP_EMISSION].texture = MakeTexture();
  model.materials[0].maps[MATERIAL_MAP_ROUGHNESS].texture = {
      1, 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  return model;
}

bool IsModelValid(Model model) {
  return model.meshCount > 0 && model.meshes && model.materials &&
         model.meshMaterial;
}

void UnloadModel(Model model) {
  ++modelUnloads;
  for (int i = 0; i < model.materialCount; ++i)
    delete[] model.materials[i].maps;
  delete[] model.materials;
  delete[] model.meshes;
  delete[] model.meshMaterial;
}

ModelAnimation* LoadModelAnimations(const char* path, int* count) {
  ++animationLoads;
  *count = 0;
  if (std::string(path) == "invalid") return nullptr;
  if (std::string(path) == "empty") return new ModelAnimation[0]{};
  *count = 2;
  auto* animations = new ModelAnimation[2]{};
  std::snprintf(animations[0].name, sizeof(animations[0].name), "Idle");
  std::snprintf(animations[1].name, sizeof(animations[1].name), "Walk");
  return animations;
}

void UnloadModelAnimations(ModelAnimation* animations, int) {
  ++animationUnloads;
  delete[] animations;
}

Sound LoadSound(const char* path) {
  ++loads[SoundKind];
  Sound sound{};
  if (std::string(path) != "invalid") sound.frameCount = 100;
  return sound;
}
bool IsSoundValid(Sound sound) { return sound.frameCount > 0; }
void UnloadSound(Sound sound) {
  if (sound.frameCount > 0) ++unloads[SoundKind];
}

Music LoadMusicStream(const char* path) {
  ++loads[MusicKind];
  Music track{};
  if (std::string(path) != "invalid") track.ctxData = &musicContext;
  return track;
}
bool IsMusicValid(Music track) { return track.ctxData != nullptr; }
void UnloadMusicStream(Music track) {
  if (track.ctxData != nullptr) ++unloads[MusicKind];
}

Font GetFontDefault() {
  Font font{};
  font.texture = {1, 64, 64, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  font.baseSize = 10;
  font.glyphCount = 1;
  font.recs = &glyphRectangle;
  font.glyphs = &glyph;
  return font;
}
Font LoadFontEx(const char* path, int size, const int* codepoints, int count) {
  ++loads[FontKind];
  requestedFontSize = size;
  requestedSpanishGlyphs = codepoints && count >= 224 && codepoints[0] == 32 &&
                          codepoints[209] == 241 && codepoints[223] == 255;
  if (std::string(path) == "invalid") return {};
  if (std::string(path) == "fallback") return GetFontDefault();
  Font font = GetFontDefault();
  font.baseSize = size;
  font.texture = MakeTexture();
  if (std::string(path) == "bad-atlas") font.texture.width = 0;
  return font;
}
bool IsFontValid(Font font) {
  return font.baseSize > 0 && font.glyphCount > 0 && font.recs && font.glyphs;
}
void UnloadFont(Font font) {
  if (font.texture.id == 1)
    ++defaultFontUnloads;
  else if (font.texture.id != 0)
    ++unloads[FontKind];
}

void DrawRectangleRec(Rectangle rectangle, Color) {
  rectangles.push_back(rectangle);
}
void DrawText(const char* text, int, int, int, Color) {
  labels.emplace_back(text);
}
const char* TextFormat(const char* format, ...) {
  static char text[256];
  va_list arguments;
  va_start(arguments, format);
  std::vsnprintf(text, sizeof(text), format, arguments);
  va_end(arguments);
  return text;
}

namespace {
void TestQueueAndIncrementalLoading() {
  ResourceManager resources;
  Check(resources.IsLoadingComplete() && resources.GetLoadingProgress() == 1.0f,
        "La cola vacia debe estar completa.");
  Check(resources.GetProcessedResourceCount() == 0 &&
            resources.GetLoadHistory().empty() &&
            resources.GetNextResourceName().empty() &&
            resources.GetNextResourcePath().empty(),
        "Una cola vacia no tiene historial ni siguiente recurso.");
  Check(resources.GetTexture("none") == nullptr &&
            resources.GetSound("none") == nullptr &&
            resources.GetMusic("none") == nullptr &&
            resources.GetFont("none") == nullptr,
        "Un recurso inexistente debe devolver nullptr.");
  Check(!resources.QueueTexture("", "file") &&
            !resources.QueueSound("name", "") &&
            !resources.QueueFont("font", "file", 0) &&
            !resources.QueueFont("font", "file", -1),
        "Debe rechazar entradas invalidas sin agregarlas a la cola.");

  const auto previousLoads = loads;
  const auto previousUnloads = unloads;
  Check(resources.QueueTexture("shared", "texture") &&
            resources.QueueSound("shared", "sound") &&
            resources.QueueMusic("shared", "music") &&
            resources.QueueFont("shared", "font", 24),
        "Cada tipo debe tener su propio espacio de nombres.");
  Check(!resources.QueueTexture("shared", "other") &&
            !resources.QueueSound("shared", "other") &&
            !resources.QueueMusic("shared", "other") &&
            !resources.QueueFont("shared", "other"),
        "Debe rechazar duplicados del mismo tipo.");
  Check(resources.GetTotalResourceCount() == 4 && loads == previousLoads &&
            resources.GetTexture("shared") == nullptr,
        "Encolar no debe cargar recursos.");
  Check(resources.GetNextResourceName() == "shared" &&
            resources.GetNextResourcePath() == "texture" &&
            resources.GetLoadHistory().empty(),
        "La UI debe poder mostrar el proximo archivo antes de cargarlo.");
  resources.UpdateLoading(0);
  Check(loads == previousLoads,
        "Un presupuesto cero no debe procesar recursos.");
  resources.UpdateLoading();
  Texture2D* texture = resources.GetTexture("shared");
  Check(texture && resources.GetSound("shared") == nullptr &&
            resources.GetLoadingProgress() == 0.25f &&
            resources.GetLoadedResourceCount() == 1,
        "Debe cargar un solo recurso por defecto.");
  Check(resources.GetProcessedResourceCount() == 1 &&
            resources.GetLoadHistory().size() == 1 &&
            resources.GetLoadHistory()[0].name == "shared" &&
            resources.GetLoadHistory()[0].path == "texture" &&
            resources.GetLoadHistory()[0].success &&
            resources.GetNextResourcePath() == "sound",
        "El historial debe reflejar el archivo cargado y adelantar la cola.");
  resources.UpdateLoading();
  Check(resources.GetLoadingProgress() == 0.5f,
        "Debe informar progreso parcial.");
  rectangles.clear();
  labels.clear();
  resources.DrawLoading({10, 40, 200, 20});
  Check(rectangles.size() == 2 && rectangles[0].width == 200 &&
            rectangles[1].width == 100 && rectangles[1].x == 10 &&
            labels.size() == 1 && labels[0].find("50%") != std::string::npos,
        "La barra debe reflejar el progreso y los limites indicados.");
  resources.UpdateLoading(100);
  Check(resources.IsLoadingComplete() &&
            resources.GetLoadingProgress() == 1.0f &&
            resources.GetLoadedResourceCount() == 4 && requestedFontSize == 24 &&
            requestedSpanishGlyphs && resources.GetNextResourceName().empty() &&
            resources.GetNextResourcePath().empty(),
        "Debe completar el lote y respetar el tamano de fuente.");
  const ResourceManager& view = resources;
  Check(view.GetTexture("shared") == texture && view.GetSound("shared") &&
            view.GetMusic("shared") && view.GetFont("shared"),
        "Las consultas const deben acceder a los mismos recursos.");
  const auto completedLoads = loads;
  resources.UpdateLoading();
  Check(loads == completedLoads && !resources.QueueTexture("shared", "other"),
        "No debe volver a cargar un recurso ya procesado.");

  // Forzar crecimiento del unordered_map y comprobar la estabilidad del
  // puntero.
  for (int i = 0; i < 100; ++i) {
    Check(resources.QueueTexture("extra" + std::to_string(i), "texture"),
          "Debe admitir nuevos recursos despues de completar un lote.");
  }
  resources.UpdateLoading(100);
  Check(resources.GetTexture("shared") == texture && texture->width == 64,
        "Agregar recursos no debe invalidar punteros existentes.");
  resources.UnloadAll();
  for (int kind = 0; kind < 4; ++kind) {
    Check(unloads[kind] - previousUnloads[kind] ==
              completedLoads[kind] - previousLoads[kind] +
                  (kind == TextureKind ? 100 : 0),
          "UnloadAll debe liberar cada recurso una vez.");
  }
  const auto released = unloads;
  resources.UnloadAll();
  Check(unloads == released && resources.IsLoadingComplete() &&
            resources.GetTotalResourceCount() == 0 &&
            resources.GetLoadedResourceCount() == 0 &&
            resources.GetTexture("shared") == nullptr &&
            resources.GetLoadHistory().empty() &&
            resources.GetProcessedResourceCount() == 0,
        "UnloadAll debe ser repetible y limpiar el estado.");
  Check(resources.QueueTexture("shared", "texture"),
        "Debe poder reutilizar el administrador despues de liberar.");
  resources.UpdateLoading();
}

void TestFailuresAndCleanup() {
  const auto before = unloads;
  {
    ResourceManager resources;
    resources.QueueTexture("missing", "missing");
    resources.QueueTexture("bad", "invalid");
    resources.QueueSound("bad", "invalid");
    resources.QueueMusic("bad", "invalid");
    resources.QueueFont("bad", "invalid");
    resources.QueueFont("default", "fallback");
    resources.QueueFont("atlas", "bad-atlas");
    resources.QueueTexture("good", "texture");
    resources.UpdateLoading(8);
    Check(resources.IsLoadingComplete() &&
              resources.GetLoadingProgress() == 1.0f &&
              resources.GetLoadedResourceCount() == 1 &&
              resources.GetFailedResources().size() == 7,
          "Los fallos no deben bloquear el progreso ni contarse como exitos.");
    Check(resources.GetProcessedResourceCount() == 8 &&
              resources.GetLoadHistory().size() == 8 &&
              !resources.GetLoadHistory()[0].success &&
              resources.GetLoadHistory()[0].path == "missing" &&
              resources.GetLoadHistory()[7].success &&
              resources.GetLoadHistory()[7].name == "good",
          "El historial debe registrar intentos fallidos y exitosos en orden.");
    Check(resources.GetFailedResources()[0].name == "missing" &&
              resources.GetFailedResources()[0].path == "missing" &&
              !resources.GetTexture("missing") &&
              !resources.GetTexture("bad") && !resources.GetSound("bad") &&
              !resources.GetMusic("bad") && !resources.GetFont("bad") &&
              !resources.GetFont("default") && !resources.GetFont("atlas") &&
              resources.GetTexture("good"),
          "Debe registrar los fallos y publicar solo recursos validos.");
    Check(unloads[FontKind] == before[FontKind] + 1 && defaultFontUnloads == 0,
          "Debe liberar fuentes incompletas sin liberar la fuente de Raylib.");
    rectangles.clear();
    labels.clear();
    resources.DrawLoading();
    Check(labels.size() == 2 && labels[1].find("7") != std::string::npos,
          "La UI debe informar los fallos.");
    resources.UnloadAll();
    Check(resources.GetFailedResources().empty(),
          "Debe limpiar el registro de fallos.");
    resources.QueueSound("sound", "sound");
    resources.QueueMusic("music", "music");
    resources.QueueFont("font", "font");
    resources.UpdateLoading(3);
    resources.QueueTexture("pending", "texture");
  }
  Check(unloads[TextureKind] == before[TextureKind] + 1 &&
            unloads[SoundKind] == before[SoundKind] + 1 &&
            unloads[MusicKind] == before[MusicKind] + 1 &&
            unloads[FontKind] == before[FontKind] + 2,
        "El destructor debe liberar los recursos cargados, ignorando los "
        "pendientes.");
}

void TestDevicesNotReady() {
  ResourceManager resources;
  windowReady = false;
  audioReady = false;
  const auto before = loads;
  const int previousModels = modelLoads;
  const int previousAnimations = animationLoads;
  resources.QueueTexture("texture", "file");
  resources.QueueSound("sound", "file");
  resources.QueueMusic("music", "file");
  resources.QueueFont("font", "file");
  resources.QueueModel("model", "file");
  resources.QueueModelAnimations("clips", "file");
  resources.UpdateLoading(6);
  Check(loads == before && modelLoads == previousModels &&
            resources.GetFailedResources().size() == 5 &&
            resources.IsLoadingComplete(),
        "No debe invocar cargadores sin sus dispositivos inicializados.");
  Check(animationLoads == previousAnimations + 1 &&
            resources.GetModelAnimations("clips"),
        "Los clips de animacion solo necesitan memoria de CPU.");
  windowReady = true;
  audioReady = true;
}

void TestModelsAndAnimations() {
  ResourceManager resources;
  const int previousModels = modelLoads;
  const int previousModelUnloads = modelUnloads;
  const int previousAnimationLoads = animationLoads;
  const int previousAnimationUnloads = animationUnloads;
  const int previousTextureUnloads = unloads[TextureKind];
  Check(
      !resources.GetModel("player") && !resources.GetModelAnimations("player"),
      "Las consultas 3D deben devolver nullptr si el recurso no existe.");
  Check(!resources.QueueModel("", "file") &&
            !resources.QueueModelAnimations("clips", ""),
        "Los recursos 3D deben validar nombre y ruta.");
  Check(resources.QueueModel("player", "player.glb") &&
            resources.QueueModelAnimations("player", "player.glb") &&
            resources.QueueTexture("player", "texture") &&
            !resources.QueueModel("player", "other.glb") &&
            !resources.QueueModelAnimations("player", "other.glb"),
        "Los nombres 3D deben ser unicos por tipo.");
  Check(modelLoads == previousModels &&
            animationLoads == previousAnimationLoads &&
            resources.GetTotalResourceCount() == 3,
        "Encolar recursos 3D no debe cargarlos todavia.");
  resources.UpdateLoading();
  Model* model = resources.GetModel("player");
  Check(model && !resources.GetModelAnimations("player") &&
            resources.GetLoadedResourceCount() == 1 &&
            resources.GetLoadingProgress() == 1.0f / 3.0f &&
            animationLoads == previousAnimationLoads,
        "La carga de un modelo estatico no debe exigir clips.");
  const unsigned int albedo =
      model->materials[0].maps[MATERIAL_MAP_ALBEDO].texture.id;
  const unsigned int emission =
      model->materials[1].maps[MATERIAL_MAP_EMISSION].texture.id;
  resources.UpdateLoading(2);
  const ResourceManager& view = resources;
  const auto* clips = view.GetModelAnimations("player");
  Check(resources.IsLoadingComplete() &&
            resources.GetLoadedResourceCount() == 3 &&
            view.GetModel("player") == model && clips && clips->count == 2 &&
            std::string(clips->animations[0].name) == "Idle" &&
            std::string(clips->animations[1].name) == "Walk",
        "Debe permitir consultar el modelo y sus clips por separado.");
  Check(clips == resources.GetModelAnimations("player"),
        "Las consultas const y mutables deben acceder a los mismos clips.");

  // Cambiar materiales no transfiere la propiedad de las texturas prestadas.
  // Incluso retiradas de los materiales, las texturas importadas deben
  // liberarse.
  const Texture2D external = *resources.GetTexture("player");
  model->materials[0].maps[MATERIAL_MAP_ALBEDO].texture = external;
  model->materials[0].maps[MATERIAL_MAP_NORMAL].texture = external;
  model->materials[1].maps[MATERIAL_MAP_ALBEDO].texture = external;
  model->materials[1].maps[MATERIAL_MAP_EMISSION].texture = external;
  resources.UnloadAll();
  Check(modelUnloads == previousModelUnloads + 1 &&
            animationUnloads == previousAnimationUnloads + 1 &&
            unloads[TextureKind] == previousTextureUnloads + 3 &&
            releasedTextureIds.contains(albedo) &&
            releasedTextureIds.contains(emission) &&
            defaultTextureUnloads == 0 && duplicateTextureUnloads == 0,
        "Debe liberar modelos, clips y texturas propias una sola vez.");
  Check(
      !resources.GetModel("player") && !resources.GetModelAnimations("player"),
      "UnloadAll debe retirar tambien los recursos 3D.");
  resources.UnloadAll();
  Check(modelUnloads == previousModelUnloads + 1 &&
            animationUnloads == previousAnimationUnloads + 1,
        "UnloadAll no debe liberar modelos ni clips dos veces.");
}

void TestModelFailuresAndDestructor() {
  const int previousModels = modelLoads;
  const int previousModelUnloads = modelUnloads;
  const int previousAnimations = animationLoads;
  const int previousAnimationUnloads = animationUnloads;
  const int previousTextureUnloads = unloads[TextureKind];
  {
    ResourceManager resources;
    resources.QueueModel("missing", "missing");
    resources.QueueModel("invalid", "invalid");
    resources.QueueModelAnimations("invalid", "invalid");
    resources.QueueModelAnimations("empty", "empty");
    resources.QueueModel("static", "static.obj");
    resources.UpdateLoading(5);
    Check(resources.IsLoadingComplete() &&
              resources.GetLoadingProgress() == 1.0f &&
              resources.GetLoadedResourceCount() == 1 &&
              resources.GetFailedResources().size() == 4 &&
              !resources.GetModel("missing") &&
              !resources.GetModel("invalid") &&
              !resources.GetModelAnimations("invalid") &&
              !resources.GetModelAnimations("empty") &&
              resources.GetModel("static"),
          "Los fallos 3D no deben publicar recursos ni bloquear el resto de la "
          "cola.");
    Check(modelLoads == previousModels + 2 &&
              modelUnloads == previousModelUnloads + 1 &&
              unloads[TextureKind] == previousTextureUnloads + 2 &&
              animationUnloads == previousAnimationUnloads + 1,
          "Una carga 3D fallida debe limpiar los recursos parciales.");
    resources.QueueModelAnimations("valid", "animated.glb");
    resources.UpdateLoading();
    resources.QueueModel("pending", "pending.glb");
  }
  Check(modelUnloads == previousModelUnloads + 2 &&
            animationLoads == previousAnimations + 3 &&
            animationUnloads == previousAnimationUnloads + 2 &&
            unloads[TextureKind] == previousTextureUnloads + 4 &&
            defaultTextureUnloads == 0 && duplicateTextureUnloads == 0,
        "El destructor debe liberar los recursos 3D cargados sin cargar "
        "pendientes.");
}
}  // namespace

int main() {
  static_assert(!std::is_copy_constructible_v<ResourceManager>);
  static_assert(!std::is_move_constructible_v<ResourceManager>);
  try {
    TestQueueAndIncrementalLoading();
    TestFailuresAndCleanup();
    TestDevicesNotReady();
    TestModelsAndAnimations();
    TestModelFailuresAndDestructor();
    std::cout << "Pruebas de ResourceManager completadas.\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
