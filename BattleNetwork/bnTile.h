/*! \brief Tiles represent where entities can move around on.
 * 
 * Without a tile, an entity cannot exist in the battle field.
 * Tiles have states such as LAVA, ICE, GRASS, NORMAL, CRACKED, BROKEN, and EMPTY.
 * Each state may affect an entity that lands on it or prevent other entities 
 * from occupying them.
 * 
 * The tiles are responsible for updating everything that exists on it. To achieve this
 * each type of entity has divided into different buckets: artifacts, spells, characters, 
 * and obstacles. Spells can affect characters and obstacles can affect both spells and 
 * other characters while artifacts are seen as purely visual and don't affect others.
 * 
 * When entities move they adopt new tiles and should remove themselves from their
 * previous tile. The tile will remove the entity from its appropriate bucket.
 */

#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <set>
#include <algorithm>
#include <functional>

using sf::RectangleShape;
using sf::Sprite;
using std::vector;
using std::find;
using std::set;

class Spell;
class Character;
class Obstacle;
class Artifact;

#include "bnEntity.h"
#include "bnSpriteProxyNode.h"
#include "bnTeam.h"
#include "bnResourcePaths.h"
#include "bnTileState.h"
#include "bnAnimation.h"
#include "bnDefenseRule.h"
#include "bnResourceHandle.h"

// forward decl
class Field;

namespace Battle {
  enum class TileHighlight : int {
    none = 0,
    flash = 1,
    solid = 2,
  };

  class Tile : public SpriteProxyNode, public ResourceHandle {
  public:

    friend Field;

    /**
    * \brief Base 1. Creates a tile at column x and row y.
    * 
    * If the tile column is <= 3, the tile's team is
    * automatically RED. Otherwise it is BLUE.
    */
    Tile(int _x, int _y);
    ~Tile();

    Tile(const Tile& rhs);
    Tile& operator=(const Tile& other);

    /**
     * @brief Query the current state of the tile
     * @return TileState
     */
    const TileState GetState() const;

    /**
     * @brief Assigns the tile's field pointer
     * @param _field
     */
    void SetField(std::weak_ptr<Field> field);

    /**
     * @brief Base 1. Returns the column of the tile.
     * @return Column number
     */
    const int GetX() const;
    
    /**
     * @brief Base 1. Returns the row of the tile.
     * @return Row number
     */
    const int GetY() const;

    Tile* GetTile(Direction dir, unsigned count);

    /**
     * @brief Query the tile's team
     * @return Team
     */
    const Team GetTeam() const;
    
    void HandleMove(std::shared_ptr<Entity> entity);

    void PerspectiveFlip(bool state);

    /**
     * @brief Change the tile's team if unoccupied
     * This will also change the color of the tile. 
     * If the tile has characters of another team on it,
     * it will not change. 
     * @param _team Team to change to
     * @param useFlicker if true, will change state & will flicker for flickerTeamCooldownLength milliseconds
     */
    void SetTeam(Team _team, bool useFlicker = false);

    void SetFacing(Direction facing);
    Direction GetFacing();

    /**
     * @brief Get the width of the tile sprite
     * @return width in pixels
     */
    float GetWidth() const;

    /**
     * @brief Get the number of entities occupying this tile
     * Size
     */
    const size_t GetEntityCount() const;
    
    /**
     * @brief Get the height of the tile sprite
     * @return height in pixels
     */
    float GetHeight() const;

    /**
     * @brief Change the tile's state if unoccupied or not broken nor empty
     * SetState tries to abide by the game's original rules
     * You cannot use a card to change the state of the tile if it's in a state
     * that cannot be modified such as being TileState::Empty
     * @param _state
     */
    void SetState(TileState _state);

    /**
     * @brief Texture updates to reflect the tile's current state
     */
    void RefreshTexture();

    /**
     * @brief Query if the tile is walkable e.g. not Broken nor Empty
     * @return true if walkable, false otherwise
     */
    bool IsWalkable() const;
    
    /**
     * @brief Query if the tile is cracked
     * @return true if state == TileState::cracked 
     */
    bool IsCracked() const;

    /**
     * @brief Query if the tile is an edge tile
     * @return true if x = {0, 7} or y = {0, 4}
     */
    bool IsEdgeTile() const;

    /**
    * @brief Query if the tile state is some kind of hole
    */
    bool IsHole() const;

    /**
     * @brief Query if the tile should be highlighted in battle
     * Some spells don't highlight tiles they occupy
     * The boolean is toggled in the Tile's Update() loop 
     * In the battle scene, if a tile is highlighted, it will use the
     * yellow luminocity shader to highlight the tile.
     * @return true if tile will be highlighted
     */
    bool IsHighlighted() const;

    /**
     * @brief will request a highlight style for one frame
     */
    void RequestHighlight(TileHighlight mode);

    /**
     * @brief Returns true if a character is standing on or has reserved this tile 
     * 
     * Only checks for character entity types
     * 
     * @param exclude list. Optional parameter. Any character matching the ID in this list are exluded from the final count and could change the result.
     */
    bool IsReservedByCharacter(std::vector<Entity::ID_t> exclude = {});

    /**
     * @brief Adds an entity and stores in an appropriate bucket if it doesn't already exist
     * @param _entity
     */
    void AddEntity(std::shared_ptr<Entity> _entity);

    /**
     * @brief If found, remove the entity from all buckets with the same ID
     * @param ID
     * @return true if the entity bucket has been modified and the entity removed
     */
    bool RemoveEntityByID(Entity::ID_t ID);
    
    /**
     * @brief Query if the given entity already occupies this tile
     * @param _entity to check
     * @return true if the entity is already in one of the buckets
     */
    bool ContainsEntity(const std::shared_ptr<Entity> _entity) const;

    /**
     * @brief Reserve this tile for an entity with ID 
     * An entity may be temporarily removed from play but will be back
     * later. This mechanic is seen with AntiDamage card. While
     * the entity is removed, the tile it was on must not be modified
     * by state changes and should be treated as occupied for all other checks.
     * When the entity is added again with AddEntity(), the reserve will be removed.
     * @param ID
     */
    void ReserveEntityByID(long ID);

    /**
     * @brief Query the tile for an entity type
     * @return true if the tile contains entity of type Type, false if no matches
     */
    template<class Type> bool ContainsEntityType();
    
    /**
     * @brief Queues an entity to attack to all other entities occupying this tile 
     * @param caller must be valid non-null entity
     */
    void AffectEntities(Entity& attacker);

    /**
     * @brief Updates all entities occupying this tile
     * @param _elapsed in seconds
     */
    void Update(Field& field, double _elapsed);

    /**
     * @brief Triggers this tile and all entities to behave as if time is frozen
     * @param state
     */
    void ToggleTimeFreeze(bool state);

    /**
     * @brief Notifies all entities that the battle is active
     */
    void BattleStart();

    /**
     * @brief Notifies all entities that the battle is inactive
     */
    void BattleStop();

    /**
    * @brief Assigns the textures used by the tile according to team, their states, and their animation files
    */
    void SetupGraphics(
      std::shared_ptr<sf::Texture> redTeam, 
      std::shared_ptr<sf::Texture> blueTeam, 
      std::shared_ptr<sf::Texture> unknown, 
      const Animation& anim
    );

    void HandleTileBehaviors(Field& field, Obstacle& obst);
    void HandleTileBehaviors(Field& field, Character& character);

    /**
     * @brief Query for multiple entities using a functor
     * This is useful for movement as well as card attacks 
     * to find specific enemy types under specific conditions
     * @param e Functor that takes in an entity and returns a boolean
     * @return returns a list of entities that returned true in the functor `e` 
     */
    std::vector<std::shared_ptr<Entity>> FindEntities(std::function<bool(std::shared_ptr<Entity>& e)> query);

    /**
     * @brief Query for multiple charactors using a functor
     * This is useful for movement as well as card attacks
     * to find specific enemy types under specific conditions
     * @param e Functor that takes in an character and returns a boolean
     * @return returns a list of characters that returned true in the functor `e`
     */
    std::vector<std::shared_ptr<Character>> FindCharacters(std::function<bool(std::shared_ptr<Character>& e)> query);

    /**
     * @brief Query for multiple charactors using a functor
     * This is useful for movement as well as card attacks
     * to find specific enemy types under specific conditions
     * @param e Functor that takes in an obstacle and returns a boolean
     * @return returns a list of obstacles that returned true in the functor `e`
     */
    std::vector<std::shared_ptr<Obstacle>> FindObstacles(std::function<bool(std::shared_ptr<Obstacle>& e)> query);

    /**
     * @brief Calculates and returns Manhattan-distance from this tile to the other
     */
    int Distance(Battle::Tile& other);

    /**
    * @brief Tile math easily returns tiles with the directional input enum type
    *
    * If the operand tile is an edge tile and the direction would proceed the edge tile,
    * then nullptr is returned
    */
    Tile* operator+(const Direction& dir);

    Tile* Offset(int x, int y);

  private:

    std::string GetAnimState(const TileState state);

    void PrepareNextFrame(Field& field);
    void ExecuteAllAttacks(Field& field);
    void UpdateSpells(Field& field, const double elapsed);
    void UpdateArtifacts(Field& field, const double elapsed);
    void UpdateCharacters(Field& field, const double elapsed);

    int x{}; /**< Column number*/
    int y{}; /**< Row number*/
    bool willHighlight{ false }; /**< Highlights when there is a spell occupied in this tile */
    bool isTimeFrozen{ false };
    bool isBattleOver{ false };
    bool isBattleStarted{ false };
    bool isPerspectiveFlipped{ false };
    float width{};
    float height{};
    static double teamCooldownLength;
    static double brokenCooldownLength;
    static double flickerTeamCooldownLength;
    double teamCooldown{};
    double brokenCooldown{};
    double flickerTeamCooldown{};
    double totalElapsed{};
    double elapsedBurnTime{};
    double burncycle{};
    std::weak_ptr<Field> fieldWeak;
    std::shared_ptr<sf::Texture> red_team_atlas, red_team_perm;
    std::shared_ptr<sf::Texture> blue_team_atlas, blue_team_perm;
    std::shared_ptr<sf::Texture> unk_team_atlas, unk_team_perm;
    TileHighlight highlightMode;
    Team team{Team::unset}, ogTeam{Team::unset};
    Direction facing{}, ogFacing{};
    TileState state;
    std::string animState; /**< reflects the tile's state - lookup animation from animation file */
    // Todo: use sets to avoid duplicate entries
    vector<Artifact*> artifacts; /**< Entity bucket for type Artifacts */
    vector<Entity*> spells; /**< Entity bucket for type Spells */
    vector<std::shared_ptr<Character>> characters; /**< Entity bucket for type Characters */

    set<Character*, EntityComparitor> deletingCharacters;

    vector<std::shared_ptr<Entity>> entities; /**< Entity bucket for looping over all entities **/

    set<Entity::ID_t> reserved; /**< IDs of entities reserving this tile*/
    vector<Entity::ID_t> queuedAttackers; /**< IDs of occupying attackers that have signaled they are to attack this frame */
    vector<Entity::ID_t> taggedAttackers; /**< IDs of occupying attackers that have already attacked this frame*/

    Animation animation;
    Animation volcanoErupt;
    double volcanoEruptTimer{ 4 }; // seconds
    std::shared_ptr<SpriteProxyNode> volcanoSprite;
  };

  template<class Type>
  bool Tile::ContainsEntityType() {
    for (vector<std::shared_ptr<Entity>>::iterator it = entities.begin(); it != entities.end(); ++it) {
      if (dynamic_cast<Type*>((*it).get()) != nullptr) {
        return true;
      }
    }

    return false;
  }

  // handles pointer to tiles and will return nullptr if lhs is null
  static Tile* operator+(Tile* lhs, Direction rhs) {
    if (lhs) {
      return *lhs + rhs;
    }

    return nullptr;
  }
}