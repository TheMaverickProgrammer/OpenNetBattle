#ifdef BN_MOD_SUPPORT
#include "bnUserTypeBasicPlayer.h"

#include "bnWeakWrapper.h"
#include "bnUserTypeEntity.h"
#include "bnScriptedPlayer.h"
#include "bnScriptedPlayerForm.h"
#include "bnScriptedCardAction.h"

void DefineBasicPlayerUserType(sol::table& battle_namespace) {
  auto player_table = battle_namespace.new_usertype<WeakWrapper<Player>>("BasicPlayer",
    "card_action_event", sol::overload(
      [](WeakWrapper<Player>& player, WeakWrapper<ScriptedCardAction>& cardAction, ActionOrder order) {
        player.Unwrap()->AddAction(CardEvent{ cardAction.UnwrapAndRelease() }, order);
      },
      [](WeakWrapper<Player>& player, WeakWrapper<CardAction>& cardAction, ActionOrder order) {
        player.Unwrap()->AddAction(CardEvent{ cardAction.UnwrapAndRelease() }, order);
      }
    ),
    "can_attack", [](WeakWrapper<Player>& player) {
      auto playerPtr = player.Unwrap();
      return playerPtr->CanAttack();
     },
    "get_attack_level", [](WeakWrapper<Player>& player) -> unsigned int {
      return player.Unwrap()->GetAttackLevel();
    },
    "set_attack_level", [](WeakWrapper<Player>& player, unsigned int level) {
      return player.Unwrap()->SetAttackLevel(level);
    },
    "get_charge_level", [](WeakWrapper<Player>& player) -> unsigned int {
      return player.Unwrap()->GetChargeLevel();
    },
    "set_charge_level", [](WeakWrapper<Player>& player, unsigned int level) {
      return player.Unwrap()->SetChargeLevel(level);
    },
    "mod_max_health", [](WeakWrapper<Player>& player, int mod) -> void {
      player.Unwrap()->ModMaxHealth(mod);
    },
    "get_max_health_mod", [](WeakWrapper<Player>& player) -> int {
      return player.Unwrap()->GetMaxHealthMod();
    },
    "set_fully_charged_color", [](WeakWrapper<Player>& player, sf::Color color) {
      player.Unwrap()->GetChargeComponent().SetFullyChargedColor(color);
    },
    "set_charge_position", [](WeakWrapper<Player>& player, float x, float y) {
      player.Unwrap()->GetChargeComponent().setPosition(x, y);
    },
    "slide_when_moving", [](WeakWrapper<Player>& player, bool enable, const frame_time_t& frames) {
      player.Unwrap()->SlideWhenMoving(enable, frames);
    },
    "create_sync_node", [] (WeakWrapper<Player>& player, const std::string& point) -> std::shared_ptr<SyncNode> {
      return player.Unwrap()->AddSyncNode(point);
    },
    "remove_sync_node", [] (WeakWrapper<Player>& player, const std::shared_ptr<SyncNode>& node) {
      player.Unwrap()->RemoveSyncNode(node);
    }
  );

  DefineEntityFunctionsOn(player_table);
  player_table["set_animation"] = [](WeakWrapper<Player>& player, std::string animation) {
    player.Unwrap()->SetAnimation(animation);
  };
  player_table["get_animation"] = [](WeakWrapper<Player>& player) -> AnimationWrapper {
    auto& animation = player.Unwrap()->GetComponents<AnimationComponent>()[0]->GetAnimationObject();
    return AnimationWrapper(player.GetWeak(), animation);
  
  };
}
#endif
