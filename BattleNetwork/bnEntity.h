/*! \brief Base class for all content that exists on the battle field
 * 
 *  \class Entity
 * 
 * Moving on a battle field consisting of a 6x3 grid requires specific
 * behavior. To appear authentic, the entities that exist on the field
 * must abide by special movement rules. Movement is on a per-tile basis
 * and entities can only appear on one tile at a time. 
 * 
 * Entities by themselves respond to no collision and knows nothing
 * about health points. Entities only know about the tile they are on,
 * their element if any, their team, their passthrough properties,
 * and maintain a list of Components that may alter their behavior in 
 * battle.
 */

#pragma once
#include <string>
#include <vector>
#include <functional>
using std::string;

#include "bnResourceHandle.h"
#include "bnAnimation.h"
#include "bnDirection.h"
#include "bnTeam.h"
#include "bnGame.h"
#include "bnTextureType.h"
#include "bnElements.h"
#include "bnComponent.h"
#include "bnEventBus.h"
#include "bnActionQueue.h"

namespace Battle {
  class Tile;
}

class Field;
class BattleSceneBase; // forward decl

struct MoveEvent {
  frame_time_t deltaFrames{}; //!< Frames between tile A and B. If 0, teleport. Else, we could be sliding
  frame_time_t delayFrames{}; //!< Startup lag to be used with animations
  frame_time_t endlagFrames{}; //!< Wait period before action is complete
  float height{}; //!< If this is non-zero with delta frames, the character will effectively jump
  Battle::Tile* dest{ nullptr };
  std::function<void()> onBegin = []{};
  bool immutable{ false }; //!< Some move events cannot be cancelled or interupted

  //!< helper function true if jumping
  inline bool IsJumping() const {
    return dest && height > 0.f && deltaFrames > frames(0);
  }

  //!< helper function true if sliding
  inline bool IsSliding() const {
    return dest && deltaFrames > frames(0) && height <= 0.0f;
  }

  //!< helper function true if normal moving
  inline bool IsTeleporting() const {
    return dest && deltaFrames == frames(0) && (+height) == 0.0f;
  }
};

struct EntityComparitor {
  bool operator()(Entity* f, Entity* s) const;
};

class Entity : public SpriteProxyNode, public ResourceHandle {
public:
  using ID_t = long;

  friend class Field;
  friend class Component;
  friend class BattleSceneBase;

private:
  ID_t ID{}; /*!< IDs are used for tagging during battle & to identify entities in scripting. */
  static long numOfIDs; /*!< Internal counter to identify the next entity with. */
  int alpha{ 255 }; /*!< Control the transparency of an entity. */
  Component::ID_t lastComponentID{}; /*!< Entities keep track of new components to run through scene injection later. */
  bool hasSpawned{ false }; /*!< Flag toggles true when the entity is first placed onto the field. Calls OnSpawn(). */
  float height{0}; /*!< Height of the entity relative to tile floor. Used for visual effects like projectiles or for hitbox detection */
  bool isUpdating{ false }; /*!< If an entity has updated once this frame, skip some update routines */
  EventBus::Channel channel; /*!< Our event bus channel to emit events */
  MoveEvent currMoveEvent{};
  unsigned moveEventFrame{};
  unsigned frame{};
  float currJumpHeight{};

  /**
   * @brief Frees one component with the same ID
   * @param ID ID of the component to remove
   */
  void SortComponents();
  void ClearPendingComponents();
  void ReleaseComponentsPendingRemoval();
  void InsertComponentsPendingRegistration();
  void UpdateMovement(double elapsed);
  void SetFrame(unsigned frame);
public:
  Entity();
  virtual ~Entity();

  /**
  * @brief Performs some user-specified deletion behavior before removing from play
  *
  * Deleted entities are excluded from all battle attack steps however they 
  * will be visually present and must be removed from field by calling Remove()
  */
  virtual void OnDelete() = 0;

  /**
  * @brief Sets the tile for this entity and invokes OnSpawn() if this is the first time
  */
  void Spawn(Battle::Tile& start);

  virtual void OnSpawn(Battle::Tile& start) { };

  /**
  * @brief Performs some user-specified behavior when the battle starts for the first time 
  * @example See bnGears.h for an entity that is live when the round begins and only moves after this call.
  */
  virtual void OnBattleStart() { };

  /**
  * @brief Performs some user-specified behavior when the battle is over
  * @example See bnGears.h for an entity that only stops in place when the round is over
  */
  virtual void OnBattleStop() { };

  /**
   * @brief TODO
   */
  virtual void Update(double _elapsed);
  
  /**
   * @brief Move to any tile on the field
   * @return true if the move action was queued. False if obstructed.
   * 
   * @warning This doesn't mean that the entity will successfully move just that they could at the time
   */
  bool Teleport(Battle::Tile* dest, ActionOrder order = ActionOrder::voluntary, std::function<void()> onBegin = [] {});
  bool Slide(Battle::Tile* dest, const frame_time_t& slideTime, const frame_time_t& endlag, ActionOrder order = ActionOrder::voluntary, std::function<void()> onBegin = [] {});
  bool Jump(Battle::Tile* dest, float destHeight, const frame_time_t& jumpTime, const frame_time_t& endlag, ActionOrder order = ActionOrder::voluntary, std::function<void()> onBegin = [] {});
  void FinishMove();
  void RawMoveEvent(const MoveEvent& event, ActionOrder order = ActionOrder::voluntary);
  void HandleMoveEvent(MoveEvent& event, const ActionQueue::ExecutionType& exec);
  void ClearActionQueue();
  const float GetJumpHeight() const;
  virtual void FilterMoveEvent(MoveEvent& event) {};

  /**
  * @brief How high off the ground the entity currently is
  * @return current height off ground
  */
  const float GetCurrJumpHeight() const;

  /**
   * @brief Virtual. Queries if an entity can move to a target tile.
   * This function dictates the rules of when an entity can move.
   * Some implementation exampes are when bosses can teleport to the player's area
   * only when their state permits it. It will return true in that case but false
   * when the boss is not attacking and shall only be allowed to roam on their 
   * team's side.
   * @param next The tile to query.
   * @return true if the entity can move to the tile, false if not permitted.
   */
  virtual bool CanMoveTo(Battle::Tile* next);

  /**
   * @brief Entity's ID
   * @return ID 
   */
  const ID_t GetID() const;

  /**
   * @brief Checks to see if the input team is friendly 
   * @param _team
   * @return true if the input team is the same or friendly, false if they are opposing teams
   */
  bool Teammate(Team _team) const;

  /**
   * @brief Changes the tile pointer to the input tile
   * Directly changing the tile pointer is not recommended. @see Teleport() @see Move()
   * The input tile claims ownership of the entity and the entity's old tile releases ownership
   * @param _tile
   */
  void SetTile(Battle::Tile* _tile);
  
  /**
   * @brief Get a pointer to a tile based upon the Entity's position.
   * If no arguments are provided, it will return a pointer to the tile they are currently on.
   * If arguments are provided, it will attempt to return a pointer to a tile offset by a specified amount in a specified direction.
   * @param dir The direction you want to pick the tile from
   * @param count How many tiles away that tile is
   * @return Tile pointer
   */
  Battle::Tile* GetTile(Direction dir = Direction::none, unsigned count = 0) const;
  /**
   * @brief Get a pointer to the tile the Entity is currently located on
   * Overloaded function as sol docs say there isn't any support for default arguments in Lua
   * Without this, scripts would need to specify direction and offset every time
   */
  Battle::Tile* GetCurrentTile() const;
  const sf::Vector2f GetTileOffset() const;

  /**
   * @brief Checks if entity is moving
   */
  const bool IsSliding() const;
  const bool IsJumping() const;
  const bool IsTeleporting() const;
  const bool IsMoving() const;

  /**
   * @brief Sets the field pointer
   * @param _field
   */
  void SetField(Field* _field);

  /**
   * @brief Gets the field pointer
   * Useful to check field state such as IsTimeFrozen()
   * @return Field pointer
   */
  Field* GetField() const;

  /**
   * @brief Gets the entity's assigned team
   * @return Current Team
   */
  Team GetTeam() const;
  
  /**
   * @brief Assigns team 
   * @param _team the team to assign
   */
  void SetTeam(Team _team);

  /**
   * @brief Attacks skip entities with passthrough enabled.
   * @param state set true for passthrough, set false to be tangible
   */
  void SetPassthrough(bool state);
  
  /**
   * @brief Get the passthrough state
   * @return true if passthrough, false if tangible
   */
  bool IsPassthrough();

  /**
   * @brief Sets the opacity level of this entity
   * @param value range from 0-255 
   */
  void SetAlpha(int value);

  /**
   * @brief Enable or disable FloatShoe
   * @param state set true to enable, false to disable
   */
  void SetFloatShoe(bool state);
  
  /**
   * @brief Enable or disable AirShoe
   * @param state set true to enable, false to disable
   */
  void SetAirShoe(bool state);

  /**
  * @brief prevents tile slide events like ice
  */
  void SlidesOnTiles(bool state);
  
  /**
   * @brief Query if entity has FloatShoe enabled
   * @return true if Floatshoe is enabled, false otherwise
   */
  bool HasFloatShoe();
  
  /**
   * @brief Query if entity has AirShoe enabled
   * @return true if AirShoe is enabled, false otherwise
   */
  bool HasAirShoe();

  bool WillSlideOnTiles();

  /**
   * @brief Directly modify the entity's current direction property. @see Entity::Move() 
   * Direction can be used for entities that use a single direction for linear movement.
   * e.g. Mettaur Waves, Bullet, Fishy
   * @param direction the new direction
   */
  void SetDirection(Direction direction);
  
  /**
   * @brief Query the entity's current direction
   * @return The entity's current direction
   */
  Direction GetDirection();
  
  void SetFacing(Direction facing);
  Direction GetFacing();
  /**
   * @brief Query the entity's previous direction
   * Can be used to see if an entity was previously moving before it changed direction
   * @see Gear::Update()
   * @return The entity's previous direction
   */
  Direction GetPreviousDirection();

  /**
   * @brief Frees and clears all components attached. Sets `delete` flag to true.
   */
  void Delete();

  /**
  * @brief Flags the entity to be pruned from the field 
  */
  void Remove();
  
  /**
   * @brief Query if an entity has been deleted but not removed this frame
   * @return true if flagged for deletion, false otherwise
   */
  bool IsDeleted() const;

  /**
  * @brief Query if an entity has been marked for removal
  * @return true if flagged, false otherwise
  */
  bool WillRemoveLater() const;

  /**
   * @brief Changes the element of the entity
   * Elements change combat outcomes and some tile effects
   * @param _elem the element to change to
   */
  void SetElement(Element _elem);
  
  /**
   * @brief Query the element of the entity
   * @return Returns the current element
   */
  const Element GetElement() const;
  
  /**
   * @brief Takes an input elements and returns if the element is super effective against this entity
   * Wood > Electric > Aqua > Fire > Wood 
   * @param _other the other element
   * @return true if super effective, false if not
   */
  const bool IsSuperEffective(Element _other) const;
  
  /**
   * @brief Forces the entity to adopt the next tile pointer 
   */
  void AdoptNextTile();
  
  /**
   * @brief Pure virtual. Must be defined by super classes of Entity.
   * 
   * This is a visitor design pattern with double distpatching
   * The super class must call tile->AddEntity(*this) to force the overloaded type
   * in the Tile class so that the entity is added to the correct type bucket @see Tile
   * @param tile to adopt
   */
  virtual void AdoptTile(Battle::Tile* tile) = 0;

  /**
   * @brief Sets the entity's TFC flag
   * Useful in update loop so that enemies will not attack when
   * the screen is paused or the cards are open
   * @param state true to freeze time, false if ongoing
   */
  void ToggleTimeFreeze(bool state);
  
  /**
   * @brief Query if entity is time frozen
   * @return true if entity is time frozen, false if active
   */
  const bool IsTimeFrozen();

  /**
   * @brief Get the first component that matches the exact Type
   * @return null if no component is found, otherwise returns the component
   */
  template<typename ComponentType>
  ComponentType* GetFirstComponent() const;

   /**
   * @brief Get all components that matches the exact Type
   * @return vector of specified components
   */
  template<typename ComponentType>
  std::vector<ComponentType*> GetComponents() const;

  /**
* @brief Get all components that inherit BaseType
* @return vector of related components
*/
  template<typename BaseType>
  std::vector<BaseType*> GetComponentsDerivedFrom() const;

  /**
  * @brief Check if entity is a specialized type
  * @return true if entity could be dynamically casted to Type
  * @warning dynamic casts are costly! Avoid if possible!
  */
  template<typename Type>
  bool IsA();

  /**
   * @brief Creates and then registers a component to an entity
   * @param Args. Parameter pack of any argument type to pass into the component's constructor
   * @return Returns the component as a pointer of the same type as ComponentT
   */
  template<typename ComponentType, typename... Args>
  ComponentType* CreateComponent(Args&&...);

  /**
  * @brief Attaches a component to an entity
  * @param c the component to add
  * @return Returns the component as a pointer of the common base class type
  */
  Component* RegisterComponent(Component* c);
  
  /**
   * @brief Frees all attached components from an owner
   * All attached components owner pointer will be null
   */
  void FreeAllComponents();

  /**
  * @brief Returns a channel in the EventBus
  * @return Returns the channel as const reference
  */
  const EventBus::Channel& EventChannel() const;

  /**
   * @brief Hit height can be overwritten to deduce from sprite bounds
   * @return float
   */
  virtual const float GetHeight() const;
  virtual void SetHeight(const float height);

protected:
  Battle::Tile* tile{ nullptr }; /**< Current tile pointer */
  Battle::Tile* previous{ nullptr }; /**< Entities retain a previous pointer in case they need to be moved back */
  sf::Vector2f tileOffset{ 0,0 }; /**< complete motion is captured by `tile_pos + tileOffset`*/
  sf::Vector2f moveStartPosition{ 0,0 }; /**< Used internally when moving*/
  Field* field{ nullptr };
  Team team{};
  Element element{Element::none};
  ActionQueue actionQueue;
  frame_time_t moveStartupDelay{};
  frame_time_t moveEndlagDelay{};

  std::vector<Component*> components; /*!< List of all components attached to this entity*/

  struct ComponentBucket {
    Component* pending{ nullptr };
    enum class Status : unsigned {
      add,
      remove
    };

    Status action{ Status::add };
  };
  std::list<ComponentBucket> queuedComponents;

  const int GetMoveCount() const; /*!< Total intended movements made. Used to calculate rank*/
  void SetMoveEndlag(const frame_time_t& frames);
  void SetMoveStartupDelay(const frame_time_t& frames);

private:
  bool isTimeFrozen{};
  bool ownedByField{}; /*!< Must delete the entity manual if not owned by the field. */
  bool passthrough{};
  bool floatShoe{};
  bool airShoe{};
  bool slidesOnTiles{true}; /* by default everything responds to tile push/slide events */
  bool deleted{}; /*!< Used to trigger OnDelete() callback and exclude entity from most update routines*/
  bool flagForRemove{}; /*!< Used to remove this entity from the field immediately */
  int moveCount{}; /*!< Used by battle results */
  double elapsedMoveTime{}; /*!< delta time since recent move event began */
  Direction direction{};
  Direction previousDirection{};
  Direction facing{};

    /**
   * @brief Used internally before moving and updates the start position
   */
  void UpdateMoveStartPosition();

  /**
   * @brief Frees one component with the same ID
   * @param ID ID of the component to remove
   */
  void FreeComponentByID(Component::ID_t ID);
};

template<typename ComponentType>
inline ComponentType* Entity::GetFirstComponent() const
{
  for (vector<Component*>::const_iterator it = components.begin(); it != components.end(); ++it) {
    if (typeid(*(*it)) == typeid(ComponentType)) {
      return dynamic_cast<ComponentType*>(*it);
    }
  }

  return nullptr;
}

template<typename ComponentType>
inline std::vector<ComponentType*> Entity::GetComponents() const
{
  auto res = std::vector<ComponentType*>();

  for (vector<Component*>::const_iterator it = components.begin(); it != components.end(); ++it) {
    if (typeid(*(*it)) == typeid(ComponentType)) {
      res.push_back(static_cast<ComponentType*>(*it));
    }
  }

  return res;
}


template<typename BaseType>
inline std::vector<BaseType*> Entity::GetComponentsDerivedFrom() const
{
  auto res = std::vector<BaseType*>();

  for (vector<Component*>::const_iterator it = components.begin(); it != components.end(); ++it) {
    BaseType* cast = dynamic_cast<BaseType*>(*it);

    if (cast) {
      res.push_back(cast);
    }
  }

  return res;
}

template<typename Type>
inline bool Entity::IsA() {
  return (dynamic_cast<Type*>(this) != nullptr);
}

template<typename ComponentType, typename... Args>
inline ComponentType* Entity::CreateComponent(Args&& ...args) {
  ComponentType* c = new ComponentType(std::forward<decltype(args)>(args)...);

  RegisterComponent(c);

  return c;
}