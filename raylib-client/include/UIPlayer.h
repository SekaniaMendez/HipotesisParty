/**
 * @file UIPlayer.h
 * @brief Declaración de la clase UIPlayer para representar la interfaz del jugador en el juego
 * @version 1.0
 * @date 2026-10-27
 * @author Daniel Mendez Sanchez
 */

#pragma once
#include <raylib.h>

#include "UIEntity.h"

class UIPlayer : public UIEntity {
 public:
  /**
   * @brief Constructor de la clase UIPlayer.
   * Inicializa una nueva interfaz de jugador con valores predeterminados.
   */
  UIPlayer();

 protected:
  /**
   * @brief Actualiza el estado de la interfaz del jugador.
   * Esta función se llama en cada frame para actualizar la lógica de la interfaz.
   */
  void Update();

  /**
   * @brief Dibuja la interfaz del jugador en la pantalla.
   * Esta función se llama en cada frame para renderizar la interfaz.
   */
  void Draw();
};