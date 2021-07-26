#pragma once
#ifdef BN_MOD_SUPPORT

#include <sol/sol.hpp>
#include "dynamic_object.h"
#include "../bnSpell.h"
#include "../bnAnimationComponent.h"

using sf::Texture;

/**
 * @class ScriptedSpell
*/
class ScriptedSpell final : public Spell, public dynamic_object {
public:
  ScriptedSpell(Team _team);
  ~ScriptedSpell();
  
  void OnUpdate(double _elapsed) override;
  void OnDelete() override;
  void OnCollision(const Character* other) override;
  bool CanMoveTo(Battle::Tile * next) override;
  void Attack(Character* e) override;
  void OnSpawn(Battle::Tile& spawn) override;
  const float GetHeight() const;
  void SetHeight(const float height);
  void ShowShadow(const bool shadow);

  Animation& GetAnimationObject();
  const sf::Vector2f& GetTileOffset() const;
  void SetTileOffset(float x, float y);
  // duration in seconds
  void ShakeCamera(double power, float duration);

  std::function<void(ScriptedSpell&, Battle::Tile&)> spawnCallback;
  std::function<void(ScriptedSpell&, Character&)> attackCallback;
  std::function<void(ScriptedSpell&, Character&)> collisionCallback;
  std::function<bool(Battle::Tile&)> canMoveToCallback;
  std::function<void(ScriptedSpell&)> deleteCallback;
  std::function<void(ScriptedSpell&, double)> updateCallback;
private:
  float height{};
  sf::Vector2f scriptedOffset{};
  SpriteProxyNode* shadow{ nullptr };
  AnimationComponent* animComponent{ nullptr };
};
#endif