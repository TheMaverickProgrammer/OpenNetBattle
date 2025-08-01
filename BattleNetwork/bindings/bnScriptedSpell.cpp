#ifdef BN_MOD_SUPPORT
#include "bnScriptedSpell.h"
#include "../bnSolHelpers.h"
#include "../bnTile.h"

ScriptedSpell::ScriptedSpell(Team team) : Spell(team) {
  setScale(2.f, 2.f);
}

void ScriptedSpell::Init() {
  Spell::Init();
  auto sharedPtr = shared_from_base<ScriptedSpell>();
  animComponent = CreateComponent<AnimationComponent>(sharedPtr);
  weakWrap = WeakWrapper(sharedPtr);
}

ScriptedSpell::~ScriptedSpell() {
}

bool ScriptedSpell::CanMoveTo(Battle::Tile * next)
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

void ScriptedSpell::OnUpdate(double _elapsed) {
  if (update_func.valid()) {
    auto result = CallLuaCallback(update_func, weakWrap, _elapsed);

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
    }
  }
}

void ScriptedSpell::OnDelete() {
  if (delete_func.valid()) 
  {
    auto result = CallLuaCallback(delete_func, weakWrap);

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
    }
  }

  Erase();
}

void ScriptedSpell::OnCollision(const std::shared_ptr<Entity> other)
{
  if (collision_func.valid()) 
  {
    auto result = CallLuaCallback(collision_func, weakWrap, WeakWrapper(other));

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
    }
  }
}

void ScriptedSpell::Attack(std::shared_ptr<Entity> other) {
  other->Hit(GetHitboxProperties());

  if (attack_func.valid()) 
  {
    auto result = CallLuaCallback(attack_func, weakWrap, WeakWrapper(other));

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
    }
  }
}

void ScriptedSpell::OnSpawn(Battle::Tile& spawn)
{
  if (on_spawn_func.valid()) 
  {
    auto result = CallLuaCallback(on_spawn_func, weakWrap);

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
    }
  }
}

void ScriptedSpell::OnBattleStart() {
  if (battle_start_func.valid())
  {
    auto result = CallLuaCallback(battle_start_func, weakWrap);

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
    }
  }

  Spell::OnBattleStart();
}

void ScriptedSpell::OnBattleStop() {
  Spell::OnBattleStop();

  if (battle_end_func.valid())
  {
    auto result = CallLuaCallback(battle_end_func, weakWrap);

    if (result.is_error()) {
      Logger::Log(LogLevel::critical, result.error_cstr());
    }
  }
}

const float ScriptedSpell::GetHeight() const
{
  return height;
}

void ScriptedSpell::SetHeight(const float height)
{
  this->height = height;
  Entity::drawOffset.y = -this->height;
}

void ScriptedSpell::SetAnimation(const std::string& path)
{
  animComponent->SetPath(path);
  animComponent->Load();
}

Animation& ScriptedSpell::GetAnimationObject()
{
  return animComponent->GetAnimationObject();
}

#endif