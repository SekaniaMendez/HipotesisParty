/**
 * @file main.cpp
 * @brief Punto de entrada principal del juego HypotesisParty.
 * @version 1.0
 * @date 2026-10-26
 * @author Daniel Mendez Sanchez
 */
#include <emscripten_config.h>
#include <game.h>
#include <raylib.h>

int main() {
  static Game game;
  InitWindow(800, 450, "HypotesisParty");
  SetTargetFPS(60);
#ifdef __EMSCRIPTEN__
  startWebGame(game);
#else
  while (!WindowShouldClose()) {
    game.UpdateDrawFrame();
    game.drawFrame();
  }
#endif
  CloseWindow();

  return 0;
}