/**
 * @file UIEntity.h
 * @brief Declaración de la clase UIEntity para representar la interfaz de una entidad en el juego
 * @version 1.0
 * @date 2026-10-27
 * @author Daniel Mendez Sanchez
 */
#pragma once
#include <raylib.h>

class UIEntity {
 public:
  /**
   * @brief Constructor de la clase UIEntity.
   * Inicializa una nueva interfaz de entidad con valores predeterminados.
   */
  UIEntity();

 protected:
  /**
   * @brief Actualiza el estado de la interfaz de la entidad.
   * Esta función se llama en cada frame para actualizar la lógica de la interfaz.
   */
  void Update();

  /**
   * @brief Dibuja la interfaz de la entidad en la pantalla.
   * Esta función se llama en cada frame para renderizar la interfaz.
   */
  void Draw();
};
