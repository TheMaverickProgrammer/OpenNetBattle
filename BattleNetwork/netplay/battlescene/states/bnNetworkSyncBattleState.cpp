#include "bnNetworkSyncBattleState.h"
#include "../bnNetworkBattleScene.h"
#include "../../battlescene/States/bnCardSelectBattleState.h"

NetworkSyncBattleState::NetworkSyncBattleState(NetworkBattleScene* scene) :
  scene(scene),
  NetworkBattleSceneState() {
}

NetworkSyncBattleState::~NetworkSyncBattleState() {
}

void NetworkSyncBattleState::onStart(const BattleSceneState* next) {
  Logger::Log(LogLevel::net, "NetworkSyncBattleState::onStart");
  if (onStartCallback) {
    onStartCallback(next);
  }
}

void NetworkSyncBattleState::onEnd(const BattleSceneState* next) {
  Logger::Logf(LogLevel::net, "Synced on frame: %d", scene->FrameNumber().count());

  if (onEndCallback) {
    onEndCallback(next);
  }

  requestedSync = false;
  remoteRequestedSync = false;
}

void NetworkSyncBattleState::SetStartCallback(std::function<void(const BattleSceneState*)> callback) {
  onStartCallback = callback;
}

void NetworkSyncBattleState::SetEndCallback(std::function<void(const BattleSceneState*)> callback) {
  onEndCallback = callback;
}

bool NetworkSyncBattleState::IsReady() const {
  return requestedSync && remoteRequestedSync && scene->FrameNumber() == syncFrame;
}

void NetworkSyncBattleState::MarkSyncRequested() {
  Logger::Log(LogLevel::net, "Requested sync");
  requestedSync = true;
}

void NetworkSyncBattleState::MarkRemoteSyncRequested() {
  Logger::Log(LogLevel::net, "Remote requested sync");

  remoteRequestedSync = true;
}

frame_time_t NetworkSyncBattleState::GetSyncFrame() const {
  return syncFrame;
}

bool NetworkSyncBattleState::SetSyncFrame(frame_time_t frame) {
  if (frame.count() < syncFrame.count()) {
    return false;
  }

  Logger::Log(LogLevel::net, "Set sync frame to " + std::to_string(frame.count()));

  syncFrame = frame;
  return true;
}

void NetworkSyncBattleState::onUpdate(double elapsed) {
  flicker += from_seconds(elapsed);
}

void NetworkSyncBattleState::onDraw(sf::RenderTexture& surface) {
  if (flicker.count() % 60 > 30) {
    Text label = Text("Waiting...", Font::Style::thick);
    label.setScale(2.0f, 2.0f);
    label.setOrigin(label.GetLocalBounds().width, label.GetLocalBounds().height * 0.5f);


    sf::Vector2f position = sf::Vector2f(470.0f, 80.0f);;
    label.SetColor(sf::Color::Black);
    label.setPosition(position.x + 2.f, position.y + 2.f);
    surface.draw(label);
    label.SetColor(sf::Color::White);
    label.setPosition(position);
    surface.draw(label);
  }
}