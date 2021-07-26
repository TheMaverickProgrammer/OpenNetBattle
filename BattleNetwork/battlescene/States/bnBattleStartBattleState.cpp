#include "bnBattleStartBattleState.h"

#include "../../bnPlayer.h"
#include "../bnBattleSceneBase.h"
#include "../../bnAudioResourceManager.h"
#include "../../bnTextureResourceManager.h"
#include "../../bnPlayerSelectedCardsUI.h"

BattleStartBattleState::BattleStartBattleState(std::vector<Player*>& tracked) : tracked(tracked) {
  battleStart = sf::Sprite(*LOAD_TEXTURE(BATTLE_START));
  battleStart.setOrigin(battleStart.getLocalBounds().width / 2.0f, battleStart.getLocalBounds().height / 2.0f);
  battleStartPos = sf::Vector2f(240.f, 140.f);
  battleStart.setPosition(battleStartPos);
  battleStart.setScale(2.f, 2.f);
}

void BattleStartBattleState::SetStartupDelay(frame_time_t frames)
{
  startupDelay = frames;
}

void BattleStartBattleState::onStart(const BattleSceneState*)
{
  battleStartTimer.reset();
  battleStartTimer.start();

  // only reveal first player's UI widget to them
  auto ui = GetScene().GetPlayer()->GetFirstComponent<PlayerSelectedCardsUI>();

  if (ui) {
    ui->Reveal();
  }
}

void BattleStartBattleState::onEnd(const BattleSceneState*)
{
}

void BattleStartBattleState::onUpdate(double elapsed)
{
  battleStartTimer.update(sf::seconds(static_cast<float>(elapsed)));
}

void BattleStartBattleState::onDraw(sf::RenderTexture& surface)
{
  frame_time_t start = from_milliseconds(battleStartTimer.getElapsed().asMilliseconds());

  if (start >= startupDelay) {
    double delta = (start - startupDelay).asSeconds().value;
    double length = preBattleLength.asSeconds().value;

    double scale = swoosh::ease::wideParabola(delta, length, 2.0);
    battleStart.setScale(2.f, static_cast<float>(scale * 2.0));

    surface.draw(GetScene().GetCardSelectWidget());
    surface.draw(battleStart);
  }
}

bool BattleStartBattleState::IsFinished() {
  return battleStartTimer.getElapsed().asSeconds() >= (startupDelay + preBattleLength).asSeconds().value;
}