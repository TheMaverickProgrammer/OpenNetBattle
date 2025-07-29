#ifdef BN_MOD_SUPPORT
#include "bnScriptedObstacle.h"
#include "../bnDefenseObstacleBody.h"
#include "../bnSolHelpers.h"

ScriptedObstacle::ScriptedObstacle(Team _team) :
  Obstacle(_team) {
  setScale(2.f, 2.f);
}

void ScriptedObstacle::Init() {
  Obstacle::Init();

  animComponent = CreateComponent<AnimationComponent>(weak_from_this());
  animComponent->Load();
  animComponent->Refresh();

  obstacleBody = std::make_shared<DefenseObstacleBody>();
  this->AddDefenseRule(obstacleBody);

  weakWrap = WeakWrapper(weak_from_base<ScriptedObstacle>());
}

ScriptedObstacle::~ScriptedObstacle() {
}

bool ScriptedObstacle::CanMoveTo(Battle::Tile * next)
{
  if (can_move_to_func.valid()) 
  {
    auto result = CallLuaCallbackExpectingValue<bool>(can_move_to_func, next);

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
    }

    return result.value();
  }

  return false;
}

void ScriptedObstacle::OnCollision(const std::shared_ptr<Entity> other)
{
  if (collision_func.valid()) 
  {
    auto result = CallLuaCallback(collision_func, weakWrap, WeakWrapper(other));

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
    }
  }
}

void ScriptedObstacle::OnUpdate(double _elapsed) {
  if (update_func.valid()) 
  {
    auto result = CallLuaCallback(update_func, weakWrap, _elapsed);

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
    }
  }
}

void ScriptedObstacle::OnDelete() {
  if (delete_func.valid()) 
  {
    auto result = CallLuaCallback(delete_func, weakWrap);

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
    }
  }

  Erase();
}

void ScriptedObstacle::Attack(std::shared_ptr<Entity> other) {
  other->Hit(GetHitboxProperties());

  if (attack_func.valid()) 
  {
    auto result = CallLuaCallback(attack_func, weakWrap, WeakWrapper(other));

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
    }
  }
}

void ScriptedObstacle::OnSpawn(Battle::Tile& spawn)
{
  if (on_spawn_func.valid()) 
  {
    auto result = CallLuaCallback(on_spawn_func, weakWrap);

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
    }
  }
}

void ScriptedObstacle::OnBattleStart() {
  if (battle_start_func.valid())
  {
    auto result = CallLuaCallback(battle_start_func, weakWrap);

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
    }
  }

  Obstacle::OnBattleStart();
}

void ScriptedObstacle::OnBattleStop() {
  Obstacle::OnBattleStop();

  if (battle_end_func.valid())
  {
    auto result = CallLuaCallback(battle_end_func, weakWrap);

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
    }
  }
}

const float ScriptedObstacle::GetHeight() const
{
  return height;
}

void ScriptedObstacle::SetHeight(const float height)
{
  this->height = height;
}

void ScriptedObstacle::SetAnimation(const std::string& path)
{
  animComponent->SetPath(path);
  animComponent->Load();
}

Animation& ScriptedObstacle::GetAnimationObject()
{
  return animComponent->GetAnimationObject();
}

Battle::Tile* ScriptedObstacle::GetCurrentTile() const
{
  return GetTile();
}

void ScriptedObstacle::ShakeCamera(double power, float duration)
{
}
#endif