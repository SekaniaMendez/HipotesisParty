/**
 * @file player.h
 * @brief Declaración de la clase Player para representar al jugador en el juego.
 * @version 1.0
 * @date 2026-10-27
 * @author Daniel Mendez Sanchez
 */

#pragma once
#include <string>

class Player {
 public:
  /**
   * @brief Constructor de la clase Player.
   * Inicializa un nuevo jugador con valores predeterminados.
   */
  Player();

  /**
   * @brief Actualiza el estado del jugador.
   * Esta función se llama en cada frame para actualizar la lógica del jugador.
   */
  void Update();

  /**
   * @brief Dibuja al jugador en la pantalla.
   * Esta función se llama en cada frame para renderizar al jugador.
   */
  void Draw();

 private:
  std::string name; /**< Nombre del jugador */
  float velocity;   /**< Velocidad del jugador */
};