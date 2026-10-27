#include "Animation.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace {
std::vector<float> MakeDurations(const SpriteSheet& spriteSheet, float duration,
                                 int firstFrame, int frameCount) {
  const int totalFrames = spriteSheet.GetFrameCount();
  if (firstFrame < 0 || firstFrame >= totalFrames || frameCount < 0 ||
      frameCount > totalFrames - firstFrame) {
    throw std::invalid_argument("El rango de animacion esta fuera de la hoja.");
  }
  if (!std::isfinite(duration) || duration <= 0.0f) {
    throw std::invalid_argument(
        "La duracion de cada sprite debe ser positiva y finita.");
  }
  if (frameCount == 0) {
    frameCount = totalFrames - firstFrame;
  }
  return std::vector<float>(frameCount, duration);
}
}  // namespace

Animation::Animation(const SpriteSheet& spriteSheet, float frameDuration,
                     AnimationMode mode, int firstFrame, int frameCount)
    : Animation(
          spriteSheet,
          MakeDurations(spriteSheet, frameDuration, firstFrame, frameCount),
          mode, firstFrame) {}

Animation::Animation(const SpriteSheet& spriteSheet,
                     std::vector<float> frameDurations, AnimationMode mode,
                     int firstFrame)
    : spriteSheet(spriteSheet),
      frameDurations(std::move(frameDurations)),
      mode(mode),
      firstFrame(firstFrame) {
  if (firstFrame < 0 || firstFrame >= spriteSheet.GetFrameCount() ||
      this->frameDurations.empty() ||
      this->frameDurations.size() >
          static_cast<std::size_t>(spriteSheet.GetFrameCount() - firstFrame)) {
    throw std::invalid_argument("El rango de animacion esta fuera de la hoja.");
  }
  if (mode != AnimationMode::Loop && mode != AnimationMode::ReverseLoop &&
      mode != AnimationMode::Once && mode != AnimationMode::RestartLoop) {
    throw std::invalid_argument("El modo de animacion no es valido.");
  }
  for (std::size_t i = 0; i < this->frameDurations.size(); ++i) {
    const float duration = this->frameDurations[i];
    if (!std::isfinite(duration) || duration <= 0.0f) {
      throw std::invalid_argument(
          "La duracion de cada sprite debe ser positiva y finita.");
    }
    cycleDuration += duration;
    if (mode == AnimationMode::ReverseLoop && i > 0 &&
        i + 1 < this->frameDurations.size()) {
      cycleDuration += duration;
    }
  }
}

void Animation::Update(float deltaTime) {
  if (!std::isfinite(deltaTime) || deltaTime < 0.0f) {
    throw std::invalid_argument(
        "El tiempo transcurrido debe ser finito y no negativo.");
  }
  if (!playing) {
    return;
  }

  elapsedTime += deltaTime;
  if (mode != AnimationMode::Once) {
    // Saltar ciclos completos evita recorrer miles de sprites tras una pausa
    // larga.
    elapsedTime = std::fmod(elapsedTime, cycleDuration);
  }
  while (playing && elapsedTime >= frameDurations[currentFrame]) {
    elapsedTime -= frameDurations[currentFrame];
    AdvanceFrame();
  }
}

void Animation::AdvanceFrame() {
  const int frameCount = static_cast<int>(frameDurations.size());
  if (mode == AnimationMode::Once) {
    if (currentFrame == frameCount - 1) {
      finished = true;
      playing = false;
      elapsedTime = 0.0;
    } else {
      ++currentFrame;
    }
  } else if (mode == AnimationMode::ReverseLoop && frameCount > 1) {
    if (currentFrame == frameCount - 1) {
      direction = -1;
    } else if (currentFrame == 0) {
      direction = 1;
    }
    currentFrame += direction;
  } else {
    currentFrame = (currentFrame + 1) % frameCount;
  }
}

void Animation::Play() {
  if (mode == AnimationMode::RestartLoop || finished) {
    Restart();
  } else {
    playing = true;
  }
}

void Animation::Pause() { playing = false; }

void Animation::Stop() {
  currentFrame = 0;
  direction = 1;
  elapsedTime = 0.0;
  playing = false;
  finished = false;
}

void Animation::Restart() {
  Stop();
  playing = true;
}

bool Animation::IsPlaying() const { return playing; }

bool Animation::HasFinished() const { return finished; }

int Animation::GetCurrentFrame() const { return firstFrame + currentFrame; }

Rectangle Animation::GetSourceRectangle() const {
  return spriteSheet.GetFrame(GetCurrentFrame());
}

const SpriteSheet& Animation::GetSpriteSheet() const { return spriteSheet; }
