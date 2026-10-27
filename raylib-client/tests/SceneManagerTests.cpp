#include "SceneManager.h"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

class RecordedScene : public hp::Scene {
 public:
  RecordedScene(std::vector<std::string>& log, std::string name)
      : log(log), name(std::move(name)) {}
  void Enter() override { log.push_back(name + ":enter"); }
  void Exit() override { log.push_back(name + ":exit"); }
  void Update(float deltaTime) override {
    lastDelta = deltaTime;
    log.push_back(name + ":update");
    if (onUpdate) onUpdate();
  }
  void Draw() override {
    log.push_back(name + ":draw");
    if (onDraw) onDraw();
  }
  std::function<void()> onUpdate, onDraw;
  float lastDelta = -1.0f;

 private:
  std::vector<std::string>& log;
  std::string name;
};

void TestDeferredTransitions() {
  std::vector<std::string> log;
  {
    hp::SceneManager manager;
    Check(!manager.HasCurrent(), "No debe haber escena activa antes de Request.");
    Check(!manager.Request(hp::SceneId::Results), "Debe rechazar una escena inexistente.");
    Check(!manager.Register(hp::SceneId::Menu, nullptr), "Debe rechazar una escena nula.");
    manager.Update(0.1f);
    manager.Draw();
    auto loading = std::make_unique<RecordedScene>(log, "loading");
    auto menu = std::make_unique<RecordedScene>(log, "menu");
    auto* loadingView = loading.get();
    auto* menuView = menu.get();
    Check(manager.Register(hp::SceneId::Loading, std::move(loading)) &&
              manager.Register(hp::SceneId::Menu, std::move(menu)),
          "Debe registrar las escenas.");
    Check(!manager.Register(hp::SceneId::Menu,
                             std::make_unique<RecordedScene>(log, "duplicate")),
          "No debe reemplazar una escena registrada.");
    Check(manager.Request(hp::SceneId::Loading) && log.empty(),
          "Request no debe ejecutar callbacks inmediatamente.");
    loadingView->onUpdate = [&] {
      manager.Request(hp::SceneId::Menu);
      Check(manager.Current() == hp::SceneId::Loading,
            "La transición no debe ocurrir durante Update del propietario.");
      log.push_back("loading:after-request");
    };
    manager.Update(0.25f);
    manager.Draw();
    Check(log == std::vector<std::string>{"loading:enter", "loading:update",
              "loading:after-request", "loading:exit", "menu:enter", "menu:draw"},
          "La transición debe completar Update antes de Exit/Enter.");
    Check(loadingView->lastDelta == 0.25f && manager.Current() == hp::SceneId::Menu,
          "Debe entregar deltaTime y actualizar la escena activa.");
    manager.Request(hp::SceneId::Menu);
    log.clear();
    manager.Update(-2.0f);
    Check(log == std::vector<std::string>{"menu:update"} && menuView->lastDelta == 0.0f,
          "Pedir la escena actual no debe reiniciar el ciclo; delta negativo es cero.");
    manager.Update(std::numeric_limits<float>::quiet_NaN());
    Check(menuView->lastDelta == 0.0f, "El tiempo NaN no debe llegar a las escenas.");
    menuView->onDraw = [&] { manager.Request(hp::SceneId::Loading); };
    manager.Draw();
    Check(manager.Current() == hp::SceneId::Menu,
          "Solicitudes del dibujo deben esperar al próximo Update.");
    loadingView->onUpdate = {};
    manager.Update(0.1f);
    Check(manager.Current() == hp::SceneId::Loading,
          "El próximo Update debe aplicar la solicitud del dibujo.");
  }
  Check(log.back() == "loading:exit", "La destrucción debe salir de la escena activa.");
}
}  // namespace

int main() {
  try {
    TestDeferredTransitions();
    std::cout << "Pruebas de SceneManager completadas.\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
