#include "SpriteSheet.h"

#include <limits>
#include <stdexcept>

SpriteSheet::SpriteSheet(Texture2D texture, int rows, int columns)
    : texture(texture), rows(rows), columns(columns) {
  if (texture.width <= 0 || texture.height <= 0 || rows <= 0 || columns <= 0 ||
      rows > texture.height || columns > texture.width ||
      rows > std::numeric_limits<int>::max() / columns) {
    throw std::invalid_argument(
        "La textura y la cuadricula deben tener dimensiones validas.");
  }
}

const Texture2D& SpriteSheet::GetTexture() const { return texture; }

int SpriteSheet::GetRows() const { return rows; }

int SpriteSheet::GetColumns() const { return columns; }

int SpriteSheet::GetFrameCount() const { return rows * columns; }

Rectangle SpriteSheet::GetFrame(int index) const {
  if (index < 0 || index >= GetFrameCount()) {
    throw std::out_of_range(
        "El indice del sprite esta fuera de la cuadricula.");
  }

  const float width = static_cast<float>(texture.width) / columns;
  const float height = static_cast<float>(texture.height) / rows;
  return {width * (index % columns), height * (index / columns), width, height};
}
