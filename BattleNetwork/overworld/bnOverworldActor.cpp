#define _USE_MATH_DEFINES
#include "bnOverworldActor.h"
#include "bnOverworldMap.h"
#include "../bnMath.h"

std::shared_ptr<sf::Texture> Overworld::Actor::missing;

Overworld::Actor::Actor(const std::string& name) : name(name)
{

}

Overworld::Actor::Actor(Actor&& other) noexcept
{
  std::swap(animProgress, other.animProgress);
  std::swap(walkSpeed, other.walkSpeed);
  std::swap(runSpeed, other.runSpeed);
  std::swap(heading, other.heading);
  std::swap(anim, other.anim);
  std::swap(validStates, other.validStates);
  std::swap(state, other.state);
  std::swap(pos, other.pos);
  std::swap(name, other.name);
  std::swap(lastStateStr, other.lastStateStr);
  std::swap(onInteractFunc, other.onInteractFunc);
  std::swap(collisionRadius, other.collisionRadius);
}

Overworld::Actor::~Actor()
{
}

void Overworld::Actor::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  WorldSprite::draw(target, states);

  sf::RenderStates myStates = states;
  myStates.transform *= getTransform();

  sf::CircleShape circle;
  circle.setFillColor(sf::Color(255, 0, 0, 100));
  circle.setRadius(this->collisionRadius);
  circle.setOrigin(this->collisionRadius, this->collisionRadius);
  circle.setScale(std::sqrtf(2.), std::sqrtf(2.));
  circle.setPosition(0, -collisionRadius);
  target.draw(circle, myStates);

  sf::CircleShape circleBottom;
  circleBottom.setFillColor(sf::Color(0, 0, 255, 100));
  circleBottom.setRadius(this->collisionRadius);
  circleBottom.setOrigin(this->collisionRadius, this->collisionRadius);
  circleBottom.setScale(std::sqrtf(2.), std::sqrtf(2.f) / 2.f);
  target.draw(circleBottom, myStates);

  sf::RectangleShape yaxis;
  yaxis.setSize(sf::Vector2f(0.25f, 2.f));
  yaxis.setOrigin(0.25f*0.5f, 1.f);
  target.draw(yaxis, myStates);

  sf::RectangleShape xaxis;
  xaxis.setSize(sf::Vector2f(2.f, 0.25f));
  xaxis.setOrigin(1.f, 0.25f*0.5f);
  target.draw(xaxis, myStates);
}

void Overworld::Actor::Walk(const Direction& dir, bool move)
{
  if (dir == Direction::none) return;

  this->heading = dir;
  this->state = MovementState::walking;
  this->moveThisFrame = move;
}

void Overworld::Actor::Run(const Direction& dir, bool move)
{
  if (dir == Direction::none) return;

  this->heading = dir;
  this->state = MovementState::running;
  this->moveThisFrame = move;
}

void Overworld::Actor::SetAnimationSpeed(float speed)
{
  anim.SetPlaybackSpeed(speed);
}

void Overworld::Actor::Face(const Direction& dir)
{
  if (dir == Direction::none) return;

  this->heading = dir;
  this->state = MovementState::idle;
}

void Overworld::Actor::Face(const Actor& actor)
{
  auto direction = MakeDirectionFromVector(actor.getPosition() - getPosition());

  Face(direction);
}

void Overworld::Actor::LoadAnimations(const Animation& animation)
{
  anim = animation;

  validStates.clear();

  auto dir_list = {
    Direction::down, Direction::down_left, Direction::down_right,
    Direction::left, Direction::right, Direction::up,
    Direction::up_left, Direction::up_right
  };

  auto state_list = {
    MovementState::idle, MovementState::running, MovementState::walking
  };

  auto loadDirectionalAnimationThunk = [=](Direction dir, MovementState state) {
    std::string str = MovementAnimStrPrefix(state) + "_" + DirectionAnimStrSuffix(dir);

    if (animation.HasAnimation(str)) {
      validStates.push_back({ state, dir });
      return true;
    }

    return false;
  };

  for (auto dir : dir_list) {
    for (auto state : state_list) {
      loadDirectionalAnimationThunk(dir, state);
    }
  }

  UpdateAnimationState(0);
}

void Overworld::Actor::PlayAnimation(const std::string& name, bool loop)
{
  if (!anim.HasAnimation(name)) {
    return;
  }

  anim << name;
  playingCustomAnimation = true;

  if (loop) {
    anim << Animator::Mode::Loop;
  }
  else {
    // after the animation completes, set playingCustomAnimation to false
    auto frame = (int)anim.GetFrameList(name).GetFrameCount();

    auto callback = [this] {
      playingCustomAnimation = false;
    };

    anim.AddCallback(frame, callback, true);
  }
}

void Overworld::Actor::SetWalkSpeed(float speed)
{
  this->walkSpeed = speed;
}

void Overworld::Actor::SetRunSpeed(float speed)
{
  this->runSpeed = speed;
}

void Overworld::Actor::Rename(const std::string& newName)
{
  this->name = newName;
}

const std::string Overworld::Actor::GetName() const
{
  return this->name;
}

float Overworld::Actor::GetWalkSpeed() const
{
  return walkSpeed;
}

float Overworld::Actor::GetRunSpeed() const
{
  return runSpeed;
}

bool Overworld::Actor::IsPlayingCustomAnimation() const
{
  return playingCustomAnimation;
}

std::string Overworld::Actor::GetAnimationString() const
{
  return anim.GetAnimationString();
}

const Direction Overworld::Actor::GetHeading() const
{
  return this->heading;
}

sf::Vector2f Overworld::Actor::PositionInFrontOf() const
{
  return getPosition() + UnitVector(GetHeading()) * collisionRadius;
}

void Overworld::Actor::Update(float elapsed, Map& map, SpatialMap& spatialMap)
{
  auto texture = getTexture();

  if (texture != Actor::missing) {
    currTexture = texture;
  }

  UpdateAnimationState(elapsed);

  if (state != MovementState::idle && moveThisFrame) {
    auto& [_, new_pos] = CanMoveTo(GetHeading(), state, elapsed, map, spatialMap);

    // We don't care about success or not, update the best position
    Set3DPosition(new_pos);

    moveThisFrame = false;
  }
}

void Overworld::Actor::SetMissingTexture(std::shared_ptr<sf::Texture> texture)
{
  Actor::missing = texture;
}

void Overworld::Actor::CollideWithMap(bool collide)
{
  collidesWithMap = collide;
}

sf::Vector2f Overworld::Actor::MakeVectorFromDirection(Direction dir, float length)
{
  sf::Vector2f offset{};

  const auto& [a, b] = Split(dir);

  auto updateOffsetThunk = [&offset, length](const Direction& dir) mutable {
    if (dir == Direction::left) {
      offset.x -= length;
    }
    else if (dir == Direction::right) {
      offset.x += length;
    }
    else if (dir == Direction::up) {
      offset.y -= length;
    }
    else if (dir == Direction::down) {
      offset.y += length;
    }
  };

  updateOffsetThunk(a);
  updateOffsetThunk(b);

  return offset;
}

Direction Overworld::Actor::MakeDirectionFromVector(const sf::Vector2f& vec)
{
  if (vec.x == 0 && vec.y == 0) {
    return Direction::none;
  }

  Direction first = vec.x < 0 ? Direction::left : Direction::right;
  Direction second = vec.y < 0 ? Direction::up : Direction::down;

  // using slope to calculate direction, graph if you want to take a look
  auto ratio = std::fabs(vec.y) / std::fabs(vec.x);

  if (ratio < 1.f / 2.f) {
    return first;
  }
  else if (ratio > 2.f) {
    return second;
  }

  return Join(first, second);
}

void Overworld::Actor::SetSolid(bool solid)
{
  this->solid = solid;
}

float Overworld::Actor::GetCollisionRadius()
{
  return collisionRadius;
}

void Overworld::Actor::SetCollisionRadius(float radius)
{
  this->collisionRadius = radius;
}

void Overworld::Actor::SetInteractCallback(const std::function<void(std::shared_ptr<Actor>, Interaction)>& func)
{
  this->onInteractFunc = func;
}

void Overworld::Actor::Interact(const std::shared_ptr<Actor>& with, Interaction type)
{
  if (this->onInteractFunc) {
    this->onInteractFunc(with, type);
  }
}

const std::optional<sf::Vector2f> Overworld::Actor::CollidesWith(const Actor& actor, const sf::Vector2f& offset)
{
  auto delta = (getPosition() + offset) - actor.getPosition();
  float distance = Hypotenuse(delta);
  float sumOfRadii = actor.collisionRadius + collisionRadius;

  if (distance > sumOfRadii) {
    return {};
  }

  auto delta_unit = sf::Vector2f(delta.x / distance, delta.y / distance);

  // suggested point of collision is the edge of the circle
  auto vec = actor.getPosition() + (delta_unit * sumOfRadii);

  // return collision status and point of potential intersection
  return vec;
}

const std::pair<bool, sf::Vector3f> Overworld::Actor::CanMoveTo(Direction dir, MovementState state, float elapsed, Map& map, SpatialMap& spatialMap)
{
  float px_per_s = 0;
  float elevation = GetElevation();
  bool onStairs = elevation != std::floor(elevation);

  if (onStairs || state == MovementState::running) {
    px_per_s = GetRunSpeed() * elapsed;
  }
  else if (state == MovementState::walking) {
    px_per_s = GetWalkSpeed() * elapsed;
  }

  auto currPos = getPosition();

  auto offset = MakeVectorFromDirection(dir, px_per_s);
  offset.x = std::round(offset.x);
  offset.y = std::round(offset.y);

  const auto& [first, second] = Split(dir);

  // The referenced games did not use isometric coordinates
  // and moved uniformly in screen-space.
  // These 2 cases are quicker in isometric space because it is extremely 
  // negative or positive for both x and y.
  bool shouldNormalize = dir == Direction::up_right || dir == Direction::down_left;
  auto normalizedOffset = offset;

  if (shouldNormalize) {
    // If we are clearly moving in these extreme directions, nerf the result
    normalizedOffset *= 0.5f;
  }

  auto newPos = currPos + normalizedOffset;
  auto [canMove, firstPositionResult] = CanMoveTo(newPos, map, spatialMap);

  if (!canMove) {
    // test alternate directions, this provides a sliding effect
    sf::Vector2f altOffsetA;
    sf::Vector2f altOffsetB;

    if (second != Direction::none) {
      altOffsetA.x = offset.x;
      altOffsetB.y = offset.y;
    }
    else if (first == Direction::left || first == Direction::right) {
      // normalized diagonal
      altOffsetA.x = offset.x * 0.5f;
      altOffsetA.y = altOffsetA.x;
      // flipped y
      altOffsetB.x = altOffsetA.x;
      altOffsetB.y = -altOffsetA.y;
    }
    else {
      // normalized diagonal
      altOffsetA.x = offset.y * 0.5f;
      altOffsetA.y = altOffsetA.x;
      // flipped x
      altOffsetB.x = -altOffsetA.x;
      altOffsetB.y = altOffsetA.y;
    }

    auto [canMoveOffsetX, offsetXResult] = CanMoveTo(currPos + altOffsetA, map, spatialMap);

    if (canMoveOffsetX) {
      return { canMoveOffsetX, offsetXResult };
    }

    auto [canMoveOffsetY, offsetYResult] = CanMoveTo(currPos + altOffsetB, map, spatialMap);

    if (canMoveOffsetY) {
      return { canMoveOffsetY, offsetYResult };
    }
  }

  return { canMove, firstPositionResult };
}

static bool SameTile(sf::Vector2f a, sf::Vector2f b) {
  return std::floor(a.x) == std::floor(b.x) && std::floor(a.y) == std::floor(b.y);
}

static int GetTargetLayer(Overworld::Map& map, int currentLayer, float layerRelativeElevation, sf::Vector2f currentTilePos, sf::Vector2f targetTilePos) {
  const auto offset = targetTilePos - currentTilePos;
  const float offsetLength = std::sqrt(std::pow(offset.x, 2.0f) + std::pow(offset.y, 2.0f));
  const float elevationPadding = 1.0f / map.GetTileSize().y * 2.0f;
  const float maxElevationDiff = offsetLength + elevationPadding;

  const auto layerBelow = currentLayer - 1;

  if (layerBelow >= 0 && layerBelow < map.GetLayerCount() && map.IgnoreTileAbove(targetTilePos.x, targetTilePos.y, layerBelow)) {
    return layerBelow;
  }
  else if (layerRelativeElevation > 1 - maxElevationDiff && !SameTile(currentTilePos, targetTilePos)) {
    // if we're at the top of stairs, target the layer above
    return currentLayer + 1;
  }

  return currentLayer;
}

const std::pair<bool, sf::Vector3f> Overworld::Actor::CanMoveTo(sf::Vector2f newPos, Map& map, SpatialMap& spatialMap) {
  const sf::Vector2f& currPos = getPosition();
  sf::Vector3f currPos3D = sf::Vector3f(currPos.x, currPos.y, GetElevation());
  float layerRelativeElevation = currPos3D.z - std::floor(currPos3D.z);

  sf::Vector2f offset = newPos - currPos;
  float offsetLength = std::sqrt(std::pow(offset.x, 2.0f) + std::pow(offset.y, 2.0f));
  const float maxElevationDiff = (offsetLength + 1.0f) / map.GetTileSize().y * 2.0f;

  sf::Vector2f currPosTileSpace = map.WorldToTileSpace(currPos);
  sf::Vector2f newPosTileSpace = map.WorldToTileSpace(newPos);

  // check if there's stairs below, if there is, target that layer for newPos3D
  int newLayer = GetTargetLayer(map, GetLayer(), layerRelativeElevation, currPosTileSpace, newPosTileSpace);
  sf::Vector3f newPos3D = sf::Vector3f(newPos.x, newPos.y, map.GetElevationAt(newPosTileSpace.x, newPosTileSpace.y, newLayer));

  /**
  * Check if colliding with the map
  * This should be a cheaper check to do first
  * before checking neighboring actors
  */

  if (collidesWithMap && newLayer >= 0 && map.GetLayerCount() > newLayer) {
    if (std::fabs(newPos3D.z - currPos3D.z) > maxElevationDiff) {
      // height difference is too great
      return { false, currPos3D };
    }

    // detect collision at the edge of the collisionRadius
    sf::Vector2f ray_unit = sf::Vector2f(offset.x / offsetLength, offset.y / offsetLength);
    sf::Vector2f ray1 = ray_unit * collisionRadius;

    float theta = 90.f * M_PI / 180.f;
    sf::Vector2f perp = sf::Vector2f(cos(theta)*ray1.x, sin(theta)*ray1.y);

    sf::Vector2f ray2 = ray1 + perp;
    sf::Vector2f ray3 = ray1 - perp;
    
    auto [canMove1, _1] = CanMoveToEdgeStep(currPos, currPos3D, map, ray1, layerRelativeElevation, maxElevationDiff, currPosTileSpace);
    auto [canMove2, _2] = CanMoveToEdgeStep(currPos, currPos3D, map, ray2, layerRelativeElevation, maxElevationDiff, currPosTileSpace);
    auto [canMove3, _3] = CanMoveToEdgeStep(currPos, currPos3D, map, ray3, layerRelativeElevation, maxElevationDiff, currPosTileSpace);

    const sf::Vector2i tileSize = map.GetTileSize();

    if (!(canMove1 && canMove2 && canMove3)) {
      sf::Vector2f alpha = sf::Vector2f(currPosTileSpace.x - std::floor(currPosTileSpace.x), currPosTileSpace.y - std::floor(currPosTileSpace.y));
      sf::Vector2f relativeTilePos = sf::Vector2f(alpha.x * tileSize.x, alpha.y * tileSize.y);
      sf::Vector2f intersection = relativeTilePos + offset;

      intersection.x = std::fmin(std::fmax(intersection.x, 0.f), (float)tileSize.x);
      intersection.y = std::fmin(std::fmax(intersection.y, 0.f), (float)tileSize.y);

      intersection += -ray1;

      sf::Vector2f delta =  intersection - relativeTilePos;
      newPos3D = currPos3D;
      newPos3D.x += delta.x;
      newPos3D.y += delta.y;

      return { false, currPos3D };
    }
  }

  /**
  * If we can move forward, check neighboring actors
  */
  if (solid) {
    for (const auto& actor : spatialMap.GetNeighbors(*this)) {
      if (actor.get() == this || !actor->solid) continue;

      float elevationDifference = std::fabs(actor->GetElevation() - newPos3D.z);

      if (elevationDifference > 0.1f) continue;

      std::optional<sf::Vector2f> collision = CollidesWith(*actor, offset);

      if (collision) {
        // push the ourselves out of the other actor
        // use current position to prevent sliding off map
        sf::Vector2f delta = currPos - actor->getPosition();
        float distance = Hypotenuse(delta);
        sf::Vector2f delta_unit = sf::Vector2f(delta.x / distance, delta.y / distance);
        float sumOfRadii = collisionRadius + actor->GetCollisionRadius();
        sf::Vector2f outPos = actor->getPosition() + (delta_unit * sumOfRadii);

        sf::Vector2f outPosInTileSpace = map.WorldToTileSpace(outPos);
        float elevation = map.GetElevationAt(outPosInTileSpace.x, outPosInTileSpace.y, newLayer);

        return { false, { outPos.x, outPos.y, elevation } };
      }
    }
  }

  // There were no obstructions
  return { true, newPos3D };
}

void Overworld::Actor::UpdateAnimationState(float elapsed) {
  if (this->state == MovementState::idle && playingCustomAnimation) {
    if (getTexture() != Actor::missing) {
      // update anim if the animation is valid
      // if the texture is currently the missing texture, then we decided it's not valid at some earlier point
      anim.Update(elapsed, getSprite());
    }
    return;
  }

  playingCustomAnimation = false;
  std::string stateStr;

  auto findValidAnimThunk = [this](const MovementState& state) {
    return FindValidAnimState(this->heading, state);
  };

  auto state_list = { MovementState::running, MovementState::walking, MovementState::idle };

  for (auto item : state_list) {
    // begin with our desired state and work our way through the list...
    // only look down in the state list for valid state alternatives
    // i.e. no RUN animations but there are valid animations for WALK!
    if (item <= this->state) {
      stateStr = findValidAnimThunk(item);

      // If we have something (non-empty string), exit the loop!
      if (anim.HasAnimation(stateStr)) {
        break;
      }
    }
  }

  // we may have exhausted the possible animations to substitute in the previous for-loop
  // so we need to check again that the new state string is valid...
  if (!anim.HasAnimation(stateStr)) {
    UseMissingTexture();
    return;
  }

  auto texture = getTexture();

  if (texture == Actor::missing && currTexture) {
    this->setTexture(currTexture, true);
  }

  // we're going to be syncing the time so this is required if changing sprites
  anim << stateStr << Animator::Mode::Loop;

  animProgress += from_seconds(elapsed * std::abs(anim.GetPlaybackSpeed()));

  if (!lastStateStr.empty()) {
    anim.SyncTime(animProgress);
    anim.Refresh(getSprite());
  }

  if (lastStateStr != stateStr) {
    animProgress = frames(0); // reset animation
    anim.SyncTime(animProgress);

    // we have changed states
    anim.Refresh(getSprite());
    lastStateStr = stateStr;
  }
}

void Overworld::Actor::UseMissingTexture() {
  // Do we have a valid 'missing' texture?
  if (!Actor::missing) {
    return;
  }

    // try and preserve what texture we currently have
  auto texture = getTexture();

  if (texture != Actor::missing) {
    currTexture = texture;
  }

  sf::Vector2u size = missing->getSize();
  sf::Vector2f originf = sf::Vector2f(size) * 0.5f;
  this->setTexture(missing, true);
  this->getSprite().setOrigin(originf);
}

std::string Overworld::Actor::FindValidAnimState(Direction dir, MovementState state)
{
  // Some animations do not need to be provided (like cardinal-facing left or right)
  // when others can be substituted
  //
  // If we do not have a matching state animation, find the next best one:
  // 1. Try using another vertical direction
  // 2. Try using another horizontal direction
  // Else bail

  auto hasAnimStateThunk = [this](Direction dir, MovementState state) {
    auto iter = std::find_if(validStates.begin(), validStates.end(),
      [dir, state](const AnimStatePair& pair) {
      return std::tie(dir, state) == std::tie(pair.dir, pair.movement);
    });

    return std::tuple{ iter != validStates.end(), dir, state };
  };

  auto attemptThunk = [](const std::vector<std::tuple<bool, Direction, MovementState>>& ts) {
    for (auto& item : ts) {
      if (std::get<0>(item)) {
        return AnimStatePair{ std::get<2>(item), std::get<1>(item) };
      }
    }

    // bad state
    return AnimStatePair{ MovementState::size, Direction::none };
  };

  auto str = MovementAnimStrPrefix(state) + "_" + DirectionAnimStrSuffix(Isometric(dir));

  if (anim.HasAnimation(str) == false) {
    auto [ud, lr] = Split(Isometric(dir));

    std::map<Direction, std::vector<std::tuple<bool, Direction, MovementState>>> attempts = {
      {
        Direction::left,
        {
          hasAnimStateThunk(Direction::up_left, state),
          hasAnimStateThunk(Direction::down_left, state)
        }
      },

      {
        Direction::up_left,
        {
          hasAnimStateThunk(Direction::left, state),
          hasAnimStateThunk(Direction::up, state)
        }
      },

      {
        Direction::up,
        {
          hasAnimStateThunk(Direction::up_left, state),
          hasAnimStateThunk(Direction::up_right, state)
        }
      },

      {
        Direction::up_right,
        {
          hasAnimStateThunk(Direction::right, state),
          hasAnimStateThunk(Direction::up, state)
        }
      },

      {
        Direction::right,
        {
          hasAnimStateThunk(Direction::up_right, state),
          hasAnimStateThunk(Direction::down_right, state)
        }
      },

      {
        Direction::down_right,
        {
          hasAnimStateThunk(Direction::right, state),
          hasAnimStateThunk(Direction::down, state)
        }
      },

      {
        Direction::down,
        {
          hasAnimStateThunk(Direction::down_left, state),
          hasAnimStateThunk(Direction::down_right, state)
        }
      },

      {
        Direction::down_left,
        {
          hasAnimStateThunk(Direction::left, state),
          hasAnimStateThunk(Direction::down, state)
        }
      }
    };

    AnimStatePair pair = attemptThunk(attempts[Isometric(dir)]);

    // valid pair was found
    if (pair.dir != Direction::none) {
      return MovementAnimStrPrefix(pair.movement) + "_" + DirectionAnimStrSuffix(pair.dir);
    }
  }

  return str;
}

std::string Overworld::Actor::MovementAnimStrPrefix(const MovementState& state)
{
  switch (state) {
  case MovementState::idle:
    return "IDLE";
  case MovementState::running:
    return "RUN";
  case MovementState::walking:
    return "WALK";
  }

  return "IDLE"; // default is IDLE
}

std::string Overworld::Actor::DirectionAnimStrSuffix(const Direction& dir)
{
  switch (dir) {
  case Direction::up:
    return "U";
  case Direction::left:
    return "L";
  case Direction::down:
    return "D";
  case Direction::right:
    return "R";
  case Direction::up_left:
    return "UL";
  case Direction::up_right:
    return "UR";
  case Direction::down_left:
    return "DL";
  case Direction::down_right:
    return "DR";
  default:
    return "D"; // silence warnings
  }

  return "D"; // default is down
}

const std::pair<bool, sf::Vector3f> 
Overworld::Actor::CanMoveToEdgeStep(
  sf::Vector2f pos, 
  sf::Vector3f pos3D, 
  Map& map, 
  sf::Vector2f ray, 
  float layerRelativeElevation, 
  float maxElevationDiff, 
  sf::Vector2f currTileSpace)
{
  sf::Vector2f edgeTileSpace = map.WorldToTileSpace(pos + ray);
  int edgeLayer = GetTargetLayer(map, GetLayer(), layerRelativeElevation, currTileSpace, edgeTileSpace);
  float edgeElevation = map.GetElevationAt(edgeTileSpace.x, edgeTileSpace.y, edgeLayer);

  if (
    !map.CanMoveTo(edgeTileSpace.x, edgeTileSpace.y, edgeElevation, edgeLayer) || // can't move
    std::fabs(pos3D.z - edgeElevation) > maxElevationDiff // height difference is too great at edge
    ) {
    return { false, pos3D };
  }
}
