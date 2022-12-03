#include "bnFadeOutBattleState.h"
#include "../bnBattleSceneBase.h"

#include "../../bnEntity.h"
#include "../../bnPlayer.h"
#include "../../bnField.h"

FadeOutBattleState::FadeOutBattleState(const FadeOut& mode) : mode(mode) {}

void FadeOutBattleState::onStart(const BattleSceneState*) {
  BattleSceneBase& scene = GetScene();
  std::shared_ptr<Player> localPlayer = scene.GetLocalPlayer();
  Team localTeam = localPlayer->GetTeam();

  for (std::shared_ptr<Player> p : scene.GetAllPlayers()) {
    p->ChangeState<PlayerIdleState>();
  }

  std::shared_ptr<Field> field = scene.GetField();
  field->RequestBattleStop();

  std::vector<std::reference_wrapper<const Character>> mobList;
  mobList = localTeam == Team::red ? scene.BlueTeamMobList() : scene.RedTeamMobList();

  BattleResults& results = scene.BattleResultsObj();
  results.turns = scene.GetTurnCount();

  if (mobList.empty())
    return;

  std::sort(mobList.begin(), mobList.end(), [](auto& a, auto& b) { return a.get().GetID() < b.get().GetID(); });

  Entity::ID_t next_id{ mobList.front().get().GetID()};

  for (const Character& enemy : mobList) {
    BattleResults::MobData mob;
    mob.health = enemy.GetHealth();
    mob.id = enemy.GetName();

    // track deleted enemies from this mob by returning empty values where they would be
    while (enemy.GetID() != next_id) {
      results.mobStatus.push_back(BattleResults::MobData{});
      next_id++;
    }

    results.mobStatus.push_back(mob);
    next_id++;
  }
}

void FadeOutBattleState::onEnd(const BattleSceneState*)
{
}

void FadeOutBattleState::onUpdate(double elapsed)
{
  BattleSceneBase& scene = GetScene();
  wait -= elapsed;

  if (wait <= 0.0) {
    scene.Quit(mode);
  }
  else if(keepPlaying) {
    scene.GetField()->Update(elapsed);
  }
}

void FadeOutBattleState::onDraw(IRenderer& renderer)
{
}

void FadeOutBattleState::EnableKeepPlaying(bool enable)
{
  this->keepPlaying = enable;
}
