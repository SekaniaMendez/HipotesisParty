#pragma once

#include <map>
#include <memory>
#include <optional>

namespace hp {
enum class SceneId { Loading, Menu, Exploration, Results };

// Ciclo Enter/Exit y actualización/dibujo inspirado en Scene de
// treasure_labyrinth. La lógica de cada escena no depende de un servidor.
class Scene {
 public:
  virtual ~Scene() = default;
  virtual void Enter() {}
  virtual void Exit() {}
  virtual void Update(float deltaTime) = 0;
  virtual void Draw() = 0;
};

class SceneManager {
 public:
  ~SceneManager();
  SceneManager() = default;
  SceneManager(const SceneManager&) = delete;
  SceneManager& operator=(const SceneManager&) = delete;
  // No reemplaza escenas registradas: evita destruir un callback en ejecución.
  bool Register(SceneId id, std::unique_ptr<Scene> scene);
  // Guarda la última solicitud válida; nunca llama Enter/Exit inmediatamente.
  bool Request(SceneId id);
  void Update(float deltaTime);
  void Draw();
  SceneId Current() const { return current_.value_or(SceneId::Loading); }
  bool HasCurrent() const { return current_.has_value(); }

 private:
  void ApplyPending();
  std::map<SceneId, std::unique_ptr<Scene>> scenes_;
  std::optional<SceneId> current_;
  std::optional<SceneId> pending_;
};
}  // namespace hp
