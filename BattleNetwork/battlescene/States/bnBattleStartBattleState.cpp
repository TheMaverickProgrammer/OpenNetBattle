#include "bnBattleStartBattleState.h"

#include "../../bnPlayer.h"
#include "../bnBattleSceneBase.h"
#include "../../bnAudioResourceManager.h"
#include "../../bnTextureResourceManager.h"
#include "../../bnPlayerSelectedCardsUI.h"


BattleStartBattleState::BattleStartBattleState() : BattleTextIntroState()
{
  SetIntroText("Battle Start!");
}

void BattleStartBattleState::onStart(const BattleSceneState* _)
{
  BattleTextIntroState::onStart(_);

  GetScene().IncrementTurnCount();

  // only reveal first player's UI widget to them
  std::shared_ptr<Player> localPlayer = GetScene().GetLocalPlayer();
  std::shared_ptr<PlayerSelectedCardsUI> ui = localPlayer->GetFirstComponent<PlayerSelectedCardsUI>();

  if (ui) {
    ui->Reveal();
  }
}