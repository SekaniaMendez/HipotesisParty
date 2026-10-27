#include "SceneManager.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace hp {
SceneManager::~SceneManager() {
  if (current_) scenes_.at(*current_)->Exit();
}

bool SceneManager::Register(SceneId id, std::unique_ptr<Scene> scene) {
  if (!scene || scenes_.contains(id)) return false;
  scenes_.emplace(id, std::move(scene));
  return true;
}

bool SceneManager::Request(SceneId id) {
  if (!scenes_.contains(id)) return false;
  pending_ = id;
  return true;
}

void SceneManager::ApplyPending() {
  if (!pending_) return;
  const SceneId next = *pending_;
  pending_.reset();
  if (current_ && *current_ == next) return;
  if (current_) scenes_.at(*current_)->Exit();
  current_ = next;
  scenes_.at(next)->Enter();
  // Enter/Exit pueden solicitar otra escena. Se procesa en el próximo límite,
  // sin recursión ni bucles de transición dentro del mismo callback.
}

void SceneManager::Update(float deltaTime) {
  ApplyPending();
  if (current_)
    scenes_.at(*current_)->Update(std::isfinite(deltaTime) ? std::max(0.0f, deltaTime) : 0.0f);
  ApplyPending();
}

void SceneManager::Draw() {
  if (current_) scenes_.at(*current_)->Draw();
}
}  // namespace hp
