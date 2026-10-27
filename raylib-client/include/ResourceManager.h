/**
 * @file ResourceManager.h
 * @brief Recursos compartidos y carga incremental para la interfaz.
 */
#pragma once

#include <raylib.h>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class ResourceManager {
 public:
  struct FailedResource {
    std::string name;
    std::string path;
  };

  struct LoadResult {
    std::string name;
    std::string path;
    bool success = false;
  };

  struct ModelAnimationSet {
    ModelAnimation* animations = nullptr;
    int count = 0;
  };

  ResourceManager() = default;
  /** @brief Libera los recursos. Debe ejecutarse antes de cerrar ventana/audio.
   */
  ~ResourceManager();

  ResourceManager(const ResourceManager&) = delete;
  ResourceManager& operator=(const ResourceManager&) = delete;
  ResourceManager(ResourceManager&&) = delete;
  ResourceManager& operator=(ResourceManager&&) = delete;

  /**
   * @brief Agrega un recurso a la cola sin cargarlo todavía.
   * Devuelve false para nombres/rutas vacíos o nombres duplicados del mismo
   * tipo. Un mismo nombre puede identificar recursos de tipos diferentes.
   */
  bool QueueTexture(const std::string& name, const std::string& path);
  bool QueueSound(const std::string& name, const std::string& path);
  bool QueueMusic(const std::string& name, const std::string& path);
  /** @brief Encola una fuente TTF/OTF con tamaño positivo en píxeles. */
  bool QueueFont(const std::string& name, const std::string& path,
                 int fontSize = 32);
  /** @brief Encola un modelo 3D con sus materiales y texturas. Requiere
   * ventana. */
  bool QueueModel(const std::string& name, const std::string& path);
  /**
   * @brief Encola los clips de un archivo animado, por separado del modelo.
   * Cada llamada cuenta como un recurso en la barra de carga.
   * Un archivo sin clips se registra como un fallo de carga de animaciones.
   */
  bool QueueModelAnimations(const std::string& name, const std::string& path);

  /**
   * @brief Procesa hasta maxResources archivos; por defecto uno por cuadro.
   * Llamar desde el hilo principal, con ventana/audio inicializados según el
   * tipo. Cada archivo se carga de forma síncrona; dibujar entre llamadas
   * mantiene visible el progreso, aunque un archivo grande puede demorar un
   * cuadro.
   */
  void UpdateLoading(std::size_t maxResources = 1);
  bool IsLoadingComplete() const;
  /** @brief Progreso de intentos completados (incluidos fallos); cola vacía
   * = 1. */
  float GetLoadingProgress() const;
  std::size_t GetLoadedResourceCount() const;
  /** @brief Intentos terminados, incluidos fallos; coincide con el historial. */
  std::size_t GetProcessedResourceCount() const;
  std::size_t GetTotalResourceCount() const;
  const std::vector<FailedResource>& GetFailedResources() const;
  /** @brief Próximo archivo a procesar, o cadena vacía si terminó la cola. */
  std::string GetNextResourceName() const;
  std::string GetNextResourcePath() const;
  const std::vector<LoadResult>& GetLoadHistory() const;

  /** @brief Dibuja progreso y fallos entre BeginDrawing() y EndDrawing(). */
  void DrawLoading(Rectangle bounds = {200, 300, 400, 30}) const;

  /**
   * @brief Devuelve nullptr si no existe o todavía no se cargó.
   * Punteros prestados: no liberar sus recursos ni modificar sus
   * identificadores. Son válidos hasta UnloadAll() o la destrucción del
   * administrador.
   */
  Texture2D* GetTexture(const std::string& name);
  const Texture2D* GetTexture(const std::string& name) const;
  Sound* GetSound(const std::string& name);
  const Sound* GetSound(const std::string& name) const;
  /** @brief La reproducción requiere llamar UpdateMusicStream() cada cuadro. */
  Music* GetMusic(const std::string& name);
  const Music* GetMusic(const std::string& name) const;
  Font* GetFont(const std::string& name);
  const Font* GetFont(const std::string& name) const;
  /**
   * @brief Obtiene el modelo compartido. Copiar Model no duplica sus datos.
   * Para animar varias entidades hay que gestionar sus poses independientes.
   */
  Model* GetModel(const std::string& name);
  const Model* GetModel(const std::string& name) const;
  /** @brief Clips prestados; validar su compatibilidad con
   * IsModelAnimationValid(). */
  ModelAnimationSet* GetModelAnimations(const std::string& name);
  const ModelAnimationSet* GetModelAnimations(const std::string& name) const;

  /** @brief Libera todos los recursos y reinicia la cola, el progreso y los
   * fallos. */
  void UnloadAll();

 private:
  enum class ResourceType {
    Texture,
    Sound,
    Music,
    Font,
    Model,
    ModelAnimations
  };

  struct ModelResource {
    Model model{};
    // Texturas importadas por LoadModel, sin incluir la predeterminada de
    // Raylib.
    std::vector<Texture2D> textures;
  };

  struct PendingResource {
    ResourceType type;
    std::string name;
    std::string path;
    int fontSize;
  };

  std::unordered_map<std::string, Texture2D> textures;
  std::unordered_map<std::string, Sound> sounds;
  std::unordered_map<std::string, Music> music;
  std::unordered_map<std::string, Font> fonts;
  std::unordered_map<std::string, ModelResource> models;
  std::unordered_map<std::string, ModelAnimationSet> modelAnimations;
  std::vector<PendingResource> pendingResources;
  std::vector<FailedResource> failedResources;
  std::vector<LoadResult> loadHistory;
  std::size_t processedResources = 0;
  std::size_t loadedResources = 0;

  bool QueueResource(ResourceType type, const std::string& name,
                     const std::string& path, int fontSize = 32);
  bool LoadResource(const PendingResource& resource);
  static void TrackModelTextures(ModelResource& resource);
  static void UnloadModelResource(ModelResource& resource);
};
