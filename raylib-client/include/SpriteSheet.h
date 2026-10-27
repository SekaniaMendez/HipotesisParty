/**
 * @file SpriteSheet.h
 * @brief Cuadrícula de sprites sobre una textura compartida.
 */
#pragma once

#include <raylib.h>

class SpriteSheet {
 public:
  /**
   * @brief Divide una textura en filas y columnas de igual tamaño.
   * Los índices empiezan en cero y avanzan por filas, de izquierda a derecha.
   * No carga ni libera la textura: su propietario debe mantenerla disponible.
   */
  SpriteSheet(Texture2D texture, int rows, int columns);

  const Texture2D& GetTexture() const;
  int GetRows() const;
  int GetColumns() const;
  int GetFrameCount() const;

  /** @brief Obtiene el rectángulo de un sprite dentro de la textura. */
  Rectangle GetFrame(int index) const;

 private:
  Texture2D texture;
  int rows;
  int columns;
};
