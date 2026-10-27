/**
 * @file Animation.h
 * @brief Reproducción de una secuencia de sprites independiente de la entidad.
 */
#pragma once

#include <vector>

#include "SpriteSheet.h"

enum class AnimationMode {
  Loop, /**< Repite la secuencia; Play() continúa desde el estado actual. */
  ReverseLoop, /**< Va y vuelve sin repetir los extremos: 0, 1, 2, 1, 0... */
  Once,        /**< Se reproduce una vez y conserva el último sprite. */
  RestartLoop /**< Repite la secuencia; cada Play() reinicia desde el principio.
               */
};

class Animation {
 public:
  /**
   * @brief Crea una animación con la misma duración, en segundos, para cada
   * sprite. Usa un rango consecutivo de la hoja. frameCount = 0 incluye todos
   * los sprites desde firstFrame. La animación empieza reproduciéndose.
   */
  Animation(const SpriteSheet& spriteSheet, float frameDuration,
            AnimationMode mode = AnimationMode::Loop, int firstFrame = 0,
            int frameCount = 0);

  /**
   * @brief Asigna a cada sprite su propia duración, en segundos.
   * El tamaño de frameDurations determina cuántos sprites se usan desde
   * firstFrame.
   */
  Animation(const SpriteSheet& spriteSheet, std::vector<float> frameDurations,
            AnimationMode mode = AnimationMode::Loop, int firstFrame = 0);

  /** @brief Avanza con el tiempo transcurrido en segundos, por ejemplo
   * GetFrameTime(). */
  void Update(float deltaTime);

  /** @brief Continúa; reinicia si es RestartLoop o si una animación Once ya
   * terminó. */
  void Play();
  /** @brief Pausa conservando el sprite y el tiempo acumulado. */
  void Pause();
  /** @brief Detiene y vuelve al primer sprite; limpia HasFinished(). */
  void Stop();
  /** @brief Vuelve al primer sprite y empieza a reproducir cualquier modo. */
  void Restart();

  bool IsPlaying() const;
  /** @brief Permanece true al terminar Once hasta volver a reproducir o
   * reiniciar. */
  bool HasFinished() const;
  /** @brief Índice absoluto del sprite actual en la hoja. */
  int GetCurrentFrame() const;
  Rectangle GetSourceRectangle() const;
  const SpriteSheet& GetSpriteSheet() const;

 private:
  SpriteSheet spriteSheet;
  std::vector<float> frameDurations;
  AnimationMode mode;
  int firstFrame;
  int currentFrame = 0;
  int direction = 1;
  double elapsedTime = 0.0;
  double cycleDuration = 0.0;
  bool playing = true;
  bool finished = false;

  void AdvanceFrame();
};
