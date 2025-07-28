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
#include "bnSpriteProxyNode.h"
#include "bnAnimation.h"
#include "bnDirection.h"
#include "bnTeam.h"
#include "bnResourcePaths.h"
#include "bnElements.h"
#include "bnComponent.h"
#include "bnEventBus.h"
#include "bnActionQueue.h"
#include "bnVirtualInputState.h"
#include "bnCounterHitPublisher.h"
#include "bnHitPublisher.h"
#include "bnDefenseFrameStateJudge.h"
#include "bnDefenseRule.h"
#include "bnHitProperties.h"
#include "stx/memory.h"

namespace Battle {
  class Tile;
  enum class TileHighlight : int;
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

struct CombatHitProps {
  Hit::Properties hitbox; // original hitbox data
  Hit::Properties filtered; // statuses after defense rules pass

  CombatHitProps(const Hit::Properties& original, const Hit::Properties& props) : 
    hitbox(original), filtered(props)
  {}

  CombatHitProps(const CombatHitProps& rhs) {
    this->operator=(rhs);
  }

  CombatHitProps& operator=(const CombatHitProps& rhs) {
    hitbox = rhs.hitbox;
    filtered = rhs.filtered;
    return *this;
  }
};

struct EntityComparitor {
  bool operator()(std::shared_ptr<Entity> f, std::shared_ptr<Entity> s) const;
  bool operator()(Entity* f, Entity* s) const;
};

class Entity :
  public SpriteProxyNode,
  public ResourceHandle,
  public CounterHitPublisher,
  public HitPublisher,
  public stx::enable_shared_from_base<Entity>
{
public:
  using ID_t = long;
  using StatusCallback = std::function<void()>;

  friend class Field;
  friend class Component;
  friend class BattleSceneBase;

  enum class Shadow : char {
    none = 0,
    small,
    big,
    custom
  };

private:
  ID_t ID{}; /*!< IDs are used for tagging during battle & to identify entities in scripting. */
  static long numOfIDs; /*!< Internal counter to identify the next entity with. */
  int alpha{ 255 }; /*!< Control the transparency of an entity. */
  Component::ID_t lastComponentID{}; /*!< Entities keep track of new components to run through scene injection later. */
  bool hasSpawned{ false }; /*!< Flag toggles true when the entity is first placed onto the field. Calls OnSpawn(). */
  bool isUpdating{ false }; /*!< If an entity has updated once this frame, skip some update routines */
  bool manualDelete{ false }; /* HACK: prevent network pawns from deleting until they report their HP as zero */
  unsigned moveEventFrame{};
  unsigned frame{};
  float currJumpHeight{};
  float height{}; /*!< Height of the entity relative to tile floor. Used for visual effects like projectiles or for hitbox detection */
  EventBus::Channel channel; /*!< Our event bus channel to emit events */
  MoveEvent currMoveEvent{};
  VirtualInputState inputState;
  std::shared_ptr<SpriteProxyNode> shadow{ nullptr };
  std::shared_ptr<SpriteProxyNode> iceFx{ nullptr };
  std::shared_ptr<SpriteProxyNode> blindFx{ nullptr };
  Animation iceFxAnimation, blindFxAnimation;
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
  void ShiftShadow();
public:
  Entity();
  virtual ~Entity();

  /**
  * @brief Sets the tile for this entity and invokes OnSpawn() if this is the first time
  */
  void Spawn(Battle::Tile& start);

  bool HasSpawned();

  virtual void OnSpawn(Battle::Tile& start) { };

  void BattleStart();
  void BattleStop();

  /**
   * @brief If the Entity has been initialized already or not
   */
  bool HasInit();

  /**
   * @brief Initializes Entity since std::shared_from_this<>() cannot call virtual methods in constructors
   */
  virtual void Init();

  /**
   * @brief Free's all components
   */
  virtual void Cleanup();

  /**
   * @brief Entity::Update(dt) contains particular steps that gaurantee frame accuracy for child types
   */
  virtual void Update(double _elapsed);
  
  void RefreshShader();
  void draw(sf::RenderTarget& target, sf::RenderStates states) const final;

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
  bool RawMoveEvent(const MoveEvent& event, ActionOrder order = ActionOrder::voluntary);
  void HandleMoveEvent(MoveEvent& event, const ActionQueue::ExecutionType& exec);
  void ClearActionQueue();
  const float GetJumpHeight() const;
  virtual void FilterMoveEvent(MoveEvent& event) {};

  void ShowShadow(bool enabled);
  void SetShadowSprite(Shadow type);
  void SetShadowSprite(std::shared_ptr<sf::Texture> customShadow);

  /**
  * @brief By default, hitbox is available for discovery and hitting
  * @param enabled
  */
  void EnableHitbox(bool enabled);

  /**
  * @brief If true, hitbox can recieve hits from spells and can be discovered on the field
  * @return value of `hitboxEnabled`
  */
  const bool IsHitboxAvailable() const;

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
  void SetDrawOffset(const sf::Vector2f& offset);
  void SetDrawOffset(float x, float y);
  const sf::Vector2f GetDrawOffset() const;

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
  void SetField(std::shared_ptr<Field> _field);

  /**
   * @brief Gets the field pointer
   * Useful to check field state such as IsTimeFrozen()
   * @return Field pointer
   */
  std::shared_ptr<Field> GetField() const;

  bool IsOnField() const;

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

  int GetAlpha();

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
   * @brief Directly modify the entity's current move direction property. @see Entity::Move() 
   * Direction can be used for entities that use a single direction for linear movement.
   * e.g. Mettaur Waves, Bullet, Fishy
   * @param direction the new direction
   */
  void SetMoveDirection(Direction direction);
  
  /**
   * @brief Query the entity's move direction
   * @return The entity's current move direction
   */
  Direction GetMoveDirection();
  
  void SetFacing(Direction facing);
  Direction GetFacing();
  Direction GetFacingAway();

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
  void Erase();
  
  /**
   * @brief Query if an entity has been deleted but not erased this frame
   * @return true if flagged for deletion, false otherwise
   */
  bool IsDeleted() const;

  /**
  * @brief Query if an entity has been marked for removal
  * @return true if flagged, false otherwise
  */
  bool WillEraseEOF() const;

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
  std::shared_ptr<ComponentType> GetFirstComponent() const;

   /**
   * @brief Get all components that matches the exact Type
   * @return vector of specified components
   */
  template<typename ComponentType>
  std::vector<std::shared_ptr<ComponentType>> GetComponents() const;

  /**
* @brief Get all components that inherit BaseType
* @return vector of related components
*/
  template<typename BaseType>
  std::vector<std::shared_ptr<BaseType>> GetComponentsDerivedFrom() const;

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
  std::shared_ptr<ComponentType> CreateComponent(Args&&...);

  /**
  * @brief Attaches a component to an entity
  * @param c the component to add
  * @return Returns the component as a pointer of the common base class type
  */
  std::shared_ptr<Component> RegisterComponent(std::shared_ptr<Component> c);

  /**
   * @brief Frees one component with the same ID
   * @param ID ID of the component to remove
   * @warning may call components deconstructor if no one else owns it
   */
  void FreeComponentByID(Component::ID_t ID);

  /**
   * @brief Frees all attached components from an owner
   * All attached components owner pointer will be null
   * @warning may call components deconstructor if no one else owns it
   */
  void FreeAllComponents();

  /**
  * @brief Returns a channel in the EventBus
  * @return Returns the channel as const reference
  */
  const EventBus::Channel& EventChannel() const;

  /**
   * @brief Hit height is a size property that helps identify where something can be hit
   * @return float
   */
  virtual const float GetHeight() const;
  virtual void SetHeight(const float height);

  /**
   * @brief Elevation is the distance (Z) away from the grid. In 2D coordinates, this affects the final Y position.
   * @return float
   */
  const float GetElevation() const;
  void SetElevation(const float elevation);

  /**
  * @brief Get the virtual key presses states for this entity
  * @return VirtaulInputState
  */
  VirtualInputState& InputState();

  /**
  * The hit routine that happens for every character. Queues status properties and damage
  * to resolve at the end of the battle step.
  * @param props
  * @return Returns false  if IsPassthrough() is true (i-frames), otherwise true
  */
  const bool Hit(Hit::Properties props = Hit::DefaultProperties);

  virtual const bool UnknownTeamResolveCollision(const Entity& other) const;

  const bool HasCollision(const Hit::Properties& props);

  void ResolveFrameBattleDamage();

  /**
   * @brief Get the character's current health
   * @return 
   */
  virtual int GetHealth() const;
  
  /**
 * @brief Set the max health of the character
 * @param _health
 */
  void SetMaxHealth(int _health);

  /**
   * @brief Get the character's max (init) health
   * @return 
   */
  const int GetMaxHealth() const;

  /**
   * @brief Set the health of the character
   * @param _health
   */
  void SetHealth(int _health);

  /**
   * @brief Returns true if the counter flag is on
   */
  bool IsCounterable();

  /**
   * @brief Sets counter flag on
   * @param on
   * 
   * Used by counter frames. 
   */
  void ToggleCounter(bool on = true);
  
  void NeverFlip(bool enabled);

  /**
   * @brief Query the character's state is Stunned
   * @return true if character is currently stunned, false otherwise
   */
  bool IsStunned();

  /**
   * @brief Query the character's state is Rooted
   * @return true if character is currently rooted, false otherwise
   */
  bool IsRooted();

  /**
   * @brief Query the character's state is Ice Frozen
   * @return true if character is currently frozen from hitbox status effects, false otherwise
   */
  bool IsIceFrozen();

  /**
  * @brief Query the character's state is Blind
  * @return true if character is currently blind from hitbox status effects, false otherwise
  */
  bool IsBlind();

  /**
   * @brief Some characters allow others to move on top of them
   * @param enabled true, characters can share space, false otherwise
   */
  void ShareTileSpace(bool enabled);
  
  /**
   * @brief Query if entity can share space
   * @return true if shareTilespace is enabled, false otherwise
   */
  const bool CanShareTileSpace() const;

  /**
  * @brief Query if entity is pushable by tiles
  * @return true if canTilePush is enabled, false otherwise
  */
  const bool CanTilePush() const;

  /**
   * @brief Some characters can be moved around on the field by tiles
   * @param enabled
   */
  void EnableTilePush(bool enabled);

  /**
   * @brief Characters can have names
   * @param name
   */
  void SetName(std::string name);
  
  /**
   * @brief Get name of character
   * @return const std::string
   */
  const std::string GetName() const;

  /**
   * @brief Add defense rule for combat resolution
   * @param rule
   */
  void AddDefenseRule(std::shared_ptr<DefenseRule> rule);

  /**
   * @brief Removes the defense rule from this character's defense checks
   * @param rule
   */
  void RemoveDefenseRule(std::shared_ptr<DefenseRule> rule);

  /**
   * @brief Removes the defense rule from this character's defense checks
   * @param rule
   */
  void RemoveDefenseRule(DefenseRule* rule);

  /**
   * @brief Check if spell passes all defense checks. Updates the DefenseFrameStateJudge.
   * @param in attack
   * @param judge. The frame's current defense object with triggers and block statuses
   * @param in. The attack to test defenses against.
   * @param filter. Filter which types of defenses to check against by DefenseOrder value
   */
  void DefenseCheck(DefenseFrameStateJudge& judge, std::shared_ptr<Entity> in, const DefenseOrder& filter);

  /**
   * @brief Queried by Tile to highlight or not
   * @return Highlight mode
   */
  const Battle::TileHighlight GetTileHighlightMode() const;

  /**
   * @brief Describes how the spell attacks characters
   * @param _entity
   * 
   * This is where you would check for specific character types
   * if there are special conditions. Call Hit() on the entity
   * to deal damage.
   */
  virtual void Attack(std::shared_ptr<Entity> _entity) { };
  
  /**
  * @brief Describes what happens when this attack collides with a character, regardless of defenses
  *
  * If this function is called it does not gaurantee the attack will do damage to the character
  * Use this for visual effects like bubble pop
  */
  virtual void OnCollision(const std::shared_ptr<Entity> _entity) { };

  /**
   * @brief Toggle whether or not to highlight a tile
   * @param mode
   * 
   * FLASH - flicker every other frame
   * SOLID - Stay yellow
   * NONE  - this is the default. No effects are applied.
   */
  void HighlightTile(Battle::TileHighlight mode);

  /**
   * @brief Context for creation, carries properties that Hit::Properties will automatically copy
   * @param Hit::Context
   */
  void SetHitboxContext(Hit::Context context);

  /**
   * @brief Context for creation, carries properties that Hit::Properties will automatically copy
   * @return Hit::Context
   */
  Hit::Context GetHitboxContext();

  /**
   * @brief When an entity is hit, these are the hit properties it will use
   * @param props
   */
  void SetHitboxProperties(Hit::Properties props);
  
  /**
   * @brief Get the hitbox properties of this spell
   * @return const Hit::Properties
   */
  const Hit::Properties GetHitboxProperties() const;

  /**
  * @brief if true, other entities from the same aggressor cannot hit eachother
  */
  void IgnoreCommonAggressor(bool enable);

  /**
  * @brief Query if ICA is enabled or not
  */
  const bool WillIgnoreCommonAggressor() const;

  void SetPalette(const std::shared_ptr<sf::Texture>& palette);
  void StoreBasePalette(const std::shared_ptr<sf::Texture>& palette);
  std::shared_ptr<sf::Texture> GetPalette();
  std::shared_ptr<sf::Texture> GetBasePalette();

  void PrepareNextFrame();
  void RegisterStatusCallback(const Hit::Flags& flag, const StatusCallback& callback);


  // NOTE: Netplay hack until lockstep is perfect
  void ManualDelete();

protected:  
  Battle::Tile* tile{ nullptr }; /*!< Current tile pointer */
  Battle::Tile* previous{ nullptr }; /*!< Entities retain a previous pointer in case they need to be moved back */
  sf::Vector2f tileOffset{ 0,0 }; /*!< complete motion is captured by `tile_pos + tileOffset`*/
  sf::Vector2f moveStartPosition{ 0,0 }; /*!< Used internally when moving*/
  sf::Vector2f drawOffset{ 0,0 }; /*!< extra draw offset added by the programmer */
  std::weak_ptr<Field> field;
  Team team{};
  Element element{Element::none};
  ActionQueue actionQueue;
  frame_time_t moveStartupDelay{};
  std::optional<frame_time_t> moveEndlagDelay;
  frame_time_t stunCooldown{ 0 }; /*!< Timer until stun is over */
  frame_time_t rootCooldown{ 0 }; /*!< Timer until root is over */
  frame_time_t freezeCooldown{ 0 }; /*!< Timer until freeze is over */
  frame_time_t blindCooldown{ 0 }; /*!< Timer until blind is over */
  frame_time_t invincibilityCooldown{ 0 }; /*!< Timer until invincibility is over */
  bool counterable{};
  bool neverFlip{};
  bool hit{}; /*!< Was hit this frame */
  int counterFrameFlag{ 0 };
  sf::Color baseColor = sf::Color(255, 255, 255, 255);

  std::vector<std::shared_ptr<Component>> components; /*!< List of all components attached to this entity*/

  struct ComponentBucket {
    std::shared_ptr<Component> pending{ nullptr };
    enum class Status : unsigned {
      add,
      remove
    };

    Status action{ Status::add };
  };
  std::list<ComponentBucket> queuedComponents;

  const int GetMoveCount() const; /*!< Total intended movements made. Used to calculate rank*/

  /**
  * @brief Stun a character for maxCooldown seconds
  * @param maxCooldown
  * Used internally by class
  *
  */
  void Stun(frame_time_t maxCooldown);

  /**
  * @brief Stop a character from moving for maxCooldown seconds
  * @param maxCooldown
  * Used internally by class
  *
  */
  void Root(frame_time_t maxCooldown);

  /**
  * @brief Stop a character from moving for maxCooldown seconds
  * @param maxCooldown
  * Used internally by class
  *
  */
  void IceFreeze(frame_time_t maxCooldown);

  /**
  * @brief This entity should not see opponents for maxCooldown seconds
  * @param maxCooldown
  * Used internally by class
  *
  */
  void Blind(frame_time_t maxCooldown);

  /**
  * @brief Query if an attack successfully countered a Character
  * @return true if character is currently countered, false otherwise
  * Used internally by class
  */
  bool IsCountered();

  /**
  * Can override to provide custom behavior for when an Entity is countered
  */
  virtual void OnCountered() {}

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
  * @brief Performs some user-specified deletion behavior before removing from play
  *
  * Deleted entities are excluded from all battle attack steps however they
  * will be visually present and must be erased from field by calling Erase()
  */
  virtual void OnDelete() { };

  /**
   * @brief User-implemented callback for update hooks
   */
  virtual void OnUpdate(double _elapsed) {};

private:
  bool ignoreCommonAggressor{};
  bool hasInit{};
  bool isTimeFrozen{};
  bool ownedByField{}; /*!< Must delete the entity manual if not owned by the field. */
  bool passthrough{};
  bool floatShoe{};
  bool airShoe{};
  bool slidesOnTiles{ true }; /* by default everything responds to tile push/slide events */
  bool deleted{}; /*!< Used to trigger OnDelete() callback and exclude entity from most update routines*/
  bool flagForErase{}; /*!< Used to erase this entity from the field immediately */
  bool hitboxEnabled{ true };
  bool canTilePush{};
  bool canShareTile{}; /*!< Some characters can share tiles with others */
  bool slideFromDrag{}; /*!< In combat, slides from tiles are cancellable. Slide via drag is not. This flag denotes which one we're in. */
  bool swapPalette{ false };
  bool fieldStart{ false }; /*!< Used to signify if battle has started */
  int moveCount{}; /*!< Used by battle results */
  int health{};
  int maxHealth{};
  float elevation{}; // vector away from grid
  float counterSlideDelta{};
  double elapsedMoveTime{}; /*!< delta time since recent move event began */
  Battle::TileHighlight mode; /*!< Highlight occupying tile */
  Hit::Properties hitboxProperties; /*!< Hitbox properties used when an entity is hit by this attack */
  Direction direction{};
  Direction previousDirection{};
  Direction facing{};
  sf::Vector2f counterSlideOffset{ 0.f, 0.f }; /*!< Used when enemies delete on counter - they slide back */
  std::vector<std::shared_ptr<DefenseRule>> defenses; /*<! All defense rules sorted by the lowest priority level */
  std::string name; /*!< Name of the entity */

  // Statuses are resolved one property at a time
  // until the entire Flag object is equal to 0x00 None
  // Then we process the next status
  // This continues until all statuses are processed
  std::queue<CombatHitProps> statusQueue;

  sf::Shader* whiteout{ nullptr }; /*!< Flash white when hit */
  sf::Shader* stun{ nullptr };     /*!< Flicker yellow with luminance values when stun */
  sf::Shader* root{ nullptr };     /*!< Flicker black with luminance values when root */

  std::map<Hit::Flags, StatusCallback> statusCallbackHash;
  std::shared_ptr<sf::Texture> palette, basePalette;

    /**
   * @brief Used internally before moving and updates the start position
   */
  void UpdateMoveStartPosition();
};

template<typename ComponentType>
inline std::shared_ptr<ComponentType> Entity::GetFirstComponent() const
{
  for (auto& component : components) {
    if (typeid(*component) == typeid(ComponentType)) {
      return std::dynamic_pointer_cast<ComponentType>(component);
    }
  }

  return nullptr;
}

template<typename ComponentType>
inline std::vector<std::shared_ptr<ComponentType>> Entity::GetComponents() const
{
  auto res = std::vector<std::shared_ptr<ComponentType>>();

  for (auto& component : components) {
    if (typeid(*component) == typeid(ComponentType)) {
      res.push_back(std::dynamic_pointer_cast<ComponentType>(component));
    }
  }

  return res;
}


template<typename BaseType>
inline std::vector<std::shared_ptr<BaseType>> Entity::GetComponentsDerivedFrom() const
{
  auto res = std::vector<std::shared_ptr<BaseType>>();

  for (const auto& component : components) {
    auto cast = std::dynamic_pointer_cast<BaseType>(component);

    if (cast) {
      res.push_back(std::move(cast));
    }
  }

  return res;
}

template<typename Type>
inline bool Entity::IsA() {
  return (dynamic_cast<Type*>(this) != nullptr);
}

template<typename ComponentType, typename... Args>
inline std::shared_ptr<ComponentType> Entity::CreateComponent(Args&& ...args) {
  std::shared_ptr<ComponentType> c = std::make_shared<ComponentType>(std::forward<decltype(args)>(args)...);

  RegisterComponent(c);

  return c;
}
