#include "game.h"
#include "PartyGame.h"
Game::Game(std::uint32_t seed, bool smokeTest)
    : impl_(std::make_unique<hp::PartyGame>(seed, smokeTest)) {}
Game::~Game() = default;
void Game::UpdateDrawFrame() { impl_->Update(); }
void Game::drawFrame() { impl_->Draw(); }
bool Game::ShouldQuit() const { return impl_->ShouldQuit(); }
int Game::ExitCode() const { return impl_->ExitCode(); }
