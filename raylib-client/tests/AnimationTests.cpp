#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "Animation.h"

namespace {
void Check(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

template <typename Exception, typename Function>
void CheckThrows(Function function) {
  try {
    function();
  } catch (const Exception&) {
    return;
  }
  throw std::runtime_error("No se rechazo una entrada invalida.");
}

Texture2D MakeTexture() {
  // Solo metadatos: las pruebas no necesitan ventana, GPU ni archivos de
  // imagen.
  Texture2D texture{};
  texture.width = 256;
  texture.height = 128;
  return texture;
}

void TestSpriteSheet() {
  const SpriteSheet sheet(MakeTexture(), 2, 4);
  Check(sheet.GetRows() == 2 && sheet.GetColumns() == 4 &&
            sheet.GetFrameCount() == 8,
        "Dimensiones incorrectas.");
  const Rectangle frame = sheet.GetFrame(6);
  Check(frame.x == 128 && frame.y == 64 && frame.width == 64 &&
            frame.height == 64,
        "El indice debe avanzar por filas.");
  const SpriteSheet rectangular(MakeTexture(), 4, 4);
  const Rectangle last = rectangular.GetFrame(15);
  Check(last.x == 192 && last.y == 96 && last.width == 64 && last.height == 32,
        "Los sprites pueden ser rectangulares.");
  CheckThrows<std::out_of_range>([&] { sheet.GetFrame(-1); });
  CheckThrows<std::out_of_range>([&] { sheet.GetFrame(8); });
  CheckThrows<std::invalid_argument>([] { SpriteSheet(MakeTexture(), 0, 4); });
  CheckThrows<std::invalid_argument>([] { SpriteSheet(MakeTexture(), 2, 0); });
  CheckThrows<std::invalid_argument>(
      [] { SpriteSheet(MakeTexture(), 129, 4); });
  CheckThrows<std::invalid_argument>([] { SpriteSheet(Texture2D{}, 2, 4); });
}

void TestLoopAndControls(const SpriteSheet& sheet) {
  Animation animation(sheet, 0.25f, AnimationMode::Loop, 4, 4);
  Check(animation.IsPlaying() && animation.GetCurrentFrame() == 4,
        "Debe iniciar en el primer sprite del rango.");
  animation.Update(0.125f);
  animation.Pause();
  animation.Update(100.0f);
  Check(animation.GetCurrentFrame() == 4 && !animation.IsPlaying(),
        "La pausa debe congelar la animacion.");
  animation.Play();
  animation.Update(0.125f);
  Check(animation.GetCurrentFrame() == 5,
        "Play debe conservar el tiempo parcial.");
  const Rectangle source = animation.GetSourceRectangle();
  Check(source.x == 64 && source.y == 64,
        "Debe exponer el sprite actual para dibujar.");
  animation.Play();
  Check(animation.GetCurrentFrame() == 5, "Loop no se reinicia con Play.");
  animation.Update(2.875f);
  Check(animation.GetCurrentFrame() == 4,
        "Debe saltar ciclos y conservar el sobrante.");
  animation.Update(0.125f);
  Check(animation.GetCurrentFrame() == 5 && !animation.HasFinished(),
        "Loop debe volver al principio sin terminar.");
  animation.Stop();
  Check(animation.GetCurrentFrame() == 4 && !animation.IsPlaying() &&
            !animation.HasFinished(),
        "Stop debe restaurar el estado inicial detenido.");
  animation.Play();
  animation.Update(0.125f);
  Check(animation.GetCurrentFrame() == 4,
        "Stop debe limpiar el tiempo parcial.");
  animation.Restart();
  Check(animation.IsPlaying() && animation.GetCurrentFrame() == 4,
        "Restart debe reiniciar y reproducir.");

  Animation fromOffset(sheet, 0.25f, AnimationMode::Loop, 6);
  fromOffset.Update(0.5f);
  Check(fromOffset.GetCurrentFrame() == 6,
        "El rango omitido usa el resto de la hoja.");
}

void TestOnce(const SpriteSheet& sheet) {
  Animation animation(sheet, std::vector<float>{0.125f, 0.25f, 0.5f},
                      AnimationMode::Once, 2);
  animation.Update(0.125f);
  Check(animation.GetCurrentFrame() == 3, "Debe respetar la primera duracion.");
  animation.Update(0.25f);
  Check(animation.GetCurrentFrame() == 4 && !animation.HasFinished(),
        "El ultimo sprite debe mostrarse durante toda su duracion.");
  animation.Update(0.375f);
  Check(!animation.HasFinished(), "Once no debe terminar antes de tiempo.");
  animation.Update(0.125f);
  Check(animation.HasFinished() && !animation.IsPlaying() &&
            animation.GetCurrentFrame() == 4,
        "Once debe terminar en el ultimo sprite.");
  animation.Update(100.0f);
  Check(animation.HasFinished() && animation.GetCurrentFrame() == 4,
        "El estado final debe persistir.");
  animation.Play();
  Check(animation.IsPlaying() && !animation.HasFinished() &&
            animation.GetCurrentFrame() == 2,
        "Once debe poder reproducirse de nuevo.");
  animation.Update(100.0f);
  Check(animation.HasFinished(),
        "Un delta grande tambien debe completar Once.");
  animation.Stop();
  Check(!animation.HasFinished(), "Stop debe limpiar la finalizacion.");
}

void TestReverseLoop(const SpriteSheet& sheet) {
  Animation animation(sheet, 0.25f, AnimationMode::ReverseLoop, 2, 4);
  for (const int expected : {3, 4, 5, 4, 3, 2, 3, 4}) {
    animation.Update(0.25f);
    Check(animation.GetCurrentFrame() == expected,
          "ReverseLoop debe ir y volver sin duplicar extremos.");
  }
  animation.Pause();
  animation.Play();
  animation.Update(0.25f);
  Check(animation.GetCurrentFrame() == 5, "Play debe conservar la direccion.");
  animation.Update(1500.25f);
  Check(animation.GetCurrentFrame() == 4,
        "Debe saltar ciclos desde el extremo.");
  animation.Update(1500.25f);
  Check(animation.GetCurrentFrame() == 3,
        "Debe saltar ciclos durante la vuelta.");
  animation.Restart();
  animation.Update(0.25f);
  Check(animation.GetCurrentFrame() == 3,
        "Restart debe restaurar la direccion.");

  Animation timed(sheet, std::vector<float>{0.125f, 0.25f, 0.5f},
                  AnimationMode::ReverseLoop);
  timed.Update(0.875f);
  Check(timed.GetCurrentFrame() == 1,
        "La vuelta debe usar la duracion de cada sprite.");
  timed.Update(1125.25f);
  Check(timed.GetCurrentFrame() == 0 && !timed.HasFinished(),
        "El ciclo de ida y vuelta debe considerar duraciones distintas.");

  Animation twoFrames(sheet, 0.25f, AnimationMode::ReverseLoop, 0, 2);
  for (const int expected : {1, 0, 1, 0}) {
    twoFrames.Update(0.25f);
    Check(twoFrames.GetCurrentFrame() == expected,
          "Debe funcionar con dos sprites.");
  }
}

void TestRestartLoopAndSingleFrame(const SpriteSheet& sheet) {
  Animation animation(sheet, 0.25f, AnimationMode::RestartLoop, 3, 3);
  animation.Update(0.375f);
  animation.Play();
  Check(animation.GetCurrentFrame() == 3,
        "RestartLoop debe reiniciar incluso activo.");
  animation.Update(0.125f);
  Check(animation.GetCurrentFrame() == 3,
        "RestartLoop debe limpiar el tiempo parcial.");
  animation.Update(0.125f);
  Check(animation.GetCurrentFrame() == 4,
        "RestartLoop debe seguir reproduciendose.");
  animation.Pause();
  animation.Play();
  Check(animation.GetCurrentFrame() == 3,
        "RestartLoop debe reiniciar tras una pausa.");
  animation.Update(0.75f);
  Check(animation.GetCurrentFrame() == 3 && !animation.HasFinished(),
        "RestartLoop debe repetir el rango completo.");

  for (const auto mode : {AnimationMode::Loop, AnimationMode::ReverseLoop,
                          AnimationMode::Once, AnimationMode::RestartLoop}) {
    Animation single(sheet, 0.25f, mode, 7, 1);
    single.Update(100.0f);
    Check(single.GetCurrentFrame() == 7,
          "Un solo sprite no debe salir de su rango.");
    Check(single.HasFinished() == (mode == AnimationMode::Once),
          "Solo Once debe terminar con un sprite.");
  }
}

void TestValidationAndIndependence(const SpriteSheet& sheet) {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  for (const float duration : {0.0f, -1.0f, nan, infinity}) {
    CheckThrows<std::invalid_argument>([&] { Animation(sheet, duration); });
    CheckThrows<std::invalid_argument>(
        [&] { Animation(sheet, std::vector<float>{0.25f, duration}); });
  }
  CheckThrows<std::invalid_argument>(
      [&] { Animation(sheet, std::vector<float>{}); });
  CheckThrows<std::invalid_argument>(
      [&] { Animation(sheet, 0.25f, AnimationMode::Loop, -1); });
  CheckThrows<std::invalid_argument>(
      [&] { Animation(sheet, 0.25f, AnimationMode::Loop, 0, -1); });
  CheckThrows<std::invalid_argument>(
      [&] { Animation(sheet, 0.25f, AnimationMode::Loop, 8); });
  CheckThrows<std::invalid_argument>(
      [&] { Animation(sheet, 0.25f, AnimationMode::Loop, 7, 2); });
  CheckThrows<std::invalid_argument>([&] {
    Animation(sheet, std::vector<float>{0.25f, 0.5f}, AnimationMode::Loop, 7);
  });
  Animation first(sheet, 0.25f);
  Animation second(sheet, 0.25f);
  for (const float delta : {-1.0f, nan, infinity}) {
    CheckThrows<std::invalid_argument>([&] { first.Update(delta); });
  }
  first.Update(0.0f);
  Check(first.GetCurrentFrame() == 0, "Delta cero no debe avanzar.");
  first.Update(0.5f);
  Check(first.GetCurrentFrame() == 2 && second.GetCurrentFrame() == 0,
        "Compartir una hoja no debe compartir el estado de reproduccion.");
  first.Update(std::numeric_limits<float>::max());
  Check(first.GetCurrentFrame() >= 0 &&
            first.GetCurrentFrame() < sheet.GetFrameCount(),
        "Un delta enorme debe mantenerse dentro del rango.");
}
}  // namespace

int main() {
  try {
    TestSpriteSheet();
    const SpriteSheet sheet(MakeTexture(), 2, 4);
    TestLoopAndControls(sheet);
    TestOnce(sheet);
    TestReverseLoop(sheet);
    TestRestartLoopAndSingleFrame(sheet);
    TestValidationAndIndependence(sheet);
    std::cout << "Pruebas de SpriteSheet y Animation completadas.\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
