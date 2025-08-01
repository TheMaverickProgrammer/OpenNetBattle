#ifdef BN_MOD_SUPPORT
#include "bnUserTypeScriptedPlayer.h"

#include "bnWeakWrapper.h"
#include "bnUserTypeEntity.h"
#include "bnScopedWrapper.h"
#include "bnScriptedPlayer.h"
#include "bnScriptedPlayerForm.h"
#include "bnScriptedCardAction.h"

void DefineScriptedPlayerUserType(sol::state& state, sol::table& battle_namespace) {
  auto from = [state = &state] (std::shared_ptr<Entity> entity) {
    if (auto player = std::dynamic_pointer_cast<Player>(entity)) {
      return sol::make_object(*state, WeakWrapper(player));
    }

    return sol::make_object(*state, sol::lua_nil);
  };

  auto player_table = battle_namespace.new_usertype<WeakWrapper<ScriptedPlayer>>("Player",
    sol::meta_function::index, [](WeakWrapper<ScriptedPlayer>& player, const std::string& key) {
      return player.Unwrap()->dynamic_get(key);
    },
    sol::meta_function::new_index, [](WeakWrapper<ScriptedPlayer>& player, const std::string& key, sol::stack_object value) {
      player.Unwrap()->dynamic_set(key, value);
    },
    sol::meta_function::length, [](WeakWrapper<ScriptedPlayer>& player) {
      return player.Unwrap()->entries.size();
    },
    "from", sol::overload(
      [](WeakWrapper<ScriptedPlayer>& player) -> WeakWrapper<ScriptedPlayer> {
        return player;
      },
      [state=&state](WeakWrapper<Player>& player) -> sol::object {
        if (auto scriptedPlayer = std::dynamic_pointer_cast<ScriptedPlayer>(player.Unwrap())) {
          return sol::make_object(*state, WeakWrapper(scriptedPlayer));
        }
        return sol::make_object(*state, player);
      },
      [from](WeakWrapper<Entity>& e) -> sol::object {
        return from(e.Unwrap());
      },
      [from](WeakWrapper<Character>& e) -> sol::object {
        return from(e.Unwrap());
      },
      [state=&state]() -> sol::object {
        return sol::make_object(*state, sol::lua_nil);
      }
    ),
    "card_action_event", sol::overload(
      [](WeakWrapper<ScriptedPlayer>& player, WeakWrapper<ScriptedCardAction>& cardAction, ActionOrder order) {
        player.Unwrap()->AddAction(CardEvent{ cardAction.UnwrapAndRelease() }, order);
      },
      [](WeakWrapper<ScriptedPlayer>& player, WeakWrapper<CardAction>& cardAction, ActionOrder order) {
        player.Unwrap()->AddAction(CardEvent{ cardAction.UnwrapAndRelease() }, order);
      }
    ),
    "can_attack", [](WeakWrapper<ScriptedPlayer>& player) {
      auto playerPtr = player.Unwrap();
      return playerPtr->CanAttack();
    },
    "get_attack_level", [](WeakWrapper<ScriptedPlayer>& player) -> unsigned int {
      return player.Unwrap()->GetAttackLevel();
    },
    "set_attack_level", [](WeakWrapper<ScriptedPlayer>& player, unsigned int level) {
      return player.Unwrap()->SetAttackLevel(level);
    },
    "get_charge_level", [](WeakWrapper<ScriptedPlayer>& player) -> unsigned int {
      return player.Unwrap()->GetChargeLevel();
    },
    "set_charge_level", [](WeakWrapper<ScriptedPlayer>& player, unsigned int level) {
      return player.Unwrap()->SetChargeLevel(level);
    },
    "mod_max_health", [](WeakWrapper<ScriptedPlayer>& player, int mod) -> void {
      player.Unwrap()->ModMaxHealth(mod);
    },
    "get_max_health_mod", [](WeakWrapper<ScriptedPlayer>& player) -> int {
      return player.Unwrap()->GetMaxHealthMod();
    },
    "set_fully_charged_color", [](WeakWrapper<ScriptedPlayer>& player, sf::Color color) {
      player.Unwrap()->SetFullyChargeColor(color);
    },
    "set_charge_position", [](WeakWrapper<ScriptedPlayer>& player, float x, float y) {
      player.Unwrap()->SetChargePosition(x, y);
    },
    "slide_when_moving", [](WeakWrapper<ScriptedPlayer>& player, bool enable, const frame_time_t& frames) {
      player.Unwrap()->SlideWhenMoving(enable, frames);
    },
    "create_form", [](WeakWrapper<ScriptedPlayer>& player) -> WeakWrapperChild<Player, ScriptedPlayerFormMeta> {
      auto parentPtr = player.Unwrap();
      auto formMeta = parentPtr->CreateForm();

      if (!formMeta) {
        throw std::runtime_error("Failed to create form");
      }

      return WeakWrapperChild<Player, ScriptedPlayerFormMeta>(parentPtr, *formMeta);
    },
    "create_sync_node", [] (WeakWrapper<ScriptedPlayer>& player, const std::string& point) -> std::shared_ptr<SyncNode> {
      return player.Unwrap()->AddSyncNode(point);
    },
    "remove_sync_node", [] (WeakWrapper<ScriptedPlayer>& player, const std::shared_ptr<SyncNode>& node) {
      player.Unwrap()->RemoveSyncNode(node);
    },
    "update_func", sol::property(
      [](WeakWrapper<ScriptedPlayer>& player) { return player.Unwrap()->update_func; },
      [](WeakWrapper<ScriptedPlayer>& player, sol::stack_object value) {
        player.Unwrap()->update_func = VerifyLuaCallback(value);
      }
    ),
    "battle_start_func", sol::property(
      [](WeakWrapper<ScriptedPlayer>& player) { return player.Unwrap()->battle_start_func; },
      [](WeakWrapper<ScriptedPlayer>& player, sol::stack_object value) {
        player.Unwrap()->battle_start_func = VerifyLuaCallback(value);
      }
    ),
    "battle_end_func", sol::property(
      [](WeakWrapper<ScriptedPlayer>& player) { return player.Unwrap()->battle_end_func; },
      [](WeakWrapper<ScriptedPlayer>& player, sol::stack_object value) {
        player.Unwrap()->battle_end_func = VerifyLuaCallback(value);
      }
    ),
    "normal_attack_func", sol::property(
      [](WeakWrapper<ScriptedPlayer>& player) { return player.Unwrap()->normal_attack_func; },
      [](WeakWrapper<ScriptedPlayer>& player, sol::stack_object value) {
        player.Unwrap()->normal_attack_func = VerifyLuaCallback(value);
      }
    ),
    "charged_attack_func", sol::property(
      [](WeakWrapper<ScriptedPlayer>& player) { return player.Unwrap()->charged_attack_func; },
      [](WeakWrapper<ScriptedPlayer>& player, sol::stack_object value) {
        player.Unwrap()->charged_attack_func = VerifyLuaCallback(value);
      }
    ),
    "charged_time_table_func", sol::property(
      [](WeakWrapper<ScriptedPlayer>& player) { return player.Unwrap()->charge_time_table_func; },
      [](WeakWrapper<ScriptedPlayer>& player, sol::stack_object value) {
        player.Unwrap()->charge_time_table_func = VerifyLuaCallback(value);
      }
    ),
    "special_attack_func", sol::property(
      [](WeakWrapper<ScriptedPlayer>& player) { return player.Unwrap()->special_attack_func; },
      [](WeakWrapper<ScriptedPlayer>& player, sol::stack_object value) {
        player.Unwrap()->special_attack_func = VerifyLuaCallback(value);
      }
    )
  );

  DefineEntityFunctionsOn(player_table);
  player_table["set_animation"] = [](WeakWrapper<ScriptedPlayer>& player, std::string animation) {
    player.Unwrap()->SetAnimation(animation);
  };
  player_table["get_animation"] = [](WeakWrapper<ScriptedPlayer>& player) -> AnimationWrapper {
    auto& animation = player.Unwrap()->GetAnimationObject();
    return AnimationWrapper(player.GetWeak(), animation);
  };

  battle_namespace.new_usertype<WeakWrapperChild<Player, ScriptedPlayerFormMeta>>("PlayerFormMeta",
    "set_mugshot_texture_path", [] (WeakWrapperChild<Player, ScriptedPlayerFormMeta>& form, const std::string& path) {
      form.Unwrap().SetUIPath(path);
    },
    "calculate_charge_time_func", sol::property(
      [](WeakWrapperChild<Player, ScriptedPlayerFormMeta>& form) { return form.Unwrap().calculate_charge_time_func; },
      [](WeakWrapperChild<Player, ScriptedPlayerFormMeta>& form, sol::stack_object value) {
        form.Unwrap().calculate_charge_time_func = VerifyLuaCallback(value);
      }
    ),
    "on_activate_func", sol::property(
      [](WeakWrapperChild<Player, ScriptedPlayerFormMeta>& form) { return form.Unwrap().on_activate_func; },
      [](WeakWrapperChild<Player, ScriptedPlayerFormMeta>& form, sol::stack_object value) {
        form.Unwrap().on_activate_func = VerifyLuaCallback(value);
      }
    ),
    "on_deactivate_func", sol::property(
      [](WeakWrapperChild<Player, ScriptedPlayerFormMeta>& form) { return form.Unwrap().on_deactivate_func; },
      [](WeakWrapperChild<Player, ScriptedPlayerFormMeta>& form, sol::stack_object value) {
        form.Unwrap().on_deactivate_func = VerifyLuaCallback(value);
      }
    ),
    "update_func", sol::property(
      [](WeakWrapperChild<Player, ScriptedPlayerFormMeta>& form) { return form.Unwrap().update_func; },
      [](WeakWrapperChild<Player, ScriptedPlayerFormMeta>& form, sol::stack_object value) {
        form.Unwrap().update_func = VerifyLuaCallback(value);
      }
    ),
    "charged_attack_func", sol::property(
      [](WeakWrapperChild<Player, ScriptedPlayerFormMeta>& form) { return form.Unwrap().charged_attack_func; },
      [](WeakWrapperChild<Player, ScriptedPlayerFormMeta>& form, sol::stack_object value) {
        form.Unwrap().charged_attack_func = VerifyLuaCallback(value);
      }
    ),
    "special_attack_func", sol::property(
      [](WeakWrapperChild<Player, ScriptedPlayerFormMeta>& form) { return form.Unwrap().special_attack_func; },
      [](WeakWrapperChild<Player, ScriptedPlayerFormMeta>& form, sol::stack_object value) {
        form.Unwrap().special_attack_func = VerifyLuaCallback(value);
      }
    )
  );

  battle_namespace.new_usertype<ScopedWrapper<ScriptedPlayerForm>>("PlayerForm",
    sol::meta_function::index, [](ScopedWrapper<ScriptedPlayerForm>& form, const std::string& key) {
      return form.Unwrap().dynamic_get(key);
    },
    sol::meta_function::new_index, [](ScopedWrapper<ScriptedPlayerForm>& form, const std::string& key, sol::stack_object value) {
      form.Unwrap().dynamic_set(key, value);
    },
    sol::meta_function::length, [](ScopedWrapper<ScriptedPlayerForm>& form) {
      return form.Unwrap().entries.size();
    }
  );
}
#endif
