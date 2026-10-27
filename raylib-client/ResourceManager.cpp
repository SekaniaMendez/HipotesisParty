#include "ResourceManager.h"

#include <rlgl.h>

#include <algorithm>
#include <array>

ResourceManager::~ResourceManager() { UnloadAll(); }

bool ResourceManager::QueueTexture(const std::string& name,
                                   const std::string& path) {
  return QueueResource(ResourceType::Texture, name, path);
}

bool ResourceManager::QueueSound(const std::string& name,
                                 const std::string& path) {
  return QueueResource(ResourceType::Sound, name, path);
}

bool ResourceManager::QueueMusic(const std::string& name,
                                 const std::string& path) {
  return QueueResource(ResourceType::Music, name, path);
}

bool ResourceManager::QueueFont(const std::string& name,
                                const std::string& path, int fontSize) {
  if (fontSize <= 0) {
    return false;
  }
  return QueueResource(ResourceType::Font, name, path, fontSize);
}

bool ResourceManager::QueueModel(const std::string& name,
                                 const std::string& path) {
  return QueueResource(ResourceType::Model, name, path);
}

bool ResourceManager::QueueModelAnimations(const std::string& name,
                                           const std::string& path) {
  return QueueResource(ResourceType::ModelAnimations, name, path);
}

bool ResourceManager::QueueResource(ResourceType type, const std::string& name,
                                    const std::string& path, int fontSize) {
  if (name.empty() || path.empty()) {
    return false;
  }
  const bool duplicate =
      std::any_of(pendingResources.begin(), pendingResources.end(),
                  [&](const PendingResource& resource) {
                    return resource.type == type && resource.name == name;
                  });
  if (duplicate) {
    return false;
  }
  pendingResources.push_back({type, name, path, fontSize});
  return true;
}

void ResourceManager::UpdateLoading(std::size_t maxResources) {
  while (maxResources > 0 && !IsLoadingComplete()) {
    const PendingResource& resource = pendingResources[processedResources];
    const bool success = LoadResource(resource);
    if (success) {
      ++loadedResources;
    } else {
      failedResources.push_back({resource.name, resource.path});
    }
    loadHistory.push_back({resource.name, resource.path, success});
    ++processedResources;
    --maxResources;
  }
}

bool ResourceManager::LoadResource(const PendingResource& resource) {
  if (!FileExists(resource.path.c_str())) {
    return false;
  }
  // Insertar antes de cargar evita perder el recurso si falla la asignación del
  // mapa.
  switch (resource.type) {
    case ResourceType::Texture: {
      if (!IsWindowReady()) return false;
      auto [it, inserted] = textures.try_emplace(resource.name);
      if (!inserted) return false;
      it->second = LoadTexture(resource.path.c_str());
      if (IsTextureValid(it->second)) return true;
      UnloadTexture(it->second);
      textures.erase(it);
      return false;
    }
    case ResourceType::Sound: {
      if (!IsAudioDeviceReady()) return false;
      auto [it, inserted] = sounds.try_emplace(resource.name);
      if (!inserted) return false;
      it->second = LoadSound(resource.path.c_str());
      if (IsSoundValid(it->second)) return true;
      UnloadSound(it->second);
      sounds.erase(it);
      return false;
    }
    case ResourceType::Music: {
      if (!IsAudioDeviceReady()) return false;
      auto [it, inserted] = music.try_emplace(resource.name);
      if (!inserted) return false;
      it->second = LoadMusicStream(resource.path.c_str());
      if (IsMusicValid(it->second)) return true;
      UnloadMusicStream(it->second);
      music.erase(it);
      return false;
    }
    case ResourceType::Font: {
      if (!IsWindowReady()) return false;
      auto [it, inserted] = fonts.try_emplace(resource.name);
      if (!inserted) return false;
      // ASCII + Latin-1: conserva tildes, eñes y signos de apertura del juego.
      std::vector<int> codepoints(224);
      for (std::size_t i = 0; i < codepoints.size(); ++i)
        codepoints[i] = static_cast<int>(i) + 32;
      for (int symbol : {0x0304, 0x2013, 0x2014, 0x2026, 0x2265, 0x2264, 0x2248, 0x221A, 0x03BC, 0x03C3, 0x03B1, 0x2080, 0x2081, 0x2212, 0x2192}) codepoints.push_back(symbol);
      it->second = LoadFontEx(resource.path.c_str(), resource.fontSize,
                              codepoints.data(), static_cast<int>(codepoints.size()));
      // Raylib puede devolver su fuente predeterminada al fallar un archivo.
      if (it->second.texture.id == GetFontDefault().texture.id) {
        fonts.erase(it);
        return false;
      }
      if (IsFontValid(it->second) && IsTextureValid(it->second.texture))
        return true;
      UnloadFont(it->second);
      fonts.erase(it);
      return false;
    }
    case ResourceType::Model: {
      if (!IsWindowReady()) return false;
      auto [it, inserted] = models.try_emplace(resource.name);
      if (!inserted) return false;
      it->second.model = LoadModel(resource.path.c_str());
      TrackModelTextures(it->second);
      if (IsModelValid(it->second.model)) return true;
      UnloadModelResource(it->second);
      models.erase(it);
      return false;
    }
    case ResourceType::ModelAnimations: {
      auto [it, inserted] = modelAnimations.try_emplace(resource.name);
      if (!inserted) return false;
      auto& clips = it->second;
      clips.animations =
          LoadModelAnimations(resource.path.c_str(), &clips.count);
      if (clips.animations != nullptr && clips.count > 0) return true;
      if (clips.animations != nullptr) {
        UnloadModelAnimations(clips.animations, clips.count);
      }
      modelAnimations.erase(it);
      return false;
    }
  }
  return false;
}

bool ResourceManager::IsLoadingComplete() const {
  return processedResources == pendingResources.size();
}

float ResourceManager::GetLoadingProgress() const {
  return pendingResources.empty()
             ? 1.0f
             : static_cast<float>(processedResources) / pendingResources.size();
}

std::size_t ResourceManager::GetLoadedResourceCount() const {
  return loadedResources;
}

std::size_t ResourceManager::GetProcessedResourceCount() const {
  return processedResources;
}

std::string ResourceManager::GetNextResourceName() const {
  return IsLoadingComplete() ? std::string{} : pendingResources[processedResources].name;
}

std::string ResourceManager::GetNextResourcePath() const {
  return IsLoadingComplete() ? std::string{} : pendingResources[processedResources].path;
}

const std::vector<ResourceManager::LoadResult>& ResourceManager::GetLoadHistory() const {
  return loadHistory;
}

std::size_t ResourceManager::GetTotalResourceCount() const {
  return pendingResources.size();
}

const std::vector<ResourceManager::FailedResource>&
ResourceManager::GetFailedResources() const {
  return failedResources;
}

void ResourceManager::DrawLoading(Rectangle bounds) const {
  if (bounds.width <= 0 || bounds.height <= 0) return;
  const float progress = GetLoadingProgress();
  DrawRectangleRec(bounds, DARKGRAY);
  DrawRectangleRec({bounds.x, bounds.y, bounds.width * progress, bounds.height},
                   GREEN);
  DrawText(TextFormat("Cargando... %.0f%%", progress * 100),
           static_cast<int>(bounds.x), static_cast<int>(bounds.y - 30), 20,
           WHITE);
  if (!failedResources.empty()) {
    DrawText(TextFormat("Recursos con errores: %zu", failedResources.size()),
             static_cast<int>(bounds.x),
             static_cast<int>(bounds.y + bounds.height + 10), 20, RED);
  }
}

Texture2D* ResourceManager::GetTexture(const std::string& name) {
  auto it = textures.find(name);
  return it == textures.end() ? nullptr : &it->second;
}

const Texture2D* ResourceManager::GetTexture(const std::string& name) const {
  auto it = textures.find(name);
  return it == textures.end() ? nullptr : &it->second;
}

Sound* ResourceManager::GetSound(const std::string& name) {
  auto it = sounds.find(name);
  return it == sounds.end() ? nullptr : &it->second;
}

const Sound* ResourceManager::GetSound(const std::string& name) const {
  auto it = sounds.find(name);
  return it == sounds.end() ? nullptr : &it->second;
}

Music* ResourceManager::GetMusic(const std::string& name) {
  auto it = music.find(name);
  return it == music.end() ? nullptr : &it->second;
}

const Music* ResourceManager::GetMusic(const std::string& name) const {
  auto it = music.find(name);
  return it == music.end() ? nullptr : &it->second;
}

Font* ResourceManager::GetFont(const std::string& name) {
  auto it = fonts.find(name);
  return it == fonts.end() ? nullptr : &it->second;
}

const Font* ResourceManager::GetFont(const std::string& name) const {
  auto it = fonts.find(name);
  return it == fonts.end() ? nullptr : &it->second;
}

Model* ResourceManager::GetModel(const std::string& name) {
  auto it = models.find(name);
  return it == models.end() ? nullptr : &it->second.model;
}

const Model* ResourceManager::GetModel(const std::string& name) const {
  auto it = models.find(name);
  return it == models.end() ? nullptr : &it->second.model;
}

ResourceManager::ModelAnimationSet* ResourceManager::GetModelAnimations(
    const std::string& name) {
  auto it = modelAnimations.find(name);
  return it == modelAnimations.end() ? nullptr : &it->second;
}

const ResourceManager::ModelAnimationSet* ResourceManager::GetModelAnimations(
    const std::string& name) const {
  auto it = modelAnimations.find(name);
  return it == modelAnimations.end() ? nullptr : &it->second;
}

void ResourceManager::TrackModelTextures(ModelResource& resource) {
  const unsigned int defaultTexture = rlGetTextureIdDefault();
  const Model& model = resource.model;
  if (model.materials == nullptr) return;
  for (int i = 0; i < model.materialCount; ++i) {
    const MaterialMap* maps = model.materials[i].maps;
    if (maps == nullptr) continue;
    for (int j = MATERIAL_MAP_ALBEDO; j <= MATERIAL_MAP_BRDF; ++j) {
      const Texture2D texture = maps[j].texture;
      if (texture.id == 0 || texture.id == defaultTexture) continue;
      const bool tracked = std::any_of(
          resource.textures.begin(), resource.textures.end(),
          [&](Texture2D existing) { return existing.id == texture.id; });
      if (!tracked) resource.textures.push_back(texture);
    }
  }
}

void ResourceManager::UnloadModelResource(ModelResource& resource) {
  // UnloadModel libera mallas y materiales, pero no sus texturas.
  for (const Texture2D texture : resource.textures) UnloadTexture(texture);
  UnloadModel(resource.model);
}

void ResourceManager::UnloadAll() {
  for (const auto& [name, clips] : modelAnimations) {
    UnloadModelAnimations(clips.animations, clips.count);
  }
  for (auto& [name, resource] : models) UnloadModelResource(resource);
  for (const auto& [name, texture] : textures) UnloadTexture(texture);
  for (const auto& [name, sound] : sounds) UnloadSound(sound);
  for (const auto& [name, track] : music) UnloadMusicStream(track);
  for (const auto& [name, font] : fonts) UnloadFont(font);
  textures.clear();
  sounds.clear();
  music.clear();
  fonts.clear();
  models.clear();
  modelAnimations.clear();
  pendingResources.clear();
  failedResources.clear();
  loadHistory.clear();
  processedResources = 0;
  loadedResources = 0;
}
