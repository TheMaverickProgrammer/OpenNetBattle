#include "bnNetworkSyncBattleState.h"
#include "../bnNetworkBattleScene.h"
#include "../../battlescene/States/bnCardSelectBattleState.h"
#include "../../../bnPlayerControlledState.h"
#include "../../../bnPlayer.h"

NetworkSyncBattleState::NetworkSyncBattleState(std::shared_ptr<Player>& remotePlayer, NetworkBattleScene* scene) :
  remotePlayer(remotePlayer),
  scene(scene),
  cardSelectState(scene->cardStatePtr),
  NetworkBattleSceneState()
{
}

NetworkSyncBattleState::~NetworkSyncBattleState()
{
}

void NetworkSyncBattleState::Synchronize()
{
  synchronized = true;
}

const bool NetworkSyncBattleState::IsSynchronized() const {
  return synchronized;
}

void NetworkSyncBattleState::onStart(const BattleSceneState* last)
{
  if (cardSelectState && last == cardSelectState) {
    // We have returned from the card select state to force a handshake and wait 
    scene->SendHandshakeSignal();
  }
}

void NetworkSyncBattleState::onEnd(const BattleSceneState* next)
{
  if (firstConnection) {
    GetScene().GetLocalPlayer()->ChangeState<PlayerControlledState>();
    
    if (remotePlayer) {
      remotePlayer->ChangeState<PlayerControlledState>();
    }

    firstConnection = false;
  }

  synchronized = false;
  scene->remoteState.remoteHandshake = false;
}

void NetworkSyncBattleState::onUpdate(double elapsed)
{
  flicker += from_seconds(elapsed);
}

void NetworkSyncBattleState::onDraw(sf::RenderTexture& surface)
{
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

bool NetworkSyncBattleState::IsRemoteConnected()
{
  return firstConnection && scene->remoteState.remoteConnected;
}

bool NetworkSyncBattleState::SelectedNewChips()
{
  if (cardSelectState && cardSelectState->SelectedNewChips() && synchronized) {
    Logger::Log(LogLevel::net, "SelectedNewChips has passed, moving to combo state");
  }
  return cardSelectState && cardSelectState->SelectedNewChips() && synchronized;
}

bool NetworkSyncBattleState::NoConditions()
{
  return cardSelectState && cardSelectState->OKIsPressed() && synchronized;
}

bool NetworkSyncBattleState::HasForm()
{
  return cardSelectState && (cardSelectState->HasForm() || scene->remoteState.remoteChangeForm) && synchronized;
}
