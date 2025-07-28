#ifdef BN_MOD_SUPPORT

#include <optional>
#include "bnUserTypeHitbox.h"
#include "bnUserTypeEntity.h"
#include "bnScriptedArtifact.h"
#include "bnScriptedCharacter.h"
#include "bnScriptedSpell.h"
#include "bnScriptedObstacle.h"
#include "bnScriptedPlayer.h"
#include "../bnHitboxSpell.h"
#include "../bnSharedHitbox.h"

void DefineHitboxUserTypes(sol::state& state, sol::table& battle_namespace) {
  auto hitbox_table = battle_namespace.new_usertype<WeakWrapper<HitboxSpell>>("Hitbox",
    sol::factories([] (Team team) -> WeakWrapper<HitboxSpell> {
      auto spell = std::make_shared<HitboxSpell>(team);
      auto wrappedSpell = WeakWrapper(spell);
      wrappedSpell.Own();
      return wrappedSpell;
    }),
    sol::meta_function::index, []( sol::table table, const std::string key ) { 
      ScriptResourceManager::PrintInvalidAccessMessage( table, "Hitbox", key );
    },
    sol::meta_function::new_index, []( sol::table table, const std::string key, sol::object obj ) { 
      ScriptResourceManager::PrintInvalidAssignMessage( table, "Hitbox", key );
    },
    "set_callbacks", [](WeakWrapper<HitboxSpell>& spell, sol::object luaAttackCallbackObject, sol::object luaCollisionCallbackObject) {
      ExpectLuaFunction(luaAttackCallbackObject);
      ExpectLuaFunction(luaCollisionCallbackObject);

      auto attackCallback = [luaAttackCallbackObject] (std::shared_ptr<Entity> e) {
        sol::protected_function luaAttackCallback = luaAttackCallbackObject;
        auto result = luaAttackCallback(WeakWrapper(e));

        if (!result.valid()) {
          sol::error error = result;
          Logger::Log(LogLevel::critical, error.what());
        }
      };

      auto collisionCallback = [luaCollisionCallbackObject] (const std::shared_ptr<Entity> e) {
        sol::protected_function luaCollisionCallback = luaCollisionCallbackObject;
        auto result = luaCollisionCallback(WeakWrapper(e));

        if (!result.valid()) {
          sol::error error = result;
          Logger::Log(LogLevel::critical, error.what());
        }
      };

      spell.Unwrap()->AddCallback(attackCallback, collisionCallback);
    }
  );
  DefineEntityFunctionsOn(hitbox_table);

  battle_namespace.new_usertype<SharedHitbox>("SharedHitbox",
    sol::factories(
      [] (WeakWrapper<Entity>& e, float f) -> WeakWrapper<Entity> {
        std::shared_ptr<Entity> spell = std::make_shared<SharedHitbox>(e.Unwrap(), f);
        auto wrappedSpell = WeakWrapper(spell);
        wrappedSpell.Own();
        return wrappedSpell;
      },
      [] (WeakWrapper<Character>& e, float f) -> WeakWrapper<Entity> {
        std::shared_ptr<Entity> spell = std::make_shared<SharedHitbox>(e.Unwrap(), f);
        auto wrappedSpell = WeakWrapper(spell);
        wrappedSpell.Own();
        return wrappedSpell;
      },
      [] (WeakWrapper<ScriptedCharacter>& e, float f) -> WeakWrapper<Entity> {
        std::shared_ptr<Entity> spell = std::make_shared<SharedHitbox>(e.Unwrap(), f);
        auto wrappedSpell = WeakWrapper(spell);
        wrappedSpell.Own();
        return wrappedSpell;
      },
      [] (WeakWrapper<Player>& e, float f) -> WeakWrapper<Entity> {
        std::shared_ptr<Entity> spell = std::make_shared<SharedHitbox>(e.Unwrap(), f);
        auto wrappedSpell = WeakWrapper(spell);
        wrappedSpell.Own();
        return wrappedSpell;
      },
      [] (WeakWrapper<ScriptedPlayer>& e, float f) -> WeakWrapper<Entity> {
        std::shared_ptr<Entity> spell = std::make_shared<SharedHitbox>(e.Unwrap(), f);
        auto wrappedSpell = WeakWrapper(spell);
        wrappedSpell.Own();
        return wrappedSpell;
      },
      [] (WeakWrapper<ScriptedSpell>& e, float f) -> WeakWrapper<Entity> {
        std::shared_ptr<Entity> spell = std::make_shared<SharedHitbox>(e.Unwrap(), f);
        auto wrappedSpell = WeakWrapper(spell);
        wrappedSpell.Own();
        return wrappedSpell;
      },
      [] (WeakWrapper<ScriptedObstacle>& e, float f) -> WeakWrapper<Entity> {
        std::shared_ptr<Entity> spell = std::make_shared<SharedHitbox>(e.Unwrap(), f);
        auto wrappedSpell = WeakWrapper(spell);
        wrappedSpell.Own();
        return wrappedSpell;
      },
      [] (WeakWrapper<Obstacle>& e, float f) -> WeakWrapper<Entity> {
        std::shared_ptr<Entity> spell = std::make_shared<SharedHitbox>(e.Unwrap(), f);
        auto wrappedSpell = WeakWrapper(spell);
        wrappedSpell.Own();
        return wrappedSpell;
      }
    )
  );


  state.new_usertype<Hit::Properties>("HitProps",
    sol::factories([](int damage, Hit::Flags flags, Element element, std::optional<Hit::Context> contextOptional, Hit::Drag drag) {
      Hit::Properties props = { damage, flags, element, 0, drag };

      if (contextOptional) {
        props.context = *contextOptional;
        props.aggressor = props.context.aggressor;
      }

      return props;
    }),
    "aggressor", &Hit::Properties::aggressor,
    "damage", &Hit::Properties::damage,
    "drag", &Hit::Properties::drag,
    "element", &Hit::Properties::element,
    "flags", &Hit::Properties::flags
  );

  state.new_enum("Hit",
    "None", Hit::none,
    "Flinch", Hit::flinch,
    "Flash", Hit::flash,
    "Stun", Hit::stun,
    "Root", Hit::root,
    "Impact", Hit::impact,
    "Shake", Hit::shake,
    "Pierce", Hit::pierce,
    "Retangible", Hit::retangible,
    "Breaking", Hit::breaking,
    "Bubble", Hit::bubble,
    "Freeze", Hit::freeze,
    "Drag", Hit::drag,
    "Blind", Hit::blind
  );

  state.new_usertype<Hit::Drag>("Drag",
    sol::factories(
      [] (Direction dir, unsigned count) { return Hit::Drag{ dir, count }; },
      [] { return Hit::Drag{ Direction::none, 0 }; }
    ),
    "None", sol::property([] { return Hit::Drag{ Direction::none, 0 }; }),
    sol::meta_function::index, []( sol::table table, const std::string key ) { 
      ScriptResourceManager::PrintInvalidAccessMessage( table, "Drag", key );
    },
    sol::meta_function::new_index, []( sol::table table, const std::string key, sol::object obj ) { 
      ScriptResourceManager::PrintInvalidAssignMessage( table, "Drag", key );
    },
    "direction", &Hit::Drag::dir,
    "count", &Hit::Drag::count
  );
}
#endif
