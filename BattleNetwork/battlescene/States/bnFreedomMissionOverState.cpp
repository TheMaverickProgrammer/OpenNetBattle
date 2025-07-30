#include "bnFreedomMissionOverState.h"
#include "../bnBattleSceneBase.h"

#include "../../bnPlayer.h"
#include "../../bnField.h"
#include "../../bnCardAction.h"
#include "../../bnAudioResourceManager.h"
#include "../../bnTextureResourceManager.h"

#include <Swoosh/Ease.h>
#include "../bnFreedomMissionMobScene.h"

FreedomMissionOverState::FreedomMissionOverState() :
  BattleTextIntroState()
{ }

void FreedomMissionOverState::onStart(const BattleSceneState* _)
{
  BattleTextIntroState::onStart(_);

  FreedomMissionMobScene& scene = static_cast<FreedomMissionMobScene&>(GetScene());
  BattleResults& results = scene.BattleResultsObj();
  results.runaway = false;

  if (scene.IsPlayerDeleted()) {
    context = Conditions::player_deleted;
  }
  else {
    /*
    Handle results here, because there is no rewards state.
    If player was deleted, no other results should matter.

    This will still set results if the player failed, which may 
    be useful to a server so that emotion can be kept and so on.
    The rest of the data is unlikely to be useful in a failure, 
    but is added anyway.
  */
    std::shared_ptr<Player> player = scene.GetLocalPlayer();
    results.battleLength = sf::seconds(scene.GetElapsedBattleFrames().count() / 60.f);
    results.moveCount = player->GetMoveCount();
    results.hitCount = scene.GetPlayerHitCount();
    results.turns = scene.GetTurnCount();
    results.counterCount = scene.GetCounterCount();
    results.doubleDelete = scene.DoubleDelete();
    results.tripleDelete = scene.TripleDelete();
    results.finalEmotion = player->GetEmotion();
  }

  Audio().StopStream();

  // Only calculate score on successful mission
  if (!(context == Conditions::player_deleted || context == Conditions::player_failed)) {
    results.CalculateScore(results, scene.GetProps().mobs.at(0));
  }

  switch (context) {
  case Conditions::player_deleted:
    SetIntroText(GetScene().GetLocalPlayer()->GetName() + " deleted!");
    break;
  case Conditions::player_won_single_turn:
    SetIntroText("Single Turn Liberation!");
    Audio().Stream("resources/loops/enemy_deleted.ogg");
    break;
  case Conditions::player_won_mutliple_turn:
    SetIntroText("Liberate success!");
    Audio().Stream("resources/loops/enemy_deleted.ogg");
    break;
  case Conditions::player_failed:
    SetIntroText("Liberation Failed!");
    break;
  }

  for (std::shared_ptr<Player> p : GetScene().GetAllPlayers()) {
    auto animComponent = p->GetFirstComponent<AnimationComponent>();

    // If animating, let the animation end to look natural
    if (animComponent) {
      animComponent->OnFinish([p] {
        if (auto action = p->CurrentCardAction()) {
          action->EndAction();
        }

        // NOTE: paranoid cleanup codes ALWAYS cleans up!
        // some attacks use nodes that would be cleanup with End() but overwriting the animation prevents this
        auto ourNodes = p->GetChildNodesWithTag({ Player::BASE_NODE_TAG,Player::FORM_NODE_TAG });
        auto allNodes = p->GetChildNodes();

        for (auto node : allNodes) {
          auto iter = ourNodes.find(node);
          if (iter == ourNodes.end()) {
            p->RemoveNode(node);
          }
        }
        p->ClearActionQueue();
        p->ChangeState<PlayerIdleState>();
        });
    }
    else {
      // Otherwise force state
      p->ChangeState<PlayerIdleState>();
    }
  }

  GetScene().GetField()->RequestBattleStop();
}


void FreedomMissionOverState::onUpdate(double elapsed)
{
  BattleTextIntroState::onUpdate(elapsed);

  // finish whatever animations were happening
  GetScene().GetField()->Update(elapsed);
}