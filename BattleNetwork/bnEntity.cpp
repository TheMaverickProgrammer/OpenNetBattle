#include "bnEntity.h"
#include "bnComponent.h"
#include "bnTile.h"
#include "bnField.h"
#include <cmath>
#include <Swoosh/Ease.h>

long Entity::numOfIDs = 0;

bool EntityComparitor::operator()(Entity* f, Entity* s) const
{
  return f->GetID() < s->GetID();
}

// First entity ID begins at 1
Entity::Entity() : 
  elapsedMoveTime(0),
  lastComponentID(0),
  height(0),
  moveCount(0),
  channel(nullptr)
{
  ID = ++Entity::numOfIDs;

  using namespace std::placeholders;
  auto handler = std::bind(&Entity::HandleMoveEvent, this, _1, _2);
  actionQueue.RegisterType<MoveEvent>(ActionTypes::movement, handler);
}

Entity::~Entity() {
  this->FreeAllComponents();
}

void Entity::SortComponents()
{
  // Newest components appear first in the list for easy referencing
  std::sort(components.begin(), components.end(), [](Component* a, Component* b) { return a->GetID() > b->GetID(); });
}

void Entity::ClearPendingComponents()
{
  queuedComponents.clear();
}

void Entity::ReleaseComponentsPendingRemoval()
{
  // `delete` may kick off deconstructors that Eject() other components
  auto copy = queuedComponents;

  // Remove the component from our list
  for (auto&& bucket : copy) {
    if (bucket.action != ComponentBucket::Status::remove) continue;

    auto iter = std::find(components.begin(), components.end(), bucket.pending);

    if (iter != components.end()) {
      Component* component = *iter;
      components.erase(iter);
      delete component;
    }
  }
}

void Entity::InsertComponentsPendingRegistration()
{
  bool sort = queuedComponents.size();

  for (auto&& bucket : queuedComponents) {
    if (bucket.action == ComponentBucket::Status::add) {
      components.push_back(bucket.pending);
    }
  }

  sort ? SortComponents() : void(0);
}

void Entity::UpdateMovement(double elapsed)
{
  // Only move if we have a valid next tile pointer
  auto next = currMoveEvent.dest;
  if (next) {
    if (currMoveEvent.onBegin) {
      currMoveEvent.onBegin();
      currMoveEvent.onBegin = nullptr;
    }

    elapsedMoveTime += elapsed;

    if (from_seconds(elapsedMoveTime) > currMoveEvent.delayFrames) {
      // Get a value from 0.0 to 1.0
      float duration = seconds_cast<float>(currMoveEvent.deltaFrames);
      float delta = swoosh::ease::linear(static_cast<float>(elapsedMoveTime - currMoveEvent.delayFrames.asSeconds().value), duration, 1.0f);
      
      sf::Vector2f pos = moveStartPosition;
      sf::Vector2f tar = next->getPosition();

      // Interpolate the sliding position from the start position to the end position
      auto interpol = tar * delta + (pos * (1.0f - delta));
      tileOffset = interpol - pos;

      // Once halfway, the mmbn entities switch to the next tile
      // and the slide position offset must be readjusted 
      if (delta >= 0.5f) {
        // conditions of the target tile may change, ensure by the time we switch
        if (CanMoveTo(next)) {
          if (tile != next) {
            AdoptNextTile();
          }

          // Adjust for the new current tile, begin halfway approaching the current tile
          tileOffset = -tar + pos + tileOffset;
        }
        else {
          // Slide back into the origin tile if we can no longer slide to the next tile
          moveStartPosition = next->getPosition();
          currMoveEvent.dest = tile;

          tileOffset = -tar + pos + tileOffset;
        }
      }

      float heightElapsed = static_cast<float>(elapsedMoveTime - currMoveEvent.delayFrames.asSeconds().value);
      float heightDelta = swoosh::ease::wideParabola(heightElapsed, duration, 1.0f);
      currJumpHeight = (heightDelta * currMoveEvent.height);
      tileOffset.y -= currJumpHeight;
      
      // When delta is 1.0, the slide duration is complete
      if (delta == 1.0f)
      {
        // Slide or jump is complete, clear the tile offset used in those animations
        tileOffset = { 0, 0 };

        // Now that we have finished moving across panels, we must wait out endlag
        frame_time_t lastFrame = currMoveEvent.delayFrames + currMoveEvent.deltaFrames + currMoveEvent.endlagFrames;
        if (from_seconds(elapsedMoveTime) > lastFrame) {
          Battle::Tile* prevTile = previous;
          FinishMove(); // mutates `previous` ptr
          previousDirection = direction;
          Battle::Tile* currTile = GetTile();

          // If we slide onto an ice block and we don't have float shoe enabled, slide
          if (tile->GetState() == TileState::ice && !HasFloatShoe()) {
            // calculate our new entity's position
            UpdateMoveStartPosition();

            if (prevTile->GetX() > currTile->GetX()) {
              next = GetField()->GetAt(GetTile()->GetX() - 1, GetTile()->GetY());
              previousDirection = Direction::left;
            }
            else if (prevTile->GetX() < currTile->GetX()) {
              next = GetField()->GetAt(GetTile()->GetX() + 1, GetTile()->GetY());
              previousDirection = Direction::right;
            }
            else if (prevTile->GetY() < currTile->GetY()) {
              next = GetField()->GetAt(GetTile()->GetX(), GetTile()->GetY() + 1);
              previousDirection = Direction::down;
            }
            else if (prevTile->GetY() > currTile->GetY()) {
              next = GetField()->GetAt(GetTile()->GetX(), GetTile()->GetY() - 1);
              previousDirection = Direction::up;
            }

            // If the next tile is not available, not ice, or we are ice element, don't slide
            bool notIce = (next && tile->GetState() != TileState::ice);
            bool cannotMove = (next && !CanMoveTo(next));
            bool weAreIce = (GetElement() == Element::ice);
            bool cancelSlide = (notIce || cannotMove || weAreIce);

            if (slidesOnTiles && !cancelSlide) {
              MoveEvent event = { frames(3), frames(0), frames(0), 0, tile + previousDirection };
              RawMoveEvent(event, ActionOrder::immediate);
            }
          }
          else {
            // Invalidate the next tile pointer
            next = nullptr;
          }
        }
      }
    }
  }
  else {
    // If we don't have a valid next tile pointer or are not sliding,
    // Keep centered in the current tile with no offset
    tileOffset = sf::Vector2f(0, 0);
    elapsedMoveTime = 0;
  }
}

void Entity::SetFrame(unsigned frame)
{
  this->frame = frame;
}

void Entity::Spawn(Battle::Tile & start)
{
  if (!hasSpawned) {
    OnSpawn(start);
  }

  hasSpawned = true;
}

const float Entity::GetHeight() const {
  return height;
}

void Entity::SetHeight(const float height) {
  Entity::height = std::fabs(height);
}

const bool Entity::IsSuperEffective(Element _other) const {
  switch(GetElement()) {
    case Element::aqua:
        return _other == Element::elec;
        break;
    case Element::fire:
        return _other == Element::aqua;
        break;
    case Element::wood:
        return _other == Element::fire;
        break;
    case Element::elec:
        return _other == Element::wood;
        break;
  }
    
  return false;
}

void Entity::Update(double _elapsed) {
  isUpdating = true;

  actionQueue.Process();

  UpdateMovement(_elapsed);

  sf::Uint8 alpha = getSprite().getColor().a;
  for (auto* child : GetChildNodes()) {
    auto* sprite = dynamic_cast<SpriteProxyNode*>(child);
    if (sprite) {
      sf::Color color = sprite->getColor();
      sprite->setColor(sf::Color(color.r, color.g, color.b, alpha));
    }
  }

  // Update all components
  auto iter = components.begin();
  while (iter != components.end()) {
    // respectfully only update local components
    // anything shared with the battle scene needs to update those components
    if ((*iter)->Lifetime() == Component::lifetimes::local) {
      (*iter)->Update(_elapsed);
    }
    iter = std::next(iter);
  }

  ReleaseComponentsPendingRemoval();
  InsertComponentsPendingRegistration();
  ClearPendingComponents();

  isUpdating = false;
}

void Entity::SetAlpha(int value)
{
  alpha = value;
  sf::Color c = getColor();
  c.a = alpha;

  setColor(c);
}

bool Entity::Teleport(Battle::Tile* dest, ActionOrder order, std::function<void()> onBegin) {
  if (dest && CanMoveTo(dest)) {
    MoveEvent event = { 0, moveStartupDelay, moveEndlagDelay, 0, dest, onBegin };
    actionQueue.Add(event, order, ActionDiscardOp::until_eof);

    return true;
  }

  return false;
}

bool Entity::Slide(Battle::Tile* dest, 
  const frame_time_t& slideTime, const frame_time_t& endlag, ActionOrder order, std::function<void()> onBegin)
{
  if (dest && CanMoveTo(dest)) {
    MoveEvent event = { slideTime, moveStartupDelay, moveEndlagDelay, 0, dest, onBegin };
    actionQueue.Add(event, order, ActionDiscardOp::until_eof);

    return true;
  }

  return false;
}

bool Entity::Jump(Battle::Tile* dest, float destHeight, 
  const frame_time_t& jumpTime, const frame_time_t& endlag, ActionOrder order, std::function<void()> onBegin)
{
  destHeight = std::max(destHeight, 0.f); // no negative jumps

  if (dest && CanMoveTo(dest)) {
    MoveEvent event = { jumpTime, moveStartupDelay, moveEndlagDelay, destHeight, dest, onBegin };
    actionQueue.Add(event, order, ActionDiscardOp::until_eof);

    return true;
  }

  return false;
}

void Entity::FinishMove()
{
  // completes the move or moves the object back
  if (currMoveEvent.dest /*&& !currMoveEvent.immutable*/) {
    AdoptNextTile();
    tileOffset = {};
    currMoveEvent = {};
    actionQueue.ClearFilters();
    actionQueue.Pop();
  }
}

void Entity::RawMoveEvent(const MoveEvent& event, ActionOrder order)
{
  if (event.dest && CanMoveTo(event.dest)) {
    actionQueue.Add(event, order, ActionDiscardOp::until_eof);
  }
}

void Entity::HandleMoveEvent(MoveEvent& event, const ActionQueue::ExecutionType& exec)
{
  if (exec == ActionQueue::ExecutionType::interrupt) {
    FinishMove();
    return;
  }

  if (currMoveEvent.dest == nullptr) {
    UpdateMoveStartPosition();
    FilterMoveEvent(event);
    currMoveEvent = event;
    moveEventFrame = this->frame;
    previous = tile;
    elapsedMoveTime = 0;
    actionQueue.CreateDiscardFilter(ActionTypes::buster, ActionDiscardOp::until_resolve);
    actionQueue.CreateDiscardFilter(ActionTypes::peek_card, ActionDiscardOp::until_resolve);
  }

}

// Default implementation of CanMoveTo() checks 
// 1) if the tile is walkable
// 2) if not, if the entity can float 
// 3) if the tile is valid and the next tile is the same team
bool Entity::CanMoveTo(Battle::Tile * next)
{
  bool valid = next? (HasFloatShoe()? true : next->IsWalkable()) : false;
  return valid && Teammate(next->GetTeam());
}

const long Entity::GetID() const
{
  return ID;
}

/** \brief Unkown team entities are friendly to all spaces @see Cubes */
bool Entity::Teammate(Team _team) const {
  return (team == Team::unknown) || (team == _team);
}

void Entity::SetTile(Battle::Tile* _tile) {
  tile = _tile;
}

Battle::Tile* Entity::GetTile(Direction dir, unsigned count) const {
  auto next = this->tile;

  while (count > 0) {
    next = next + dir;
    count--;
  }

  return next;
}

Battle::Tile* Entity::GetCurrentTile() const {
    return GetTile();
}

const sf::Vector2f Entity::GetTileOffset() const
{
  return this->tileOffset;
}

const bool Entity::IsSliding() const
{
  bool is_moving = currMoveEvent.IsSliding();

  return is_moving;
}

const bool Entity::IsJumping() const
{
  bool is_moving = currMoveEvent.IsJumping();

  return is_moving;
}

const bool Entity::IsTeleporting() const
{
  bool is_moving = currMoveEvent.IsTeleporting();

  return is_moving;
}

const bool Entity::IsMoving() const
{
  return IsSliding() || IsJumping() || IsTeleporting();
}

void Entity::SetField(Field* _field) {
  assert(_field && "field was nullptr");
  field = _field;
  channel = EventBus::Channel(_field->scene);
}

Field* Entity::GetField() const {
  return field;
}

Team Entity::GetTeam() const {
  return team;
}
void Entity::SetTeam(Team _team) {
  team = _team;
}

void Entity::SetPassthrough(bool state)
{
  passthrough = state;
}

bool Entity::IsPassthrough()
{
  return passthrough;
}

void Entity::SetFloatShoe(bool state)
{
  floatShoe = state;
}

void Entity::SetAirShoe(bool state) {
  airShoe = state;
}

void Entity::SlidesOnTiles(bool state)
{
  slidesOnTiles = state;
}

bool Entity::HasFloatShoe()
{
  return floatShoe;
}

bool Entity::HasAirShoe() {
  return airShoe;
}

bool Entity::WillSlideOnTiles()
{
  return slidesOnTiles;
}

void Entity::SetDirection(Direction dir) {
  direction = dir;
}

Direction Entity::GetDirection()
{
  return direction;
}

void Entity::SetFacing(Direction facing)
{
  this->facing = facing;
}

Direction Entity::GetFacing()
{
  return facing;
}

Direction Entity::GetPreviousDirection()
{
  return previousDirection;
}

void Entity::Delete()
{
  if (deleted) return;

  deleted = true;

  OnDelete();
}

void Entity::Remove()
{
  flagForRemove = true;
}

bool Entity::IsDeleted() const {
  return deleted;
}

bool Entity::WillRemoveLater() const
{
    return flagForRemove;
}

void Entity::SetElement(Element _elem)
{
  element = _elem;
}

const Element Entity::GetElement() const
{
  return element;
}

void Entity::AdoptNextTile()
{
  Battle::Tile* next = currMoveEvent.dest;
  if (next == nullptr) {
    return;
  }

  if (previous != nullptr /*&& previous != tile*/) {
    previous->RemoveEntityByID(GetID());
    // If removing an entity and the tile was broken, crack the tile
    previous->HandleMove(this);
    previous = tile;
  }

  AdoptTile(next);

  // Slide if the tile we are moving to is ICE
  if (next->GetState() != TileState::ice || HasFloatShoe()) {
    // If not using animations, then 
    // adopting a tile is the last step in the move procedure
    // Increase the move count
    moveCount++;
  }
}

void Entity::ToggleTimeFreeze(bool state)
{
  isTimeFrozen = state;
}

const bool Entity::IsTimeFrozen()
{
  return isTimeFrozen;
}

void Entity::FreeAllComponents()
{
  for (int i = 0; i < components.size(); i++) {
    components[i]->Eject();
  }

  ReleaseComponentsPendingRemoval();

  components.clear();
}

const EventBus::Channel& Entity::EventChannel() const
{
  return channel;
}

void Entity::FreeComponentByID(Component::ID_t ID) {
  auto iter = components.begin();
  while(iter != components.end()) {
    auto component = *iter;

    if (component->GetID() == ID) {
      // Safely delete component by queueing it
      queuedComponents.insert(queuedComponents.begin(), ComponentBucket{ component, ComponentBucket::Status::remove });
      return; // found and handled, quit early.
    }

    iter = std::next(iter);
  }
}

Component* Entity::RegisterComponent(Component* c) {
  if (c == nullptr) return nullptr;

  auto iter = std::find(components.begin(), components.end(), c);
  if (iter != components.end())
    return *iter;

  if (isUpdating) {
    queuedComponents.insert(queuedComponents.begin(), ComponentBucket{ c, ComponentBucket::Status::add });
  }
  else {
    components.push_back(c);
    SortComponents();
  }

  return c;
}

void Entity::UpdateMoveStartPosition()
{
  if (tile) {
    moveStartPosition = sf::Vector2f(tileOffset.x + tile->getPosition().x, tileOffset.y + tile->getPosition().y);
  }
}

const int Entity::GetMoveCount() const
{
    return moveCount;
}

void Entity::SetMoveEndlag(const frame_time_t& frames)
{
  moveEndlagDelay = frames;
}

void Entity::SetMoveStartupDelay(const frame_time_t& frames)
{
  moveStartupDelay = frames;
}

void Entity::ClearActionQueue()
{
  actionQueue.ClearQueue(ActionQueue::CleanupType::allow_interrupts);
}

const float Entity::GetJumpHeight() const
{
  return currMoveEvent.height;
}

const float Entity::GetCurrJumpHeight() const
{
  return currJumpHeight;
}