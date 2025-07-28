#include "bnBattleTextIntroState.h"

#include "../../bnPlayer.h"
#include "../bnBattleSceneBase.h"
#include "../../bnAudioResourceManager.h"
#include "../../bnTextureResourceManager.h"
#include "../../bnPlayerSelectedCardsUI.h"

BattleTextIntroState::BattleTextIntroState() :
  battleStart(Font::Style::battle)
{
  battleStart.SetLetterSpacing(0);
  battleStartPos = sf::Vector2f(240.f, 140.f);
  battleStart.setPosition(battleStartPos);
  battleStart.setScale(2.f, 2.f);
}

BattleTextIntroState::~BattleTextIntroState()
{
}

void BattleTextIntroState::SetStartupDelay(frame_time_t frames)
{
  startupDelay = frames;
}

void BattleTextIntroState::SetIntroText(const std::string& str)
{
  // Font uses the underscore `_` glyph as background-only pieces
  static std::string space = std::string(1, ' ');
  static std::string underscore = std::string(1, '_');
  static std::string L = "< ";
  static std::string R = " >";
  battleStart.SetString(stx::replace(L + str + R, space, underscore));
  battleStart.setOrigin(battleStart.GetLocalBounds().width / 2.0f, battleStart.GetLocalBounds().height / 2.0f);
}

void BattleTextIntroState::onStart(const BattleSceneState*)
{
  timer.reset();
  timer.start();
}

void BattleTextIntroState::onEnd(const BattleSceneState*)
{
}

void BattleTextIntroState::onUpdate(double elapsed)
{
  timer.tick();
}

void BattleTextIntroState::onDraw(sf::RenderTexture& surface)
{
  frame_time_t start = timer.elapsed();

  if (start >= startupDelay) {
    double delta = (start - startupDelay).asSeconds().value;
    double length = preBattleLength.asSeconds().value;

    double scale = swoosh::ease::wideParabola(delta, length, 2.0);
    battleStart.setScale(2.f, static_cast<float>(scale * 2.0));

    surface.draw(GetScene().GetCardSelectWidget());
    surface.draw(battleStart);
  }
}

bool BattleTextIntroState::IsFinished() {
  return timer.elapsed() >= startupDelay + preBattleLength;
}