// Opt-in graphical integration test: needs a working native window/OpenGL
// context. Unlike ResourceManagerTests, this uses the real raylib loaders.
#include "ResourceManager.h"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#ifndef HIPOTESIS_ASSET_DIR
#define HIPOTESIS_ASSET_DIR "assets"
#endif

namespace {
constexpr std::array<const char*, 7> characters{
    "professor", "student_f01", "student_f02", "student_f03",
    "student_m01", "student_m02", "student_m03"};
constexpr std::array<const char*, 5> props{
    "floor", "wall", "podium", "glyph", "pillar"};
constexpr std::array<const char*, 3> clipNames{"Idle", "Walk", "Celebrate"};

void Check(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

bool Finite(Vector3 value) {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

void CheckTransform(const Transform& transform, const std::string& label) {
  Check(Finite(transform.translation) && Finite(transform.scale) &&
            std::isfinite(transform.rotation.x) &&
            std::isfinite(transform.rotation.y) &&
            std::isfinite(transform.rotation.z) &&
            std::isfinite(transform.rotation.w), label + ": non-finite pose");
}

void CheckGeometry(const Model& model, const std::string& label) {
  Check(IsModelValid(model), label + ": invalid native model");
  const BoundingBox bounds = GetModelBoundingBox(model);
  Check(Finite(bounds.min) && Finite(bounds.max), label + ": non-finite bounds");
  const Vector3 size{bounds.max.x - bounds.min.x,
                     bounds.max.y - bounds.min.y,
                     bounds.max.z - bounds.min.z};
  Check(size.x > 0 && size.y > 0 && size.z > 0 && size.x < 20 &&
            size.y < 20 && size.z < 20, label + ": implausible model size");
  for (int m = 0; m < model.meshCount; ++m) {
    const Mesh& mesh = model.meshes[m];
    Check(mesh.vertexCount > 0 && mesh.triangleCount > 0 && mesh.vertices,
          label + ": missing mesh geometry");
    const float* positions = mesh.animVertices ? mesh.animVertices : mesh.vertices;
    for (int i = 0; i < mesh.vertexCount * 3; ++i) {
      Check(std::isfinite(positions[i]) && std::abs(positions[i]) < 100,
            label + ": invalid animated vertex");
    }
    if (mesh.animNormals) {
      for (int i = 0; i < mesh.vertexCount * 3; ++i) {
        Check(std::isfinite(mesh.animNormals[i]), label + ": invalid animated normal");
      }
    }
  }
}

void CheckCurrentPose(const Model& model, const std::string& label) {
  Check(model.currentPose != nullptr, label + ": missing runtime pose");
  for (unsigned int i = 0; i < model.skeleton.boneCount; ++i) {
    CheckTransform(model.currentPose[i], label);
    if (model.boneMatrices) {
      const Matrix& matrix = model.boneMatrices[i];
      const std::array<float, 16> values{
          matrix.m0, matrix.m1, matrix.m2, matrix.m3,
          matrix.m4, matrix.m5, matrix.m6, matrix.m7,
          matrix.m8, matrix.m9, matrix.m10, matrix.m11,
          matrix.m12, matrix.m13, matrix.m14, matrix.m15};
      Check(std::all_of(values.begin(), values.end(),
                        [](float value) { return std::isfinite(value); }),
            label + ": invalid bone matrix");
    }
  }
}

void DrawSample(const Model& model, const std::string& label) {
  const Camera3D camera{{3.5f, 2.6f, 4.5f}, {0, 0.9f, 0},
                        {0, 1, 0}, 40, CAMERA_PERSPECTIVE};
  BeginDrawing();
  ClearBackground({20, 26, 32, 255});
  BeginMode3D(camera);
  DrawPlane({0, -0.02f, 0}, {6, 6}, {70, 77, 81, 255});
  DrawModel(model, {0, 0, 0}, 1.0f, WHITE);
  EndMode3D();
  DrawText("Native model and animation verification", 14, 12, 18, RAYWHITE);
  DrawText(label.c_str(), 14, 38, 16, {92, 222, 196, 255});
  EndDrawing();
}

void RunIntegration() {
  // Scope guarantees GPU allocations are released while the context is alive,
  // including failure paths that throw from any assertion below.
  ResourceManager resources;
  const std::string root = std::string(HIPOTESIS_ASSET_DIR) + "/models/";
  for (const auto* name : characters) {
    const std::string path = root + name + ".glb";
    Check(resources.QueueModel(name, path), "cannot queue model " + path);
    Check(resources.QueueModelAnimations(name, path), "cannot queue clips " + path);
  }
  for (const auto* name : props) {
    Check(resources.QueueModel(name, root + name + ".glb"),
          std::string("cannot queue prop ") + name);
  }
  Check(resources.GetTotalResourceCount() == 19, "expected 19 queued resources");
  std::size_t iterations = 0;
  while (!resources.IsLoadingComplete()) {
    const auto before = resources.GetProcessedResourceCount();
    resources.UpdateLoading(1);
    Check(resources.GetProcessedResourceCount() == before + 1,
          "incremental loader did not process exactly one resource");
    BeginDrawing();
    ClearBackground({20, 26, 32, 255});
    resources.DrawLoading({24, 180, 590, 24});
    EndDrawing();
    Check(++iterations <= 19, "resource queue did not finish");
  }
  for (const auto& failed : resources.GetFailedResources()) {
    std::cerr << "Failed resource: " << failed.name << " -> " << failed.path << '\n';
  }
  Check(resources.GetFailedResources().empty(), "native asset loading failed");
  Check(resources.GetLoadedResourceCount() == 19 &&
            resources.GetLoadingProgress() == 1.0f, "resource count/progress mismatch");

  for (const auto* name : characters) {
    Model* model = resources.GetModel(name);
    auto* clips = resources.GetModelAnimations(name);
    Check(model && clips && clips->animations, std::string(name) + ": missing resources");
    CheckGeometry(*model, name);
    Check(model->skeleton.boneCount > 0 && model->skeleton.bones &&
              model->skeleton.bindPose, std::string(name) + ": missing skeleton");
    for (const auto* wanted : clipNames) {
      ModelAnimation* clip = nullptr;
      for (int i = 0; i < clips->count; ++i) {
        if (std::strcmp(clips->animations[i].name, wanted) == 0) {
          clip = &clips->animations[i];
          break;
        }
      }
      const std::string label = std::string(name) + " / " + wanted;
      Check(clip != nullptr, label + ": missing clip");
      Check(IsModelAnimationValid(*model, *clip), label + ": incompatible skeleton");
      Check(clip->keyframeCount > 1 && clip->keyframePoses,
            label + ": missing animation keyframes");
      Check(clip->boneCount == model->skeleton.boneCount,
            label + ": bone count mismatch");
      for (int frame = 0; frame < clip->keyframeCount; ++frame) {
        Check(clip->keyframePoses[frame] != nullptr, label + ": missing keyframe pose");
        for (unsigned int bone = 0; bone < clip->boneCount; ++bone) {
          CheckTransform(clip->keyframePoses[frame][bone], label);
        }
      }
      // Cover initial/final poses and fractional frame interpolation.
      for (const float frame : {0.0f, (clip->keyframeCount - 1) * 0.25f,
                                (clip->keyframeCount - 1) * 0.5f,
                                static_cast<float>(clip->keyframeCount - 1)}) {
        UpdateModelAnimation(*model, *clip, frame);
        CheckCurrentPose(*model, label);
        CheckGeometry(*model, label);
        DrawSample(*model, label);
      }
      std::cout << "PASS " << label << ": " << clip->keyframeCount
                << " frames, " << clip->boneCount << " bones\n";
    }
  }
  for (const auto* name : props) {
    Model* model = resources.GetModel(name);
    Check(model != nullptr, std::string(name) + ": missing prop");
    CheckGeometry(*model, name);
    DrawSample(*model, name);
    std::cout << "PASS " << name << ": native load and draw\n";
  }
  resources.UnloadAll();
  Check(resources.GetTotalResourceCount() == 0 &&
            resources.GetModel("professor") == nullptr &&
            resources.GetModelAnimations("professor") == nullptr,
        "resource cleanup did not reset the manager");
}
}  // namespace

int main() {
  int result = 0;
  try {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(640, 400, "HipotesisParty asset integration test");
    Check(IsWindowReady(), "window/OpenGL context unavailable");
    SetTargetFPS(60);
    RunIntegration();
    std::cout << "All 19 native resources and 21 animation clips passed.\n";
  } catch (const std::exception& error) {
    std::cerr << "Model asset integration failure: " << error.what() << '\n';
    result = 1;
  }
  if (IsWindowReady()) CloseWindow();
  return result;
}
