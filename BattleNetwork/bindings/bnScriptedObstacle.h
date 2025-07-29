#pragma once
#ifdef BN_MOD_SUPPORT

#include <sol/sol.hpp>
#include "dynamic_object.h"
#include "../bnObstacle.h"
#include "../bnAnimationComponent.h"
#include "bnWeakWrapper.h"

using sf::Texture;

/**
 * @class ScriptedObstacle
*/
class ScriptedObstacle final : public Obstacle, public dynamic_object {
public:
  ScriptedObstacle(Team _team);
  ~ScriptedObstacle();

  void Init() override;
  void OnUpdate(double _elapsed) override;
  void OnDelete() override;
  bool CanMoveTo(Battle::Tile * next) override;
  void OnCollision(const std::shared_ptr<Entity> other) override;
  void Attack(std::shared_ptr<Entity> e) override;
  void OnSpawn(Battle::Tile& spawn) override;
  void OnBattleStart() override;
  void OnBattleStop() override;
  const float GetHeight() const;
  void SetHeight(const float height);

  void SetAnimation(const std::string& path);
  Animation& GetAnimationObject();
  Battle::Tile* GetCurrentTile() const;

  // duration in seconds
  void ShakeCamera(double power, float duration);

  sol::object can_move_to_func;
  sol::object collision_func;
  sol::object update_func;
  sol::object delete_func;
  sol::object attack_func;
  sol::object on_spawn_func;
  sol::object battle_start_func;
  sol::object battle_end_func;
private:
  sf::Vector2f scriptedOffset{};
  float height{};
  std::shared_ptr<AnimationComponent> animComponent{ nullptr };
  std::shared_ptr<DefenseRule> obstacleBody{ nullptr };
  WeakWrapper<ScriptedObstacle> weakWrap;
};
#endif