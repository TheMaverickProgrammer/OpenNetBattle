#ifdef BN_MOD_SUPPORT
#include "bnUserTypeScriptedObstacle.h"

#include "bnWeakWrapper.h"
#include "bnUserTypeEntity.h"
#include "bnScriptedObstacle.h"

void DefineScriptedObstacleUserType(sol::state& state, sol::table& battle_namespace) {
  auto from = [state = &state] (std::shared_ptr<Entity> entity) {
    if (auto obstacle = std::dynamic_pointer_cast<Obstacle>(entity)) {
      return sol::make_object(*state, WeakWrapper(obstacle));
    }

    return sol::make_object(*state, sol::lua_nil);
  };

  auto scriptedobstacle_record = battle_namespace.new_usertype<WeakWrapper<ScriptedObstacle>>("Obstacle",
    sol::factories([](Team team) -> WeakWrapper<ScriptedObstacle> {
      auto obstacle = std::make_shared<ScriptedObstacle>(team);
      obstacle->Init();

      auto wrappedObstacle = WeakWrapper<ScriptedObstacle>(obstacle);
      wrappedObstacle.Own();
      return wrappedObstacle;
    }),
    "from", sol::overload(
      [](WeakWrapper<ScriptedObstacle>& e) -> WeakWrapper<ScriptedObstacle> {
        return e;
      },
      [from](WeakWrapper<Character>& e) -> sol::object {
        return from(e.Unwrap());
      },
      [from](WeakWrapper<Entity>& e) -> sol::object {
        return from(e.Unwrap());
      },
      [state=&state]() -> sol::object {
        return sol::make_object(*state, sol::lua_nil);
      }
    ),
    sol::meta_function::index, [](WeakWrapper<ScriptedObstacle>& obstacle, const std::string& key) {
      return obstacle.Unwrap()->dynamic_get(key);
    },
    sol::meta_function::new_index, [](WeakWrapper<ScriptedObstacle>& obstacle, const std::string& key, sol::stack_object value) {
      obstacle.Unwrap()->dynamic_set(key, value);
    },
    sol::meta_function::length, [](WeakWrapper<ScriptedObstacle>& obstacle) {
      return obstacle.Unwrap()->entries.size();
    },
    "can_move_to_func", sol::property(
      [](WeakWrapper<ScriptedObstacle>& obstacle) { return obstacle.Unwrap()->can_move_to_func; },
      [](WeakWrapper<ScriptedObstacle>& obstacle, sol::stack_object value) {
        obstacle.Unwrap()->can_move_to_func = VerifyLuaCallback(value);
      }
    ),
    "collision_func", sol::property(
      [](WeakWrapper<ScriptedObstacle>& obstacle) { return obstacle.Unwrap()->collision_func; },
      [](WeakWrapper<ScriptedObstacle>& obstacle, sol::stack_object value) {
        obstacle.Unwrap()->collision_func = VerifyLuaCallback(value);
      }
    ),
    "update_func", sol::property(
      [](WeakWrapper<ScriptedObstacle>& obstacle) { return obstacle.Unwrap()->update_func; },
      [](WeakWrapper<ScriptedObstacle>& obstacle, sol::stack_object value) {
        obstacle.Unwrap()->update_func = VerifyLuaCallback(value);
      }
    ),
    "delete_func", sol::property(
      [](WeakWrapper<ScriptedObstacle>& obstacle) { return obstacle.Unwrap()->delete_func; },
      [](WeakWrapper<ScriptedObstacle>& obstacle, sol::stack_object value) {
        obstacle.Unwrap()->delete_func = VerifyLuaCallback(value);
      }
    ),
    "attack_func", sol::property(
      [](WeakWrapper<ScriptedObstacle>& obstacle) { return obstacle.Unwrap()->attack_func; },
      [](WeakWrapper<ScriptedObstacle>& obstacle, sol::stack_object value) {
        obstacle.Unwrap()->attack_func = VerifyLuaCallback(value);
      }
    ),
    "on_spawn_func", sol::property(
      [](WeakWrapper<ScriptedObstacle>& obstacle) { return obstacle.Unwrap()->on_spawn_func; },
      [](WeakWrapper<ScriptedObstacle>& obstacle, sol::stack_object value) {
        obstacle.Unwrap()->on_spawn_func = VerifyLuaCallback(value);
      }
    ),
    "battle_start_func", sol::property(
      [](WeakWrapper<ScriptedObstacle>& obstacle) { return obstacle.Unwrap()->battle_start_func; },
      [](WeakWrapper<ScriptedObstacle>& obstacle, sol::stack_object value) {
        obstacle.Unwrap()->battle_start_func = VerifyLuaCallback(value);
      }
    ),
    "battle_end_func", sol::property(
      [](WeakWrapper<ScriptedObstacle>& obstacle) { return obstacle.Unwrap()->battle_end_func; },
      [](WeakWrapper<ScriptedObstacle>& obstacle, sol::stack_object value) {
        obstacle.Unwrap()->battle_end_func = VerifyLuaCallback(value);
      }
    )
  );

  DefineEntityFunctionsOn(scriptedobstacle_record);

  scriptedobstacle_record["set_animation"] = [](WeakWrapper<ScriptedObstacle>& obstacle, std::string animation) {
    obstacle.Unwrap()->SetAnimation(animation);
  };
  scriptedobstacle_record["get_animation"] = [](WeakWrapper<ScriptedObstacle>& obstacle) -> AnimationWrapper {
    auto& animation = obstacle.Unwrap()->GetAnimationObject();
    return AnimationWrapper(obstacle.GetWeak(), animation);
  };

  auto obstacle_record = battle_namespace.new_usertype<WeakWrapper<Obstacle>>("BasicObstacle");

  DefineEntityFunctionsOn(obstacle_record);
}
#endif
