#pragma once
#include "bnAI.h"
#include "bnComponent.h"
#include "bnCharacter.h"
#include "bnBattleItem.h"
#include "bnBackground.h"
#include "bnField.h"
#include "bnMobHealthUI.h"
#include "stx/tuple.h"

#include <vector>
#include <map>
#include <stdexcept>
#include <type_traits>

/*! \brief Manages spawning and deleting the enemy mob and delivers a reward based on rank */
class Mob
{
public:
  /*! \brief Spawn info data object */
  struct MobData {
    Character* character{ nullptr }; /*!< The character to spawn */
    int tileX{}; /*!< The tile column to spawn on */
    int tileY{}; /*!< The tile row to spawn on */
    unsigned index{}; /*!< this character's spawn order */
  };

  class Mutator {
    friend class Mob;

    MobData* data{ nullptr };
    std::vector<std::function<void(Character& in)>> mutations;

    MobData* GetSpawned() {
      for (auto&& m : mutations) {
        m(*data->character);
      }

      return data;
    }

  public:
    Mutator(MobData* data) : data(data) {

    }

    void Mutate(const std::function<void(Character& in)>& mutation) {
      mutations.push_back(mutation);
    }
  };

private:
  size_t spawnIndex{}; /*!< Current character to spawn in the spawn loop*/
  bool nextReady{ true }; /*!< Signal if mob is ready to spawn the next character */
  bool isBoss{ false }; /*!< Flag to change rank and music */
  std::string music; /*!< Override with custom music */
  std::vector<Mutator*> spawn; /*!< The enemies to spawn and manage */
  std::vector<Character*> tracked; /*! Enemies that may or may not be spawned through the mob class but need to be considered */
  std::vector<std::function<void(Character*)>> defaultStateInvokers; /*!< Invoke the character's default state from the spawn policy */
  std::vector<std::function<void(Character*)>> pixelStateInvokers; /*!< Invoke the character's intro tate from the spawn policy */
  std::multimap<int, BattleItem> rewards; /*!< All possible rewards for this mob by rank */
  Field* field{ nullptr }; /*!< The field to play on */
  std::shared_ptr<Background> background; /*!< Override with custom background */

public:
  friend class ScriptedMob;

  // forward decl
  template<class ClassType> class Spawner;

  /**
   * @brief constructor
   */
  Mob(Field* _field);

  /**
   * @brief Deletes memory and cleanups all enemies
   */
  ~Mob();

  /**
   * @brief Register a reward for this mob. 
   * @param rank Cap ranks between 1-10 and where 11 is Rank S
   * @param item The item to reward with
   */
  void RegisterRankedReward(int rank, BattleItem item);

  /**
   * @brief Find all possible rewards based on this score
   * @param score generated from BattleRewards class 
   * @return Randomly chooses possible ranked reward or nullptr if none 
   */
  BattleItem* GetRankedReward(int score);

  /**
   * @brief Delete the mobdata objects and all owned components
   */
  void Cleanup();

  /**
   * @brief Useful for debugging: kill all enemies on screen
   */
  void KillSwitch();

  /**
   * @brief Get the field
   * @return Field*
   */
  Field* GetField();

  /**
   * @brief Get the count of enemies in battle
   * @return const int size
   */
  const int GetMobCount();

  /**
   * @brief removes a character from the mob list
   * 
   * When the list is empty, the mob has been cleared
   * We wish to forget about them because the Field class
   * handles deletion of entities that have spawned
   * 
   * @see Field
   */
  void Forget(Character& character);

  /**
   * @brief Get the count of remaining enemies in battle
   * 
   * If health is below or equal to zero, they are skipped
   * 
   * @return const int 
   */
  const int GetRemainingMobCount();

  /**
   * @brief Toggle boss flag. Changes scoring system and music.
   */
  void ToggleBossFlag();

  /**
   * @brief Query if boss battle 
   * @return true if isBoss is true, otherwise false
   */
  bool IsBoss();

  /**
   * @brief Set a custom background
   * @param background
   */
  void SetBackground(const std::shared_ptr<Background> background);

  /**
   * @brief Get the background object
   * @return Background*
   */
  std::shared_ptr<Background> GetBackground();

  /**
   * @brief The battle scene will load this custom music
   * @param path path relative to the app 
   */
  void StreamCustomMusic(const std::string& path);

  /**
   * @brief Checks if custom music path was set
   * @return true if music string length is > 0
   */
  bool HasCustomMusicPath();

  /**
   * @brief Gets the custom music path
   * @return const std::string
   */
  const std::string GetCustomMusicPath() const;

  /**
   * @brief Gets the mob at spawn index
   * @param index spawn index
   * @return const Character& 
   * @throws std::runtime_error if index is not in range
   */
  const Character& GetMobAt(int index);

  /**
   * @brief Query if the next mob is ready to be spawned
   * @return true if flag is set and the previous spawning step is ongoing, otherwise false
   */
  const bool NextMobReady();

  /**
   * @brief Query if the spawning step is finishes
   * @return if spawn iterator is at the end and the last character has spawned, return true
   */
  const bool IsSpawningDone();

  /**
   * @brief Query if all the enemies have been deleted
   * @return true if all mobs are marked for removal from play via WillRemoveLater() == true
   */
  const bool IsCleared();

  /**
   * @brief Used in state ivokers to flag the mob to spawn the next piece
   * 
   * TODO: If possible hide this step from spawn policies. It's not the spawn policies direct responsibility 
   * and is always required.
   */
  void FlagNextReady();

  /**
   * @brief Changes all enemies state to their default state
   */
  void DefaultState();

  /**
   * @brief Get the next unspawned enemy
   * 
   * Spawns the mob and changes its state to PixelInState<>
   * @return MobData*
   */
  MobData* GetNextMob();

  /**
   * @brief Create a spawner object to spawn typed enemies
   * @param Args... constructor arguments for deferred loading
   * @return Spawner<> object to reuse
   */
  template<class ClassType>
  Spawner<ClassType> CreateSpawner();

  // with args...
  template<class ClassType, typename... Args>
  Spawner<ClassType> CreateSpawner(Args&&...);

  void Track(Character& character);
};

//
// Mob::CreateSpawner() function implementations...
//

template<class ClassType>
Mob::Spawner<ClassType> Mob::CreateSpawner() {
  auto item = Mob::Spawner<ClassType>();
  item.SetMob(this);
  return item;
}

template<class ClassType, typename... Args>
Mob::Spawner<ClassType> Mob::CreateSpawner(Args&&... args) {
  auto item = Mob::Spawner<ClassType>(std::forward<decltype(args)>(args)...);
  item.SetMob(this);
  return item;
}

//
// Spawner implementation...
//

template<class ClassType>
class Mob::Spawner {
protected:
  std::function<ClassType*()> constructor;
  Mob* mob{ nullptr };
public:
  friend class ScriptedMob;

  // ctors with zero arguments (or default args are provided)
  Spawner() {
    constructor = []() mutable {
      return new ClassType();
    };
  }

  // copy ctor
  Spawner(const Spawner& rhs) : constructor(rhs.constructor), mob(rhs.mob) {}

  // c-tors with arguments (std::enable_if_t prevents matching with the copy ctor)
  template<typename... Args, typename std::enable_if_t<!std::is_convertible<Args&&..., const Spawner&>::value>* = nullptr>
  Spawner(Args&&... args) {
    constructor = [tuple_args = std::make_tuple(std::forward<decltype(args)>(args)...)]() mutable->ClassType* {
      return stx::make_ptr_from_tuple<ClassType>(tuple_args);
    };
  }

  // virtual deconstructor for inheritence
  virtual ~Spawner() { }

  template<template<typename> class IntroState>
  Mutator* SpawnAt(unsigned x, unsigned y) {
    // assert that tileX and tileY exist in field
    assert(x >= 1 && x <= static_cast<unsigned>(mob->field->GetWidth()) 
        && y >= 1 && y <= static_cast<unsigned>(mob->field->GetHeight()));

    // Create a new enemy spawn data object
    MobData* data = new MobData();
    Character* character = constructor();

    auto ui = character->CreateComponent<MobHealthUI>(character);
    ui->Hide(); // let default state invocation reveal health

    // Assign the enemy to the spawn data object
    data->character = character;
    data->tileX = x;
    data->tileY = y;
    data->index = (unsigned)mob->spawn.size();

    mob->field->GetAt(x, y)->ReserveEntityByID(character->GetID());

    auto mutator = new Mutator(data);

    // Thinking we need to remove AI inheritence and be another component on its own
    Mob* mobPtr = this->mob;
    auto pixelStateInvoker = [mobPtr](Character* character) {
      auto onFinish = [mobPtr]() { mobPtr->FlagNextReady(); };

      ClassType* enemy = static_cast<ClassType*>(character);

      if (enemy) {
        if constexpr (std::is_base_of<AI<ClassType>, ClassType>::value) {
          enemy->template ChangeState<IntroState<ClassType>>(onFinish);
        }
        else {
          enemy->template InterruptState<IntroState<ClassType>>(onFinish);
        }
      }
    };

    auto defaultStateInvoker = [ui](Character* character) {
      ClassType* enemy = static_cast<ClassType*>(character);
      if (enemy) { enemy->InvokeDefaultState(); ui->Reveal(); }
    };

    mob->pixelStateInvokers.push_back(pixelStateInvoker);
    mob->defaultStateInvokers.push_back(defaultStateInvoker);

    // Set name special font based on rank
    switch(data->character->GetRank()) {
    case ClassType::Rank::_2:
      data->character->SetName(data->character->GetName() + "2");
      break;
    case ClassType::Rank::_3:
      data->character->SetName(data->character->GetName() + "3");
      break;
    case ClassType::Rank::Rare1:
      data->character->SetName(data->character->GetName() + "R1");
      break;
    case ClassType::Rank::Rare2:
      data->character->SetName(data->character->GetName() + "R2");
      break;
    case ClassType::Rank::SP:
      data->character->SetName(data->character->GetName() + char(-1));
      break;
    case ClassType::Rank::EX:
      data->character->SetName(data->character->GetName() + char(-2));
      break;
    } 

    // Add the mob spawn data to our list of enemies to spawn
    mob->spawn.push_back(mutator);
    mob->tracked.push_back(data->character);

    // Return a mutator to change some spawn info
    return mutator;
  }

  void SetMob(Mob* ptr) {
    this->mob = ptr;
  }
};