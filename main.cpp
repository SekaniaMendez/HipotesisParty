#include <emscripten_config.h>
#include <game.h>
#include <raylib.h>
#include <cstdint>
#include <cstdlib>
#include <string_view>
int main(int argc, char** argv) {
  std::uint32_t seed = 0;
  bool smokeTest = false;
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == "--smoke-test") smokeTest = true;
    else if (std::string_view(argv[i]) == "--seed" && i + 1 < argc)
      seed = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 10));
  }
#ifdef __EMSCRIPTEN__
  // The web shell scales a fixed-aspect canvas; native resize changes its buffer.
  SetConfigFlags(FLAG_MSAA_4X_HINT);
#else
  SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
#endif
  InitWindow(1440, 900, "Hipotesis Party | El laberinto de las ideas");
  if (!IsWindowReady()) {
    TraceLog(LOG_ERROR, "No graphics display available. Open the web client to play.");
    return 1;
  }
  SetWindowMinSize(960, 600);
  SetExitKey(KEY_NULL);
  SetTargetFPS(smokeTest ? 0 : 60);
  int exitCode = 0;
  {
#ifdef __EMSCRIPTEN__
    static Game game(seed, smokeTest);
    startWebGame(game);
#else
    Game game(seed, smokeTest);
    while (!WindowShouldClose() && !game.ShouldQuit()) {
      game.UpdateDrawFrame();
      game.drawFrame();
    }
    exitCode = game.ExitCode();
#endif
  }
  CloseWindow();
  return exitCode;
}
