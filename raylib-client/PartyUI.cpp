#include "PartyUI.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace hp::ui {
namespace {
Font borrowedFont{};
Font borrowedHeadingFont{};
std::size_t capturedButton = 0;
Font ActiveFont() {
  return borrowedFont.texture.id != 0 ? borrowedFont : GetFontDefault();
}
float Spacing(float size) { return std::max(0.5f, size / 28.0f); }
std::size_t ButtonId(Rectangle bounds, std::string_view label) {
  std::size_t hash = std::hash<std::string_view>{}(label);
  for (float value : {bounds.x, bounds.y, bounds.width, bounds.height}) {
    hash ^= std::hash<float>{}(value) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
  }
  return hash == 0 ? 1 : hash;
}
}  // namespace

void SetFont(Font font) { borrowedFont = font; }
void SetHeadingFont(Font font) { borrowedHeadingFont = font; }
void ResetFont() { borrowedFont = {}; borrowedHeadingFont = {}; }
void ResetInput() { capturedButton = 0; }

Vector2 Measure(std::string_view text, float size) {
  if (size <= 0.0f || text.empty()) return {};
  return MeasureTextEx(ActiveFont(), std::string(text).c_str(), size,
                       Spacing(size));
}

void Text(std::string_view text, float x, float y, float size, Color color) {
  if (size <= 0.0f || text.empty()) return;
  DrawTextEx(ActiveFont(), std::string(text).c_str(), {x, y}, size,
             Spacing(size), color);
}

void Title(std::string_view text, float x, float y, float size, Color color) {
  if (size <= 0.0f || text.empty()) return;
  const Font font = borrowedHeadingFont.texture.id != 0
                        ? borrowedHeadingFont : ActiveFont();
  DrawTextEx(font, std::string(text).c_str(), {x, y}, size,
             std::max(0.25f, size / 150.0f), color);
}

void Panel(Rectangle bounds, Color color) {
  if (bounds.width <= 0.0f || bounds.height <= 0.0f) return;
  DrawRectangleRounded({bounds.x, bounds.y + 5, bounds.width, bounds.height},
                       0.09f, 8, Color{0, 0, 0, 45});
  DrawRectangleRounded(bounds, 0.09f, 8, color);
  DrawRectangleRoundedLinesEx(bounds, 0.09f, 8, 1.0f, Line);
}

bool Button(Rectangle bounds, std::string_view label, bool accent, bool enabled,
            bool focused) {
  if (bounds.width <= 0.0f || bounds.height <= 0.0f) return false;
  const auto id = ButtonId(bounds, label);
  const bool hovered = enabled && CheckCollisionPointRec(GetMousePosition(), bounds);
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    if (hovered) capturedButton = id;
    else if (capturedButton == id) capturedButton = 0;
  }
  const bool held = hovered && capturedButton == id &&
                    IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  bool activated = false;
  if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && capturedButton == id) {
    activated = hovered;
    capturedButton = 0;
  }
  if (enabled && focused && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)))
    activated = true;

  Color fill = accent ? Teal : Surface;
  if (!enabled) fill = Color{36, 48, 61, 255};
  else if (held) fill = accent ? Color{48, 167, 153, 255} : Color{42, 72, 86, 255};
  else if (hovered || focused)
    fill = accent ? Color{106, 233, 214, 255} : Color{39, 64, 81, 255};
  DrawRectangleRounded(bounds, 0.22f, 10, fill);
  DrawRectangleRoundedLinesEx(bounds, 0.22f, 10, focused ? 2.0f : 1.0f,
                              focused ? Gold : (accent ? Teal : Line));
  float size = std::min(20.0f, bounds.height * 0.37f);
  Vector2 textSize = Measure(label, size);
  const float availableWidth = std::max(1.0f, bounds.width - 24.0f);
  if (textSize.x > availableWidth) {
    size *= availableWidth / textSize.x;
    textSize = Measure(label, size);
  }
  Text(label, bounds.x + (bounds.width - textSize.x) * 0.5f,
       bounds.y + (bounds.height - textSize.y) * 0.5f, size,
       !enabled ? Muted : (accent ? Navy : Cream));
  return activated;
}

void ProgressBar(Rectangle bounds, float progress, Color color) {
  if (bounds.width <= 0.0f || bounds.height <= 0.0f) return;
  progress = std::isfinite(progress) ? std::clamp(progress, 0.0f, 1.0f) : 0.0f;
  DrawRectangleRounded(bounds, 1.0f, 12, Line);
  if (progress > 0.0f)
    DrawRectangleRounded({bounds.x, bounds.y, bounds.width * progress, bounds.height},
                         1.0f, 12, color);
}

Canvas::Canvas(float designWidth, float designHeight) {
  if (designWidth <= 0.0f || designHeight <= 0.0f) return;
  const float width = static_cast<float>(GetScreenWidth());
  const float height = static_cast<float>(GetScreenHeight());
  scale_ = std::max(0.01f, std::min(width / designWidth, height / designHeight));
  offset_ = {(width - designWidth * scale_) * 0.5f,
             (height - designHeight * scale_) * 0.5f};
}

Rectangle Canvas::ToScreen(Rectangle bounds) const {
  return {offset_.x + bounds.x * scale_, offset_.y + bounds.y * scale_,
          bounds.width * scale_, bounds.height * scale_};
}
Vector2 Canvas::ToScreen(Vector2 position) const {
  return {offset_.x + position.x * scale_, offset_.y + position.y * scale_};
}
}  // namespace hp::ui
