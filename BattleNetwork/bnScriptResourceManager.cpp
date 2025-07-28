#ifdef BN_MOD_SUPPORT
#include <memory>
#include <vector>
#include <functional>
#include <cstdlib>
#include "bnScriptResourceManager.h"
#include "bnAudioResourceManager.h"
#include "bnTextureResourceManager.h"
#include "bnShaderResourceManager.h"
#include "bnResourceHandle.h"
#include "bnCardPackageManager.h"
#include "bnPlayerPackageManager.h"
#include "bnMobPackageManager.h"
#include "bnBlockPackageManager.h"
#include "bnLuaLibraryPackageManager.h"

#include "bnCard.h"
#include "bnEntity.h"
#include "bnCharacter.h"
#include "bnElements.h"
#include "bnField.h"
#include "bnParticlePoof.h"
#include "bnPlayerCustScene.h"
#include "bnRandom.h"

#include "bindings/bnLuaLibrary.h"
#include "bindings/bnScriptedMob.h"
#include "bindings/bnScriptedCard.h"
#include "bindings/bnScriptedCardAction.h"
#include "bindings/bnWeakWrapper.h"
#include "bindings/bnUserTypeAnimation.h"
#include "bindings/bnUserTypeSceneNode.h"
#include "bindings/bnUserTypeSpriteNode.h"
#include "bindings/bnUserTypeSyncNode.h"
#include "bindings/bnUserTypeField.h"
#include "bindings/bnUserTypeTile.h"
#include "bindings/bnUserTypeEntity.h"
#include "bindings/bnUserTypeHitbox.h"
#include "bindings/bnUserTypeBasicCharacter.h"
#include "bindings/bnUserTypeScriptedCharacter.h"
#include "bindings/bnUserTypeBasicPlayer.h"
#include "bindings/bnUserTypeScriptedPlayer.h"
#include "bindings/bnUserTypeScriptedSpell.h"
#include "bindings/bnUserTypeScriptedObstacle.h"
#include "bindings/bnUserTypeScriptedArtifact.h"
#include "bindings/bnUserTypeScriptedComponent.h"
#include "bindings/bnUserTypeCardMeta.h"
#include "bindings/bnUserTypeBaseCardAction.h"
#include "bindings/bnUserTypeScriptedCardAction.h"
#include "bindings/bnUserTypeDefenseRule.h"

// Useful prefabs to use in scripts...
#include "bnExplosion.h"

// temporary proof of concept includes...
#include "bnBusterCardAction.h"

namespace {
  int exception_handler(lua_State* L, sol::optional<const std::exception&> maybe_exception, sol::string_view description) {
    if (maybe_exception) {
      const std::exception& ex = *maybe_exception;
    }
    else {
      std::string message(description.data(), description.size());
    }

    return sol::stack::push(L, description);
  }
}

// Creates a std::list<OverrideFrame> object from a provided table.
// Expected format : { { unsigned int state_in_animation, double duration }, ... }
std::list<OverrideFrame> CreateFrameData(sol::lua_table table)
{
  std::list<OverrideFrame> frames;

  auto count = table.size();

  for( int ind = 1; ind <= count; ++ind )
  {
    unsigned animStateNumber = table.traverse_get<unsigned>(ind, 1);
    double duration = table.traverse_get<double>(ind, 2);

    frames.emplace_back( OverrideFrame { animStateNumber, duration } );
  }

  return frames;
}

void ScriptResourceManager::SetSystemFunctions(ScriptPackage& scriptPackage)
{
  sol::state& state = *scriptPackage.state;
  const std::string& namespaceId = scriptPackage.address.namespaceId;

  state.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table);

  // vulnerability patching, why is this included with lib::base :(
  state["load"] = nullptr;
  state["loadfile"] = nullptr;
  state["dofile"] = nullptr;

  state["math"]["randomseed"] = []{
    Logger::Log(LogLevel::warning, "math.random uses the engine's random number generator and does not need to be seeded");
  };

  state["math"]["random"] = sol::overload(
    [] (int n, int m) -> int { // [n, m]
      int range = m - n;
      return SyncedRand() % (range + 1) + n;
    },
    [] (int n) -> int { // [1, n]
      return SyncedRand() % n + 1;
    },
    [] () -> float { // [0, 1)
      return (float)SyncedRand() / ((float)(SyncedRandMax()) + 1);
    }
  );

  // 'include()' in Lua is intended to load a LIBRARY file of Lua code into the current lua state.
  // Currently only loads library files included in the SAME directory as the script file.
  // Has to capture a pointer to sol::state, the copy constructor was deleted, cannot capture a copy to reference.
  state.set_function("include",
    [this, &state, &namespaceId, &scriptPackage](const std::string& fileName) -> sol::object {
      std::string scriptPath;

      // Prefer using the shared libraries if possible.
      // i.e. ones that were present in "mods/libs/"
      // make sure it was required by checking dependencies as well
      if(std::find(scriptPackage.dependencies.begin(), scriptPackage.dependencies.end(), scriptPath) != scriptPackage.dependencies.end())
      {
        Logger::Logf(LogLevel::debug, "Including shared library: %s", fileName.c_str());

        auto* libraryPackage = FetchScriptPackage(namespaceId, fileName, ScriptPackageType::library);

        if (!libraryPackage) {
          throw std::runtime_error("Library package \"" + fileName + "\" is either not installed or has not had time to initialize");
        }

        scriptPath = libraryPackage->path;
      }
      else
      {
        Logger::Logf(LogLevel::debug, "Including local library: %s", fileName.c_str());

        auto parentFolder = GetCurrentFolder(state).unwrap();
        scriptPath = parentFolder + "/" + fileName;
      }

      sol::environment env(state, sol::create, state.globals());
      env["_folderpath"] = std::filesystem::path(scriptPath).parent_path().string() + "/";

      sol::protected_function_result result = state.do_file(scriptPath, env);

      if (!result.valid()) {
        sol::error error = result;
        throw std::runtime_error(error.what());
      }

      return result;
    }
  );
}

void ScriptResourceManager::SetModPathVariable( sol::state& state, const std::filesystem::path& modDirectory )
{
  state["_modpath"] = modDirectory.generic_string() + "/";
  state["_folderpath"] = modDirectory.generic_string() + "/";
}

// Free Function provided to a number of Lua types that will print an error message when attempting to access a key that does not exist.
// Will print the file and line the error occured in, as well as the invalid key and type. 
sol::object ScriptResourceManager::PrintInvalidAccessMessage( sol::table table, const std::string typeName, const std::string key )
{
  Logger::Log(LogLevel::critical, "[Script Error] in " + GetCurrentLine( table.lua_state() ) );
  Logger::Log(LogLevel::critical, "[Script Error] : Attempted to access \"" + key + "\" in type \"" + typeName + "\"." );
  Logger::Log(LogLevel::critical, "[Script Error] : " + key + " does not exist in " + typeName + "." );
  return sol::lua_nil;
}
// Free Function provided to a number of Lua types that will print an error message when attempting to assign to a key that exists in a system type.
// Will print the file and line the error occured in, as well as the invalid key and type. 
sol::object ScriptResourceManager::PrintInvalidAssignMessage( sol::table table, const std::string typeName, const std::string key )
{
  Logger::Log(LogLevel::critical, "[Script Error] in " + GetCurrentLine( table.lua_state() ) );
  Logger::Log(LogLevel::critical, "[Script Error] : Attempted to assign to \"" + key + "\" in type \"" + typeName + "\"." );
  Logger::Log(LogLevel::critical, "[Script Error] : " + typeName + " is read-only. Cannot assign new values to it." );
  return sol::lua_nil;
}

stx::result_t<std::string> ScriptResourceManager::GetCurrentFile(lua_State* L)
{
  lua_Debug ar;
  lua_getstack(L, 1, &ar);
  lua_getinfo(L, "S", &ar);

  auto path = std::string(ar.source);

  if (path.empty() || path[0] != '@') {
    return stx::error<std::string>("Failed to resolve current file");
  }

  return stx::ok(path.substr(1));
}

stx::result_t<std::string> ScriptResourceManager::GetCurrentFolder(lua_State* L)
{
  auto res = GetCurrentFile(L);

  if (res.is_error()) {
    return res;
  }

  auto path = std::filesystem::path(res.value()).parent_path();

  return stx::ok(path.string());
}

// Returns the current executing line in the Lua script.
// Format: \@[full_filename]:[line_number]
std::string ScriptResourceManager::GetCurrentLine( lua_State* L )
{
  lua_Debug ar;
  lua_getstack(L, 1, &ar);
  lua_getinfo(L, "Sl", &ar);
  
  return std::string(ar.source) + ":" + std::to_string(ar.currentline);
}

void ScriptResourceManager::ConfigureEnvironment(ScriptPackage& scriptPackage) {

  sol::state& state = *scriptPackage.state;
  const std::string& namespaceId = scriptPackage.address.namespaceId;
  sol::table battle_namespace = state.create_table("Battle");
  sol::table overworld_namespace = state.create_table("Overworld");
  sol::table engine_namespace = state.create_table("Engine");

  engine_namespace.set_function("get_rand_seed", [this]() -> unsigned int {
    Logger::Log(LogLevel::warning, "Engine.get_rand_seed() is deprecated");
    return 0;
  });

  DefineFieldUserType(battle_namespace);
  DefineTileUserType(state);
  DefineAnimationUserType(state, engine_namespace);
  DefineSceneNodeUserType(engine_namespace);
  DefineSpriteNodeUserType(state, engine_namespace);
  DefineSyncNodeUserType(engine_namespace);
  DefineEntityUserType(state, battle_namespace);
  DefineHitboxUserTypes(state, battle_namespace);
  DefineBasicCharacterUserType(battle_namespace);
  DefineScriptedCharacterUserType(this, namespaceId, state, battle_namespace);
  DefineBasicPlayerUserType(battle_namespace);
  DefineScriptedPlayerUserType(state, battle_namespace);
  DefineScriptedSpellUserType(battle_namespace);
  DefineScriptedObstacleUserType(state, battle_namespace);
  DefineScriptedArtifactUserType(battle_namespace);
  DefineScriptedComponentUserType(state, battle_namespace);
  DefineBaseCardActionUserType(state, battle_namespace);
  DefineScriptedCardActionUserType(namespaceId, this, battle_namespace);
  DefineDefenseRuleUserTypes(state, battle_namespace);

  auto SetPackageId = [this, &scriptPackage] (const std::string& packageId) {
    if (packageId.empty()) {
      throw std::runtime_error("Package id is blank!");
    }

    if (!scriptPackage.address.packageId.empty()) {
      throw std::runtime_error("Package id has already been set");
    }

    scriptPackage.address.packageId = packageId;

    if (address2package.find(scriptPackage.address) != address2package.end()) {
      throw std::runtime_error("Package id is already in use");
    }

    address2package[scriptPackage.address] = &scriptPackage;
  };

  DefineCardMetaUserTypes(this, state, battle_namespace, SetPackageId);

  // make meta object info metatable
  const auto& playermeta_table = battle_namespace.new_usertype<PlayerMeta>("PlayerMeta",
    sol::meta_function::index, []( sol::table table, const std::string key ) { 
      ScriptResourceManager::PrintInvalidAccessMessage( table, "PlayerMeta", key );
    },
    sol::meta_function::new_index, []( sol::table table, const std::string key, sol::object obj ) { 
      ScriptResourceManager::PrintInvalidAssignMessage( table, "PlayerMeta", key );
    },
    "set_special_description", &PlayerMeta::SetSpecialDescription,
    "set_attack", &PlayerMeta::SetAttack,
    "set_charged_attack", &PlayerMeta::SetChargedAttack,
    "set_speed", &PlayerMeta::SetSpeed,
    "set_health", &PlayerMeta::SetHP,
    "set_uses_sword", &PlayerMeta::SetIsSword,
    "set_overworld_animation_path", &PlayerMeta::SetOverworldAnimationPath,
    "set_overworld_texture_path", &PlayerMeta::SetOverworldTexturePath,
    "set_mugshot_animation_path", &PlayerMeta::SetMugshotAnimationPath,
    "set_mugshot_texture_path", &PlayerMeta::SetMugshotTexturePath,
    "set_emotions_texture_path", &PlayerMeta::SetEmotionsTexturePath,
    "set_preview_texture", &PlayerMeta::SetPreviewTexture,
    "set_icon_texture", &PlayerMeta::SetIconTexture,
    "declare_package_id", [SetPackageId] (PlayerMeta& meta, const std::string& packageId) {
      SetPackageId(packageId);
      meta.SetPackageID(packageId);
    }
  );

  const auto& mobmeta_table = battle_namespace.new_usertype<MobMeta>("MobMeta",
    sol::meta_function::index, []( sol::table table, const std::string key ) { 
      ScriptResourceManager::PrintInvalidAccessMessage( table, "MobMeta", key );
    },
    sol::meta_function::new_index, []( sol::table table, const std::string key, sol::object obj ) { 
      ScriptResourceManager::PrintInvalidAssignMessage( table, "MobMeta", key );
    },
    "set_description", &MobMeta::SetDescription,
    "set_name", &MobMeta::SetName,
    "set_preview_texture_path", &MobMeta::SetPlaceholderTexturePath,
    "set_speed", &MobMeta::SetSpeed,
    "set_attack", &MobMeta::SetAttack,
    "set_health", &MobMeta::SetHP,
    "declare_package_id", [SetPackageId] (MobMeta& meta, const std::string& packageId) {
      SetPackageId(packageId);
      meta.SetPackageID(packageId);
    }
  );
  
  const auto& lualibrarymeta_table = battle_namespace.new_usertype<LuaLibraryMeta>("LuaLibraryMeta",
    sol::meta_function::index, []( sol::table table, const std::string key ) { 
      ScriptResourceManager::PrintInvalidAccessMessage( table, "LuaLibraryMeta", key );
    },
    sol::meta_function::new_index, []( sol::table table, const std::string key, sol::object obj ) { 
      ScriptResourceManager::PrintInvalidAssignMessage( table, "LuaLibraryMeta", key );
    },
    "declare_package_id", [SetPackageId] (LuaLibraryMeta& meta, const std::string& packageId) {
      SetPackageId(packageId);
      meta.SetPackageID(packageId);
    }
  );

  const auto& blockmeta_table = battle_namespace.new_usertype<BlockMeta>("BlockMeta",
    "set_description", &BlockMeta::SetDescription,
    "set_name", &BlockMeta::SetName,
    "set_color", &BlockMeta::SetColor,
    "set_shape", &BlockMeta::SetShape,
    "as_program", &BlockMeta::AsProgram,
    "set_mutator", [](BlockMeta& self, sol::object mutatorObject) {
      ExpectLuaFunction(mutatorObject);

      self.mutator = [mutatorObject](Player& player) {
        sol::protected_function mutator = mutatorObject;
        auto result = mutator(WeakWrapper(player.shared_from_base<Player>()));

        if (!result.valid()) {
          sol::error error = result;
          Logger::Log(LogLevel::critical, error.what());
        }
      };
    },
    "declare_package_id", [SetPackageId] (BlockMeta& meta, const std::string& packageId) {
      SetPackageId(packageId);
      meta.SetPackageID(packageId);
    }
  );

  const auto& scriptedmob_table = battle_namespace.new_usertype<ScriptedMob>("Mob",
    sol::meta_function::index, []( sol::table table, const std::string key ) { 
      ScriptResourceManager::PrintInvalidAccessMessage( table, "Mob", key );
    },
    sol::meta_function::new_index, []( sol::table table, const std::string key, sol::object obj ) { 
      ScriptResourceManager::PrintInvalidAssignMessage( table, "Mob", key );
    },
    "create_spawner", [&namespaceId](ScriptedMob& self, const std::string& fqn, Character::Rank rank) {
      return self.CreateSpawner(namespaceId, fqn, rank);
    },
    "set_background", &ScriptedMob::SetBackground,
    "stream_music", [](ScriptedMob& mob, const std::string& path, std::optional<long long> startMs, std::optional<long long> endMs) {
      mob.StreamMusic(path, startMs.value_or(-1), endMs.value_or(-1));
    },
    "get_field", [](ScriptedMob& o) { return WeakWrapper(o.GetField()); },
    "enable_freedom_mission", &ScriptedMob::EnableFreedomMission,
    "spawn_player", &ScriptedMob::SpawnPlayer
  );

  const auto& scriptedspawner_table = battle_namespace.new_usertype<ScriptedMob::ScriptedSpawner>("Spawner",
    sol::meta_function::index, []( sol::table table, const std::string key ) { 
      ScriptResourceManager::PrintInvalidAccessMessage( table, "Spawner", key );
    },
    sol::meta_function::new_index, []( sol::table table, const std::string key, sol::object obj ) { 
      ScriptResourceManager::PrintInvalidAssignMessage( table, "Spawner", key );
    },
    "spawn_at", &ScriptedMob::ScriptedSpawner::SpawnAt
  );

  battle_namespace.new_usertype<Mob::Mutator>("SpawnMutator",
    sol::meta_function::index, []( sol::table table, const std::string key ) { 
      ScriptResourceManager::PrintInvalidAccessMessage( table, "SpawnMutator", key );
    },
    sol::meta_function::new_index, []( sol::table table, const std::string key, sol::object obj ) { 
      ScriptResourceManager::PrintInvalidAssignMessage( table, "SpawnMutator", key );
    },
    "mutate", [](Mob::Mutator& mutator, sol::object callbackObject) {
      ExpectLuaFunction(callbackObject);

      mutator.Mutate([callbackObject] (std::shared_ptr<Character> character) {
        sol::protected_function callback = callbackObject;

        auto result = callback(WeakWrapper(character));

        if (!result.valid()) {
          sol::error error = result;
          Logger::Log(LogLevel::critical, error.what());
        }
      });
    }
  );

  engine_namespace.set_function("load_texture",
    [](const std::string& path) {
      static ResourceHandle handle;
      return handle.Textures().LoadFromFile(path);
    }
  );

  engine_namespace.set_function("load_shader",
    [](const std::string& path) {
      static ResourceHandle handle;
      return handle.Shaders().LoadShaderFromFile(path);
    }
  );

  engine_namespace.set_function("load_audio",
    [](const std::string& path) {
      static ResourceHandle handle;
      return handle.Audio().LoadFromFile(path);
    }
  );

  engine_namespace.set_function("play_audio",
    sol::factories(
      [](std::shared_ptr<sf::SoundBuffer> buffer, AudioPriority priority) {
      static ResourceHandle handle;
      return handle.Audio().Play(buffer, priority);
    },
    [](AudioType type, AudioPriority priority) {
      static ResourceHandle handle;
      return handle.Audio().Play(type, priority);
    })
  );

  engine_namespace.set_function("stream_music",
    sol::factories(
      [](const std::string& path, std::optional<bool> loop, std::optional<long long> startMs, std::optional<long long> endMs) {
      static ResourceHandle handle;
      return handle.Audio().Stream(path, loop.value_or(true), startMs.value_or(-1), endMs.value_or(-1));
    })
  );

  engine_namespace.set_function("pitch_music",
    [](float pitch) {
      static ResourceHandle handle;
      handle.Audio().SetPitch(pitch);
    }
  );

  engine_namespace.set_function("define_character",
    [this, &scriptPackage](const std::string& fqn, const std::string& path) {
      DefineSubpackage(scriptPackage, ScriptPackageType::character, fqn, path);
    }
  );

  engine_namespace.set_function("requires_character",
    [this, &scriptPackage, &namespaceId](const std::string& fqn) {
      scriptPackage.dependencies.push_back(fqn);
    }
  );

  engine_namespace.set_function("define_card",
    [this, &scriptPackage](const std::string& fqn, const std::string& path) {
      DefineSubpackage(scriptPackage, ScriptPackageType::card, fqn, path);
    }
  );

  engine_namespace.set_function("requires_card",
    [this, &scriptPackage, &namespaceId](const std::string& fqn) {
      scriptPackage.dependencies.push_back(fqn);
    }
  );

  engine_namespace.set_function("define_library",
    [this, &scriptPackage]( const std::string& fqn, const std::string& path ) {
      DefineSubpackage(scriptPackage, ScriptPackageType::library, fqn, path);
    }
  );

  engine_namespace.set_function("requires_library",
    [this, &scriptPackage, &namespaceId](const std::string& fqn) {
      scriptPackage.dependencies.push_back(fqn);
    }
  );

  const auto& elements_table = state.new_enum("Element",
    "Fire", Element::fire,
    "Aqua", Element::aqua,
    "Elec", Element::elec,
    "Wood", Element::wood,
    "Sword", Element::sword,
    "Wind", Element::wind,
    "Cursor", Element::cursor,
    "Summon", Element::summon,
    "Plus", Element::plus,
    "Break", Element::breaker,
    "None", Element::none
  );

  const auto& direction_table = state.new_enum("Direction",
    "flip_x", [](Direction direction) { return FlipHorizontal(direction); },
    "flip_y", [](Direction direction) { return FlipVertical(direction); },
    "reverse", [](Direction direction) { return Reverse(direction); },
    "join", [](Direction a, Direction b) { return Join(a, b); },
    "unit_vector", [](Direction direction) { return UnitVector(direction); },
    "None", Direction::none,
    "Up", Direction::up,
    "Down", Direction::down,
    "Left", Direction::left,
    "Right", Direction::right,
    "UpLeft", Direction::up_left,
    "UpRight", Direction::up_right,
    "DownLeft", Direction::down_left,
    "DownRight", Direction::down_right
  );

  const auto& add_status_record = state.new_enum("EntityStatus",
    "Queued", Field::AddEntityStatus::queued,
    "Added", Field::AddEntityStatus::added,
    "Failed", Field::AddEntityStatus::deleted
  );

  auto input_event_record = state.create_table("Input");

  input_event_record.new_enum("Pressed",
    "Up", InputEvents::pressed_move_up,
    "Left", InputEvents::pressed_move_left,
    "Right", InputEvents::pressed_move_right,
    "Down", InputEvents::pressed_move_down,
    "Use", InputEvents::pressed_use_chip,
    "Special", InputEvents::pressed_special,
    "Shoot", InputEvents::pressed_shoot
  );

  input_event_record.new_enum("Held",
    "Up", InputEvents::held_move_up,
    "Left", InputEvents::held_move_left,
    "Right", InputEvents::held_move_right,
    "Down", InputEvents::held_move_down,
    "Use", InputEvents::held_use_chip,
    "Special", InputEvents::held_special,
    "Shoot", InputEvents::held_shoot
  );

  input_event_record.new_enum("Released",
    "Up", InputEvents::released_move_up,
    "Left", InputEvents::released_move_left,
    "Right", InputEvents::released_move_right,
    "Down", InputEvents::released_move_down,
    "Use", InputEvents::released_use_chip,
    "Special", InputEvents::released_special,
    "Shoot", InputEvents::released_shoot
  );

  const auto& character_rank_record = state.new_enum("Rank",
    "V1", Character::Rank::_1,
    "V2", Character::Rank::_2,
    "V3", Character::Rank::_3,
    "SP", Character::Rank::SP,
    "EX", Character::Rank::EX,
    "Rare1", Character::Rank::Rare1,
    "Rare2", Character::Rank::Rare2,
    "NM", Character::Rank::NM
  );

  const auto& audio_type_record = state.new_enum("AudioType", 
    "CounterBonus", AudioType::COUNTER_BONUS,
    "DirTile", AudioType::DIR_TILE,
    "Fanfare", AudioType::FANFARE,
    "Appear", AudioType::APPEAR,
    "AreaGrab", AudioType::AREA_GRAB,
    "AreaGrabTouchdown", AudioType::AREA_GRAB_TOUCHDOWN,
    "BusterPea", AudioType::BUSTER_PEA,
    "BusterCharged", AudioType::BUSTER_CHARGED,
    "BusterCharging", AudioType::BUSTER_CHARGING,
    "BubblePop", AudioType::BUBBLE_POP,
    "BubbleSpawn", AudioType::BUBBLE_SPAWN,
    "GuardHit", AudioType::GUARD_HIT,
    "Cannon", AudioType::CANNON,
    "Counter", AudioType::COUNTER,
    "Wind", AudioType::WIND,
    "ChipCancel", AudioType::CHIP_CANCEL,
    "ChipChoose", AudioType::CHIP_CHOOSE,
    "ChipConfirm", AudioType::CHIP_CONFIRM,
    "ChipDesc", AudioType::CHIP_DESC,
    "ChipDescClose", AudioType::CHIP_DESC_CLOSE,
    "ChipError", AudioType::CHIP_ERROR,
    "CustomBarFull", AudioType::CUSTOM_BAR_FULL,
    "CustomScreenOpen", AudioType::CUSTOM_SCREEN_OPEN,
    "ItemGet", AudioType::ITEM_GET,
    "Deleted", AudioType::DELETED,
    "Explode", AudioType::EXPLODE,
    "Gun", AudioType::GUN,
    "Hurt", AudioType::HURT,
    "PanelCrack", AudioType::PANEL_CRACK,
    "PanelReturn", AudioType::PANEL_RETURN,
    "Pause", AudioType::PAUSE,
    "PreBattle", AudioType::PRE_BATTLE,
    "Recover", AudioType::RECOVER,
    "Spreader", AudioType::SPREADER,
    "SwordSwing", AudioType::SWORD_SWING,
    "TossItem", AudioType::TOSS_ITEM,
    "TossItemLite", AudioType::TOSS_ITEM_LITE,
    "Wave", AudioType::WAVE,
    "Thunder", AudioType::THUNDER,
    "ElecPulse", AudioType::ELECPULSE,
    "Invisible", AudioType::INVISIBLE,
    "ProgramAdvance", AudioType::PA_ADVANCE,
    "LowHP", AudioType::LOW_HP,
    "DarkCard", AudioType::DARK_CARD,
    "PointSFX", AudioType::POINT_SFX,
    "NewGame", AudioType::NEW_GAME,
    "Text", AudioType::TEXT,
    "Shine", AudioType::SHINE,
    "TimeFreeze", AudioType::TIME_FREEZE,
    "Meteor", AudioType::METEOR,
    "Deform", AudioType::DEFORM
  );

  const auto& audio_priority_record = state.new_enum("AudioPriority",
    "Lowest", AudioPriority::lowest,
    "Low", AudioPriority::low,
    "High", AudioPriority::high,
    "Highest", AudioPriority::highest
  );

  const auto& prog_blocks_record = state.new_enum("Blocks",
    "White", PlayerCustScene::Piece::Types::white,
    "Red", PlayerCustScene::Piece::Types::red,
    "Green", PlayerCustScene::Piece::Types::green,
    "Blue", PlayerCustScene::Piece::Types::blue,
    "Pink", PlayerCustScene::Piece::Types::pink,
    "Yellow", PlayerCustScene::Piece::Types::yellow
  );

  const auto& move_event_record = state.new_usertype<MoveEvent>("MoveEvent",
    sol::factories([] {
      return MoveEvent{};
    }),
    "delta_frames", &MoveEvent::deltaFrames,
    "delay_frames", &MoveEvent::delayFrames,
    "endlag_frames",&MoveEvent::endlagFrames,
    "height", &MoveEvent::height,
    "dest_tile", &MoveEvent::dest,
    "on_begin_func", sol::property(
      [](MoveEvent& event, sol::object onBeginObject) {
        ExpectLuaFunction(onBeginObject);

        event.onBegin = [onBeginObject] {
          sol::protected_function onBegin = onBeginObject;
          auto result = onBegin();

          if (!result.valid()) {
            sol::error error = result;
            Logger::Log(LogLevel::critical, error.what());
          }
        };
      }
    )
  );

  const auto& explosion_record = battle_namespace.new_usertype<Explosion>("Explosion",
    sol::factories([](int count, double speed) -> WeakWrapper<Entity> {
      std::shared_ptr<Entity> artifact = std::make_shared<Explosion>(count, speed);
      auto wrappedArtifact = WeakWrapper(artifact);
      wrappedArtifact.Own();
      return wrappedArtifact;
    })
  );

  const auto& particle_poof = battle_namespace.new_usertype<ParticlePoof>("ParticlePoof",
    sol::factories([]() -> WeakWrapper<Entity> {
      std::shared_ptr<Entity> artifact = std::make_shared<ParticlePoof>();
      auto wrappedArtifact = WeakWrapper(artifact);
      wrappedArtifact.Own();
      return wrappedArtifact;
    })
  );

  const auto& vector_record = state.new_usertype<sf::Vector2f>("Vector2",
    "x", &sf::Vector2f::x,
    "y", &sf::Vector2f::y
  );

  state.set_function("frames",
    [](unsigned num) { return frames(num); }
  );

  state.set_function( "make_frame_data", &CreateFrameData );
}

ScriptResourceManager::~ScriptResourceManager()
{
  for (auto [state, scriptPackage] : state2package) {
    delete state;
    delete scriptPackage;
  }
}

stx::result_t<sol::state*> ScriptResourceManager::LoadScript(const std::string& namespaceId, const std::filesystem::path& modDirectory, ScriptPackageType type)
{
  auto entryPath = modDirectory / "entry.lua";

  sol::state* lua = new sol::state;

  // We must store package information for the proceeding configurations to work correctly
  auto [it, _] = state2package.emplace(lua, new ScriptPackage());
  ScriptPackage& scriptPackage = *it->second;
  scriptPackage.state = lua;
  scriptPackage.type = type;
  scriptPackage.address.namespaceId = namespaceId;
  scriptPackage.path = modDirectory.generic_string();

  // Configure the scripts to run safely
  SetModPathVariable(*lua, modDirectory);
  SetSystemFunctions(scriptPackage);
  ConfigureEnvironment(scriptPackage);

  lua->set_exception_handler(&::exception_handler);
  auto loadResult = lua->safe_script_file(entryPath.generic_string(), sol::script_pass_on_error);

  if (!loadResult.valid()) {
    sol::error loadError = loadResult;
    std::string msg = "Failed to load package " + scriptPackage.address.packageId + ". Reason: " + loadError.what();
    DropPackageData(lua);
    loadResult.abandon();
    return stx::error<sol::state*>(msg);
  }

  return stx::ok<sol::state*>(lua);
}

void ScriptResourceManager::DropPackageData(sol::state* state)
{
  auto stateIt = state2package.find(state);

  if (stateIt == state2package.end()) {
    return;
  }

  auto* package = stateIt->second;
  state2package.erase(stateIt);

  // drop subpackages
  for (auto& packageId : package->subpackages) {
    DropPackageData({ package->address.namespaceId, packageId });
  }

  // drop package
  auto addressIt = address2package.find(package->address);

  if (addressIt != address2package.end()) {
    Logger::Logf(LogLevel::debug, "Dropping package in partition %s with ID %s", package->address.namespaceId.c_str(), package->address.packageId.c_str());
    address2package.erase(addressIt);
  } else {
    Logger::Logf(LogLevel::debug, "Dropping unknown package in partition %s", package->address.namespaceId.c_str());
  }

  delete state;
  delete package;
}

void ScriptResourceManager::DropPackageData(const PackageAddress& addr)
{
  Logger::Logf(LogLevel::debug, "Dropping package in partition %s with ID %s", addr.namespaceId.c_str(), addr.packageId.c_str());
  auto it = address2package.find(addr);

  if (it == address2package.end()) {
    Logger::Logf(LogLevel::debug, "Cannot drop package in partition %s with ID %s", addr.namespaceId.c_str(), addr.packageId.c_str());
    return;
  }

  auto* package = it->second;
  address2package.erase(it);

  // drop subpackages
  for (auto& packageId : package->subpackages) {
    DropPackageData({ package->address.namespaceId, packageId });
  }

  state2package.erase(package->state);

  // drop package
  delete package->state;
  delete package;
}

static std::string PackageTypeToString(ScriptPackageType type) {
  switch (type) {
  case ScriptPackageType::card:
    return "card";
  case ScriptPackageType::character:
    return "character";
  case ScriptPackageType::library:
    return "library";
  default:
    return "other";
  }
}

ScriptPackage* ScriptResourceManager::DefinePackage(ScriptPackageType type, const std::string& namespaceId, const std::string& fqn, const std::string& path)
{
  PackageAddress addr = { namespaceId, fqn };
  auto iter = address2package.find(addr);

  if (iter != address2package.end()) {
    throw std::runtime_error("A package in partition " + addr.namespaceId + " with id " + fqn + " has already been registered");
  }

  auto res = LoadScript(addr.namespaceId, path, type);

  if (res.is_error()) {
    Logger::Logf(LogLevel::critical, "Failed to define %s with FQN %s. Reason: %s", PackageTypeToString(type).c_str(), fqn.c_str(), res.error_cstr());

    // bubble up to end this scene from loading that needs this card...
    throw std::runtime_error(res.error_cstr());
  }

  auto& scriptPackage = state2package[res.value()];
  scriptPackage->address = addr;
  address2package[addr] = scriptPackage;
  return scriptPackage;
}

void ScriptResourceManager::DefineSubpackage(ScriptPackage& parentPackage, ScriptPackageType type, const std::string& fqn, const std::string& path)
{
  ScriptPackage* scriptPackage = DefinePackage(type, parentPackage.address.namespaceId, fqn, path);
  parentPackage.subpackages.push_back(scriptPackage->address.packageId);
}

ScriptPackage* ScriptResourceManager::FetchScriptPackage(const std::string& namespaceId, const std::string& fqn, ScriptPackageType type)
{
  PackageAddress addr = { namespaceId, fqn };
  auto iter = address2package.find(addr);

  if (iter == address2package.end()) {
    return nullptr;
  }

  ScriptPackage* package = iter->second;

  if (type != ScriptPackageType::any && package->type != type) {
    return nullptr;
  }

  return package;
}

void ScriptResourceManager::SetCardPackagePartitioner(CardPackagePartitioner& partition)
{
  cardPartition = &partition;
}

CardPackagePartitioner& ScriptResourceManager::GetCardPackagePartitioner()
{
  return *cardPartition;
}

#endif