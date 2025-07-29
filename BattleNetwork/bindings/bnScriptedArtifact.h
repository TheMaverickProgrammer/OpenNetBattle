#pragma once
#ifdef BN_MOD_SUPPORT

#include <sol/sol.hpp>
#include "../bnArtifact.h"
#include "dynamic_object.h"
#include "../bnAnimationComponent.h"
#include "bnWeakWrapper.h"

/**
* \class ScriptedArtifact
* \brief An object in control of battlefield visual effects, with support for Lua scripting.
* 
*/
class ScriptedArtifact final : public Artifact, public dynamic_object
{
	std::shared_ptr<AnimationComponent> animationComponent{ nullptr };
	sf::Vector2f scriptedOffset{ };
	WeakWrapper<ScriptedArtifact> weakWrap;

public:
	ScriptedArtifact();
  ~ScriptedArtifact();

  void Init() override;

  /**
   * Centers the animation on the tile, offsets it by its internal offsets, then invokes the function assigned to onUpdate if present.
   * @param _elapsed: The amount of elapsed time since the last frame.
   */
  void OnUpdate(double _elapsed) override;
  void OnDelete() override;
  bool CanMoveTo(Battle::Tile* next) override;
  void OnSpawn(Battle::Tile& spawn) override;
  void OnBattleStart() override;
  void OnBattleStop() override;

  void SetAnimation(const std::string& path);
  Animation& GetAnimationObject();
  Battle::Tile* GetCurrentTile() const;

  sol::object update_func;
  sol::object on_spawn_func;
  sol::object delete_func;
  sol::object can_move_to_func;
  sol::object battle_start_func;
  sol::object battle_end_func;
};

#endif
