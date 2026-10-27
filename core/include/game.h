/**
 * @file game.h
 * @brief Declaración de la función game para el bucle principal del juego.
 * @version 1.0
 * @date 2026-10-26
 * @author Daniel Mendez Sanchez
 */

#pragma once

class Game {
 public:
  /**
   * @brief Ejecuta el bucle principal del juego.
   *
   * Esta función se encarga de actualizar la lógica del juego y dibujar los frames.
   */
  static void UpdateDrawFrame();
  static void drawFrame();
};