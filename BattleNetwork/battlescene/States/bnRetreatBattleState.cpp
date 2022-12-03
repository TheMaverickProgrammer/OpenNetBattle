#include "bnRetreatBattleState.h"
#include "bnBattleStartBattleState.h"

#include "../bnBattleSceneBase.h"
#include "../../bnAudioResourceManager.h"
#include "../../bnTextureResourceManager.h"
#include "../../bnAnimatedTextBox.h"
#include "../../bnMessage.h"

RetreatBattleState::RetreatBattleState(AnimatedTextBox& textbox, const sf::Sprite& mug, const Animation& anim) : 
  textbox(textbox),
  mug(mug),
  anim(anim) {
 
}

void RetreatBattleState::onStart(const BattleSceneState*)
{
  if (rand() % 10 < 5) {
    textbox.EnqueMessage(mug, anim, new Message("\x01...\x01 It's no good!\nWe can't run away!"));
  }
  else {
    textbox.EnqueMessage(mug, anim, new Message("\x01...\x01 OK!\nWe got away!"));
    escaped = true;
  }
  currState = RetreatBattleState::state::pending;
  textbox.Open();

  GetScene().BattleResultsObj().runaway = true;
}

void RetreatBattleState::onUpdate(double elapsed)
{
  if(currState == RetreatBattleState::state::pending) {
    if (textbox.IsEndOfBlock() && Input().Has(InputEvents::pressed_confirm)) {
      if (escaped) {
        currState = RetreatBattleState::state::success;
      }
      else {
        currState = RetreatBattleState::state::fail;
      }
      textbox.Close();
    }
  }
}

void RetreatBattleState::onDraw(IRenderer& renderer)
{
  renderer.submit(&GetScene().GetCardSelectWidget());
}

void RetreatBattleState::onEnd(const BattleSceneState*)
{
}

bool RetreatBattleState::Success() {
  return textbox.IsClosed() && currState == RetreatBattleState::state::success;
}

bool RetreatBattleState::Fail() {
  return textbox.IsClosed() && currState == RetreatBattleState::state::fail;
}