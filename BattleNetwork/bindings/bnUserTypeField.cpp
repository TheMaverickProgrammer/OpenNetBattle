#ifdef BN_MOD_SUPPORT
#include "bnUserTypeField.h"

#include "bnWeakWrapper.h"
#include "../bnField.h"
#include "../bnScriptResourceManager.h"
#include "../bnHitboxSpell.h"
#include "bnScriptedCharacter.h"
#include "bnScriptedPlayer.h"
#include "bnScriptedSpell.h"
#include "bnScriptedObstacle.h"
#include "bnScriptedArtifact.h"

static sol::as_table_t<std::vector<WeakWrapper<Character>>> FindNearestCharacters(WeakWrapper<Field>& field, std::shared_ptr<Entity> test, sol::stack_object queryObject) {
  sol::protected_function query = queryObject;

  // store entities in a temp to avoid issues if the scripter mutates entities in this loop
  std::vector<WeakWrapper<Character>> characters;

  field.Unwrap()->FindNearestCharacters(test, [&characters] (std::shared_ptr<Character>& character) -> bool {
    characters.push_back(WeakWrapper(character));
    return false;
  });

  return FilterEntities(characters, queryObject);
}

void DefineFieldUserType(sol::table& battle_namespace) {
  // Exposed "GetCharacter" so that there's a way to maintain a reference to other actors without hanging onto pointers.
  // If you hold onto their ID, and use that through Field::GetCharacter,
  // instead you will get passed a nullptr/nil in Lua after they've been effectively removed,
  // rather than potentially holding onto a hanging pointer to deleted data.
  const auto& field_record = battle_namespace.new_usertype<WeakWrapper<Field>>("Field",
    sol::meta_function::index, []( sol::table table, const std::string key ) { 
      ScriptResourceManager::PrintInvalidAccessMessage( table, "Field", key );
    },
    sol::meta_function::new_index, []( sol::table table, const std::string key, sol::object obj ) { 
      ScriptResourceManager::PrintInvalidAssignMessage( table, "Field", key );
    },
    "tile_at", [] (
      WeakWrapper<Field>& field,
      int x,
      int y
    ) -> Battle::Tile* {
      return field.Unwrap()->GetAt(x, y);
    },
    "width", [] (WeakWrapper<Field>& field) -> int {
      return field.Unwrap()->GetWidth();
    },
    "height", [] (WeakWrapper<Field>& field) -> int {
      return field.Unwrap()->GetHeight();
    },
    "spawn", sol::overload(
      [] (WeakWrapper<Field>& field, WeakWrapper<Entity>& e, int x, int y) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), x, y); },
      [] (WeakWrapper<Field>& field, WeakWrapper<Character>& e, int x, int y) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), x, y); },
      [] (WeakWrapper<Field>& field, WeakWrapper<ScriptedCharacter>& e, int x, int y) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), x, y); },
      [] (WeakWrapper<Field>& field, WeakWrapper<ScriptedPlayer>& e, int x, int y) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), x, y); },
      [] (WeakWrapper<Field>& field, WeakWrapper<ScriptedSpell>& e, int x, int y) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), x, y); },
      [] (WeakWrapper<Field>& field, WeakWrapper<ScriptedObstacle>& e, int x, int y) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), x, y); },
      [] (WeakWrapper<Field>& field, WeakWrapper<Obstacle>& e, int x, int y) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), x, y); },
      [] (WeakWrapper<Field>& field, WeakWrapper<ScriptedArtifact>& e, int x, int y) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), x, y); },
      [] (WeakWrapper<Field>& field, WeakWrapper<HitboxSpell>& e, int x, int y) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), x, y); },

      [] (WeakWrapper<Field>& field, WeakWrapper<Entity>& e, Battle::Tile& tile) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), tile); },
      [] (WeakWrapper<Field>& field, WeakWrapper<Character>& e, Battle::Tile& tile) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), tile); },
      [] (WeakWrapper<Field>& field, WeakWrapper<ScriptedCharacter>& e, Battle::Tile& tile) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), tile); },
      [] (WeakWrapper<Field>& field, WeakWrapper<ScriptedPlayer>& e, Battle::Tile& tile) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), tile); },
      [] (WeakWrapper<Field>& field, WeakWrapper<ScriptedSpell>& e, Battle::Tile& tile) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), tile); },
      [] (WeakWrapper<Field>& field, WeakWrapper<ScriptedObstacle>& e, Battle::Tile& tile) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), tile); },
      [] (WeakWrapper<Field>& field, WeakWrapper<Obstacle>& e, Battle::Tile& tile) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), tile); },
      [] (WeakWrapper<Field>& field, WeakWrapper<ScriptedArtifact>& e, Battle::Tile& tile) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), tile); },
      [] (WeakWrapper<Field>& field, WeakWrapper<HitboxSpell>& e, Battle::Tile& tile) { return field.Unwrap()->AddEntity(e.UnwrapAndRelease(), tile); }
    ),
    "get_character", [] (
      WeakWrapper<Field>& field,
      Entity::ID_t ID
    ) -> WeakWrapper<Character> {
      return WeakWrapper(field.Unwrap()->GetCharacter(ID));
    },
    "get_entity", [] (
      WeakWrapper<Field>& field,
      Entity::ID_t ID
    ) -> WeakWrapper<Entity> {
      return WeakWrapper(field.Unwrap()->GetEntity(ID));
    },
    "find_entities", [](
      WeakWrapper<Field>& field,
      sol::stack_object queryObject
    ) {
      // store entities in a temp to avoid issues if the scripter mutates entities in this loop
      std::vector<WeakWrapper<Entity>> entities;

      field.Unwrap()->FindEntities([&entities](std::shared_ptr<Entity>& entity) -> bool {
        entities.push_back(WeakWrapper(entity));
        return false;
      });

      return FilterEntities(entities, queryObject);
    },
    "find_characters", [] (
      WeakWrapper<Field>& field,
      sol::stack_object queryObject
    ) {
      // store entities in a temp to avoid issues if the scripter mutates entities in this loop
      std::vector<WeakWrapper<Character>> characters;

      field.Unwrap()->FindCharacters([&characters](std::shared_ptr<Character>& character) -> bool {
        characters.push_back(WeakWrapper(character));
        return false;
      });

      return FilterEntities(characters, queryObject);
    },
    "find_obstacles", [](
      WeakWrapper<Field>& field,
      sol::stack_object queryObject
    ) {
      // store entities in a temp to avoid issues if the scripter mutates entities in this loop
      std::vector<WeakWrapper<Obstacle>> obstacles;

      field.Unwrap()->FindObstacles([&obstacles](std::shared_ptr<Obstacle>& obstacle) -> bool {
        obstacles.push_back(WeakWrapper(obstacle));
        return false;
      });

      return FilterEntities(obstacles, queryObject);
    },
    "find_nearest_characters", sol::overload(
      [] (WeakWrapper<Field>& field, WeakWrapper<Entity>& test, sol::stack_object queryObject) {
        return FindNearestCharacters(field, test.Unwrap(), queryObject);
      },
      [] (WeakWrapper<Field>& field, WeakWrapper<Character>& test, sol::stack_object queryObject) {
        return FindNearestCharacters(field, test.Unwrap(), queryObject);
      },
      [] (WeakWrapper<Field>& field, WeakWrapper<ScriptedCharacter>& test, sol::stack_object queryObject) {
        return FindNearestCharacters(field, test.Unwrap(), queryObject);
      },
      [] (WeakWrapper<Field>& field, WeakWrapper<Player>& test, sol::stack_object queryObject) {
        return FindNearestCharacters(field, test.Unwrap(), queryObject);
      },
      [] (WeakWrapper<Field>& field, WeakWrapper<ScriptedPlayer>& test, sol::stack_object queryObject) {
        return FindNearestCharacters(field, test.Unwrap(), queryObject);
      },
      [] (WeakWrapper<Field>& field, WeakWrapper<ScriptedObstacle>& test, sol::stack_object queryObject) {
        return FindNearestCharacters(field, test.Unwrap(), queryObject);
      },
      [] (WeakWrapper<Field>& field, WeakWrapper<Obstacle>& test, sol::stack_object queryObject) {
        return FindNearestCharacters(field, test.Unwrap(), queryObject);
      },
      [] (WeakWrapper<Field>& field, WeakWrapper<ScriptedSpell>& test, sol::stack_object queryObject) {
        return FindNearestCharacters(field, test.Unwrap(), queryObject);
      },
      [] (WeakWrapper<Field>& field, WeakWrapper<ScriptedArtifact>& test, sol::stack_object queryObject) {
        return FindNearestCharacters(field, test.Unwrap(), queryObject);
      }
    ),
    "find_tiles", [] (
      WeakWrapper<Field>& field,
      sol::object queryObject
    ) {
      auto tiles = field.Unwrap()->FindTiles([queryObject] (Battle::Tile* t) -> bool {
        sol::protected_function query = queryObject;
        auto result = CallLuaCallbackExpectingBool(query, t);

        if (result.is_error()) {
          Logger::Log(LogLevel::critical, result.error_cstr());
          return false;
        }

        return result.value();
      });

      return sol::as_table(tiles);
    },
    "notify_on_delete", [] (
      WeakWrapper<Field>& field,
      Entity::ID_t target,
      Entity::ID_t observer,
      sol::object callbackObject
    ) -> Field::NotifyID_t {
      ExpectLuaFunction(callbackObject);

      return field.Unwrap()->NotifyOnDelete(
        target,
        observer,
        [callbackObject] (std::shared_ptr<Entity> target, std::shared_ptr<Entity> observer) {
          sol::protected_function callback = callbackObject;
          auto result = callback(WeakWrapper(target), WeakWrapper(observer));

          if (!result.valid()) {
            sol::error error = result;
            Logger::Log(LogLevel::critical, error.what());
          } 
        }
      );
    },
    "callback_on_delete", [] (
      WeakWrapper<Field>& field,
      Entity::ID_t target,
      sol::object callbackObject
    ) -> Field::NotifyID_t {
      ExpectLuaFunction(callbackObject);

      return field.Unwrap()->CallbackOnDelete(
        target,
        [callbackObject] (std::shared_ptr<Entity> e) {
          sol::protected_function callback = callbackObject;
          auto result = callback(WeakWrapper(e));

          if (!result.valid()) {
            sol::error error = result;
            Logger::Log(LogLevel::critical, error.what());
          } 
        }
      );
    }
  );
}
#endif
