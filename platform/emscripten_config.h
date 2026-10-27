/**
 * @file emscripten_config.h
 * @brief Configuración específica para la compilación con Emscripten.
 * @version 1.0
 * @date 2026-10-27
 * @author Daniel Mendez Sanchez
 */

#pragma once

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>

#include "game.h"

inline void emscriptenGameLoop(void* context) {
  Game* game = static_cast<Game*>(context);
  game->UpdateDrawFrame();
  game->drawFrame();
}

inline void startWebGame(Game& game) {
  // Let the browser schedule drawing with requestAnimationFrame.
  emscripten_set_main_loop_arg(emscriptenGameLoop, &game, 0, 1);
}
#endif