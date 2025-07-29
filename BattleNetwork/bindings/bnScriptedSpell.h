#pragma once
#ifdef BN_MOD_SUPPORT

#include <sol/sol.hpp>
#include "dynamic_object.h"
#include "../bnSpell.h"
#include "../bnAnimationComponent.h"
#include "bnWeakWrapper.h"

using sf::Texture;

/**
 * @class ScriptedSpell
*/
class ScriptedSpell final : public Spell, public dynamic_object {
public:
  ScriptedSpell(Team _team);
  ~ScriptedSpell();
  
  void Init() override;
  void OnUpdate(double _elapsed) override;
  void OnDelete() override;
  void OnCollision(const std::shared_ptr<Entity> other) override;
  bool CanMoveTo(Battle::Tile * next) override;
  void Attack(std::shared_ptr<Entity> e) override;
  void OnSpawn(Battle::Tile& spawn) override;
  void OnBattleStart() override;
  void OnBattleStop() override;
  const float GetHeight() const;
  void SetHeight(const float height);

  void SetAnimation(const std::string& path);
  Animation& GetAnimationObject();

  sol::object can_move_to_func;
  sol::object update_func;
  sol::object delete_func;
  sol::object collision_func;
  sol::object attack_func;
  sol::object on_spawn_func;
  sol::object battle_start_func;
  sol::object battle_end_func;
private:
  float height{};
  sf::Vector2f scriptedOffset{};
  std::shared_ptr<AnimationComponent> animComponent{ nullptr };
  WeakWrapper<ScriptedSpell> weakWrap;
};
#endif