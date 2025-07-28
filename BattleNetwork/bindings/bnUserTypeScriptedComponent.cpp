#ifdef BN_MOD_SUPPORT
#include "bnUserTypeScriptedComponent.h"

#include "bnWeakWrapper.h"
#include "bnScriptedComponent.h"
#include "bnScriptedObstacle.h"
#include "bnScriptedCharacter.h"
#include "bnScriptedPlayer.h"
#include "bnScriptedArtifact.h"
#include "../bnSolHelpers.h"

static WeakWrapper<ScriptedComponent> construct(std::shared_ptr<Entity> entity, Component::lifetimes lifetime) {
  auto component = std::make_shared<ScriptedComponent>(entity, lifetime);
  component->Init();

  auto wrappedComponent = WeakWrapper(component);
  wrappedComponent.Own();
  return wrappedComponent;
}

void DefineScriptedComponentUserType(sol::state& state, sol::table& battle_namespace) {
  battle_namespace.new_usertype<WeakWrapper<ScriptedComponent>>("Component",
    sol::factories(
      [](WeakWrapper<Entity>& owner, Component::lifetimes lifetime) -> WeakWrapper<ScriptedComponent> {
        return construct(owner.Unwrap(), lifetime);
      },
      [](WeakWrapper<Obstacle>& owner, Component::lifetimes lifetime) -> WeakWrapper<ScriptedComponent> {
        return construct(owner.Unwrap(), lifetime);
      },
      [](WeakWrapper<Character>& owner, Component::lifetimes lifetime) -> WeakWrapper<ScriptedComponent> {
        return construct(owner.Unwrap(), lifetime);
      },
      [](WeakWrapper<Player>& owner, Component::lifetimes lifetime) -> WeakWrapper<ScriptedComponent> {
        return construct(owner.Unwrap(), lifetime);
      },
      [](WeakWrapper<ScriptedObstacle>& owner, Component::lifetimes lifetime) -> WeakWrapper<ScriptedComponent> {
        return construct(owner.Unwrap(), lifetime);
      },
      [](WeakWrapper<ScriptedCharacter>& owner, Component::lifetimes lifetime) -> WeakWrapper<ScriptedComponent> {
        return construct(owner.Unwrap(), lifetime);
      },
      [](WeakWrapper<ScriptedPlayer>& owner, Component::lifetimes lifetime) -> WeakWrapper<ScriptedComponent> {
        return construct(owner.Unwrap(), lifetime);
      },
      [](WeakWrapper<ScriptedArtifact>& owner, Component::lifetimes lifetime) -> WeakWrapper<ScriptedComponent> {
        return construct(owner.Unwrap(), lifetime);
      }
    ),
    sol::meta_function::index, [](WeakWrapper<ScriptedComponent>& component, const std::string& key) {
      return component.Unwrap()->dynamic_get(key);
    },
    sol::meta_function::new_index, [](WeakWrapper<ScriptedComponent>& component, const std::string& key, sol::stack_object value) {
      component.Unwrap()->dynamic_set(key, value);
    },
    sol::meta_function::length, [](WeakWrapper<ScriptedComponent>& component) {
      return component.Unwrap()->entries.size();
    },
    "eject", [](WeakWrapper<ScriptedComponent>& component) {
      component.Unwrap()->Eject();
    },
    "get_id", [](WeakWrapper<ScriptedComponent>& component) -> Component::ID_t {
      return component.Unwrap()->GetID();
    },
    "is_injected", [](WeakWrapper<ScriptedComponent>& component) -> bool{
      return component.Unwrap()->Injected();
    },
    "get_owner", [](WeakWrapper<ScriptedComponent>& component) -> WeakWrapper<Entity> {
      return WeakWrapper(component.Unwrap()->GetOwner());
    },
    "update_func", sol::property(
      [](WeakWrapper<ScriptedComponent>& component) { return component.Unwrap()->update_func; },
      [](WeakWrapper<ScriptedComponent>& component, sol::stack_object value) {
        component.Unwrap()->update_func = VerifyLuaCallback(value);
      }
    ),
    "scene_inject_func", sol::property(
      [](WeakWrapper<ScriptedComponent>& component) { return component.Unwrap()->scene_inject_func; },
      [](WeakWrapper<ScriptedComponent>& component, sol::stack_object value) {
        component.Unwrap()->scene_inject_func = VerifyLuaCallback(value);
      }
    )
  );

  state.new_enum("Lifetimes",
    "Local", Component::lifetimes::local,
    "Battlestep", Component::lifetimes::battlestep,
    "Scene", Component::lifetimes::ui
  );
}
#endif
