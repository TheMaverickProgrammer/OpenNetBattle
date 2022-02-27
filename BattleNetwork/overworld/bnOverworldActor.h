#pragma once

#include "bnOverworldInteraction.h"
#include "bnOverworldSprite.h"
#include "bnOverworldSpatialMap.h"
#include "../bnTextureResourceManager.h"
#include "../bnSpriteProxyNode.h"
#include "../bnDirection.h"
#include "../bnAnimation.h"
#include "../bnDrawWindow.h"

#include <map>
#include <optional>

namespace Overworld {
  class Map;
  class SpatialMap;

  /**
    @brief Overworld::Actor class represents a character that can move, has animations for all movements, and has a name
  */
  class Actor : public WorldSprite {
  public:
    enum class MovementState : unsigned char {
      idle = 0,
      walking,
      running,
      size = 3
    };

    static void SetMissingTexture(std::shared_ptr<sf::Texture> texture);

  private:
    struct AnimStatePair {
      MovementState movement{};
      Direction dir{};
    };

    frame_time_t animProgress{}; //!< Used to sync movement animations
    float walkSpeed{ 80 }; //!< walk speed as pixels per second. Default 40px/s
    float runSpeed{ 140 }; //!< run speed as pixels per second. Default 70px/s
    bool playingCustomAnimation{};
    bool onStairs{ false };
    bool moveThisFrame{ false }; //!< Tells actor to move in accordance with their states or remain stationairy
    Direction heading{ Direction::down }; //!< the character's current heading
    Animation anim; //!< actor animation object
    std::vector<AnimStatePair> validStates; //!< Map of provided animations states
    MovementState state{}; //!< Current movement state (idle, moving, or running)
    sf::Vector2f pos{}; //!< 2d position in cartesian coordinates
    std::string name{}; //!< name of this character
    std::string lastStateStr{}; //!< String representing the last frame's state name
    std::function<void(std::shared_ptr<Actor> with, Interaction type)> onInteractFunc; //!< What happens if an actor interacts with the other
    float collisionRadius{ 1.0 }; //px
    bool solid{ true };
    bool collidesWithMap{ true };

    static std::shared_ptr<sf::Texture> missing; //!< Used when an animation state is missing
    std::shared_ptr<sf::Texture> currTexture; //!< Spritesheet (if any) we are using

    // aux functions
    void UpdateAnimationState(float elapsed);
    std::string FindValidAnimState(Direction dir, MovementState state);
    void UseMissingTexture();
    std::string MovementAnimStrPrefix(const MovementState& state);
    std::string DirectionAnimStrSuffix(const Direction& dir);

    const std::pair<bool, sf::Vector3f> CanMoveToEdgeStep(
      sf::Vector2f pos, 
      sf::Vector3f pos3D, 
      Map& map, 
      sf::Vector2f ray, 
      float layerRelativeElevation, 
      float maxElevationDiff, 
      sf::Vector2f currTileSpace);

  public:
    /**
    * @brief Construct a character with a name
    */
    Actor(const std::string& name);

    Actor(Actor&&) noexcept;

    /**
    * @brief Deconstructor
    */
    ~Actor();


    void draw(sf::RenderTarget& target, sf::RenderStates states) const override final;

    /**
    * @brief Make the character walk
    * @param dir direction to walk in
    * @param move if true then move (default), if false remain in place
    */
    void Walk(const Direction& dir, bool move = true);

    /**
    * @brief Make the character run
    * @param dir direction to run in
    * @param move if true then move (default), if false remain in place
    */
    void Run(const Direction& dir, bool move = true);

    /**
    * @brief Set the playback speed for animations
    * @param speed playback speed
    */
    void SetAnimationSpeed(float speed);

    /**
    * @brief Make the character idle and face a direction
    * @param dir direction to face. movement speed is applied as zero.
    */
    void Face(const Direction& dir);

    /**
    * @brief Make the character idle and face a direction
    * @param dir direction to face. movement speed is applied as zero.
    */
    void Face(const Actor& actor);

    /**
    * @brief Loads animation data from a file
    * @param path load the animations for this character from a file
    * @preconditions The animation file follows the standard for overworld states described below
    *
    * Each heading must end with a one letter suffix. For diagonal directions they are two letter suffixes.
    * For diagonal directions the y axis takes priority
    *   e.g. _YX would be _DL for "Down Right"
    *
    * Each movement state must prefix the heading with either "IDLE", "WALK", or "RUN"
    * e.g. "IDLE_L" would be "Idle left"
    *      "WALK_UL" would be "Walking Up left"
    *      "RUN_D" would be "Run Down"
    */
    void LoadAnimations(const Animation& animation);

    /**
    * @brief Plays an animation while the actor is idle
    * @param name name of the animation state
    */
    void PlayAnimation(const std::string& name, bool loop);

    /**
    * @brief Set the walk speed as pixels per second
    * @param speed Pixels per second
    */
    void SetWalkSpeed(float speed);

    /**
    * @brief Set the run speed as pixels per second
    * @param speed Pixels per second
    */
    void SetRunSpeed(float speed);


    /**
    * @brief Replaces the name of the actor with a new name
    * @param newName of the actor
    */
    void Rename(const std::string& newName);

    /**
    * @brief Fetch the name of this object
    * @return name of the character
    */
    const std::string GetName() const;

    /**
    * @brief Fetch the walking speed
    * @return walk speed as pixels per second
    */
    float GetWalkSpeed() const;

    /**
    * @brief Fetch the running speed
    * @return run speed as pixels per second
    */
    float GetRunSpeed() const;


    /**
    * @brief Fetch the current animation state
    * @return the current animation state
    */
    bool IsPlayingCustomAnimation() const;

    /**
    * @brief Fetch the current animation state
    * @return the current animation state
    */
    std::string GetAnimationString() const;

    /**
    * @brief Fetch the actor's heading
    * @return Direction
    */
    const Direction GetHeading() const;

    /**
    * @brief Return the position + heading by collisionRadius
    * @return vector in front of the actor's heading
    */
    sf::Vector2f PositionInFrontOf() const;

    /**
    * @brief Update the actor location and frame
    * @param elapsed Time elapsed in seconds
    *
    * This funtion will set the appropriate animation if it is not set to
    * reflect the current state values. It will also offset the actor x/y
    * based on walk or run speeds.
    */
    void Update(float elapsed, Map& map, SpatialMap& spatialMap);

    /**
    * @brief Watch for tile-based collisions with the current map
    * @param bool
    *
    * During Update() if the actor is intended to move, it will also check
    * against tiles to see if it should collide.
    */
    void CollideWithMap(bool);

    /**
    * @brief Convert direction flags into 2D mathematical vector objects with a length
    * @param dir The direction to convert
    * @param length the unit value to set the vector according to the direction
    * @return vector to be used in geometrical calculations
    */
    static sf::Vector2f MakeVectorFromDirection(Direction dir, float length);

    /**
    * @brief Convert 2D mathematical vector objects to a Direction
    * @param vector to convert
    * @param threshold. If the vector is below this value, the direction is considered Direction::none
    * @return Direction to be used in motion and state cases
    */
    static Direction MakeDirectionFromVector(const sf::Vector2f& vec);

    void SetSolid(bool solid);
    float GetCollisionRadius();
    void SetCollisionRadius(float radius);
    void SetInteractCallback(const std::function<void(std::shared_ptr<Actor>, Interaction)>& func);
    void Interact(const std::shared_ptr<Actor>& with, Interaction type);

    const std::optional<sf::Vector2f> CollidesWith(const Actor& actor, const sf::Vector2f& offset = sf::Vector2f{});
    const std::pair<bool, sf::Vector3f> CanMoveTo(Direction dir, MovementState state, float elapsed, Map& map, SpatialMap& spatialMap);
    const std::pair<bool, sf::Vector3f> CanMoveTo(sf::Vector2f pos, Map& map, SpatialMap& spatialMap);
  };
}
