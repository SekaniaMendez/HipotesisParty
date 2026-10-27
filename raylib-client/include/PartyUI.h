#pragma once

#include <raylib.h>

#include <string>
#include <string_view>

// Adaptación de Button/TextDisplay de treasure_labyrinth: fuentes prestadas,
// etiquetas centradas y estados hover/selección separados de la lógica de juego.
namespace hp::ui {
inline constexpr Color Navy{13, 24, 39, 255};
inline constexpr Color Surface{24, 40, 57, 245};
inline constexpr Color Cream{241, 235, 219, 255};
inline constexpr Color Teal{72, 212, 192, 255};
inline constexpr Color Gold{224, 183, 106, 255};
inline constexpr Color Muted{149, 168, 180, 255};
inline constexpr Color Line{55, 78, 94, 255};
inline constexpr Color Error{245, 127, 121, 255};

// No transfieren propiedad. ResetFont limpia ambas fuentes antes de liberarlas.
void SetFont(Font font);
void SetHeadingFont(Font font);
void ResetFont();
Vector2 Measure(std::string_view text, float size);
void Text(std::string_view text, float x, float y, float size,
          Color color = Cream);
// Fuente de títulos, con fallback a la fuente de texto si todavía no se cargó.
void Title(std::string_view text, float x, float y, float size,
           Color color = Cream);
void Panel(Rectangle bounds, Color color = Surface);
// Activación al soltar dentro del botón después de pulsar en él. El propietario
// administra el foco (por ejemplo con Tab) y pasa focused para Enter/Espacio.
// Llamar una vez por cuadro desde Draw. No cambia escenas durante el dibujo:
// el resultado se entrega a SceneManager::Request para el siguiente Update.
bool Button(Rectangle bounds, std::string_view label, bool accent = false,
            bool enabled = true, bool focused = false);
void ProgressBar(Rectangle bounds, float progress, Color color = Teal);
void ResetInput();

struct Label {
  std::string text;
  Vector2 position{};
  float size = 20.0f;
  Color color = Cream;
  void Draw() const { Text(text, position.x, position.y, size, color); }
};

// Mapeo con letterboxing; no altera la cámara de Raylib. Usar tanto en dibujo
// como en hit testing pasando los rectángulos convertidos a los widgets.
class Canvas {
 public:
  Canvas(float designWidth = 1280.0f, float designHeight = 800.0f);
  Rectangle ToScreen(Rectangle bounds) const;
  Vector2 ToScreen(Vector2 position) const;
  float Scale(float value) const { return value * scale_; }
  float GetScale() const { return scale_; }

 private:
  float scale_ = 1.0f;
  Vector2 offset_{};
};
}  // namespace hp::ui
