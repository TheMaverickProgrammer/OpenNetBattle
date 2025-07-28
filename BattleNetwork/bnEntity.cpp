#include "bnEntity.h"
#include "bnComponent.h"
#include "bnTile.h"
#include "bnField.h"
#include "bnPlayer.h"
#include "bnShakingEffect.h"
#include "bnShaderResourceManager.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"
#include <cmath>
#include <Swoosh/Ease.h>

long Entity::numOfIDs = 0;

bool EntityComparitor::operator()(std::shared_ptr<Entity> f, std::shared_ptr<Entity> s) const
{
  return f->GetID() < s->GetID();
}

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
  channel(nullptr),
  mode(Battle::TileHighlight::none),
  hitboxProperties(Hit::DefaultProperties),
  CounterHitPublisher()
{
  ID = ++Entity::numOfIDs;

  SetColorMode(ColorMode::additive);
  setColor(NoopCompositeColor(ColorMode::additive));

  if (sf::Shader* shader = Shaders().GetShader(ShaderType::BATTLE_CHARACTER)) {
    SetShader(shader);
    SmartShader& smartShader = GetShader();
    smartShader.SetUniform("texture", sf::Shader::CurrentTexture);
    smartShader.SetUniform("additiveMode", true);
    smartShader.SetUniform("swapPalette", false);
    baseColor = sf::Color(0, 0, 0, 0);
  }

  using namespace std::placeholders;
  auto handler = std::bind(&Entity::HandleMoveEvent, this, _1, _2);
  actionQueue.RegisterType<MoveEvent>(ActionTypes::movement, handler);

  whiteout = Shaders().GetShader(ShaderType::WHITE);
  stun = Shaders().GetShader(ShaderType::YELLOW);
  root = Shaders().GetShader(ShaderType::BLACK);
  setColor(NoopCompositeColor(GetColorMode()));

  shadow = std::make_shared<SpriteProxyNode>();
  shadow->SetLayer(1);
  shadow->Hide(); // default: hidden
  AddNode(shadow);

  iceFx = std::make_shared<SpriteProxyNode>();
  iceFx->setTexture(Textures().LoadFromFile(TexturePaths::ICE_FX));
  iceFx->SetLayer(-2);
  iceFx->Hide(); // default: hidden
  AddNode(iceFx);

  blindFx = std::make_shared<SpriteProxyNode>();
  blindFx->setTexture(Textures().LoadFromFile(TexturePaths::BLIND_FX));
  blindFx->SetLayer(-2);
  blindFx->Hide(); // default: hidden
  AddNode(blindFx);

  iceFxAnimation = Animation(AnimationPaths::ICE_FX);
  blindFxAnimation = Animation(AnimationPaths::BLIND_FX);
}

Entity::~Entity() {
  std::shared_ptr<Field> f = field.lock();
  if (!f) return;
  
  f->ClearAllReservations(ID);
}

void Entity::Cleanup() {
  FreeAllComponents();
}

void Entity::SortComponents()
{
  // Newest components appear first in the list for easy referencing
  std::sort(components.begin(), components.end(), [](std::shared_ptr<Component>& a, std::shared_ptr<Component>& b) { return a->GetID() > b->GetID(); });
}

void Entity::ClearPendingComponents()
{
  queuedComponents.clear();
}

void Entity::ReleaseComponentsPendingRemoval()
{
  // `delete` may kick off deconstructors that Eject() other components
  std::list<Entity::ComponentBucket> copy = queuedComponents;

  // Remove the component from our list
  for (Entity::ComponentBucket& bucket : copy) {
    if (bucket.action != ComponentBucket::Status::remove) continue;

    auto iter = std::find(components.begin(), components.end(), bucket.pending);

    if (iter != components.end()) {
      components.erase(iter);
    }
  }
}

void Entity::InsertComponentsPendingRegistration()
{
  bool sort = queuedComponents.size();

  for (Entity::ComponentBucket& bucket : queuedComponents) {
    if (bucket.action == ComponentBucket::Status::add) {
      components.push_back(bucket.pending);
    }
  }

  sort ? SortComponents() : void(0);
}

void Entity::UpdateMovement(double elapsed)
{
  // Only move if we have a valid next tile pointer
  Battle::Tile* next = currMoveEvent.dest;
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
      sf::Vector2f interpol = tar * delta + (pos * (1.0f - delta));
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
        MoveEvent copyMoveEvent = currMoveEvent;
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
            bool weAreIce = (GetElement() == Element::aqua);
            bool cancelSlide = (notIce || cannotMove || weAreIce);

            if (slidesOnTiles && !cancelSlide) {
              MoveEvent event = { frames(4), frames(0), frames(0), 0, tile + previousDirection };
              RawMoveEvent(event, ActionOrder::immediate);
              copyMoveEvent = {};
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

  if (tile) {
    setPosition(tile->getPosition() + Entity::tileOffset + drawOffset);
  }
}

void Entity::SetFrame(unsigned frame)
{
  this->frame = frame;
}

void Entity::Spawn(Battle::Tile& start)
{
  if (!hasSpawned) {
    if (GetFacing() == Direction::none) {
      SetFacing(start.GetFacing());
    }

    hasSpawned = true;

    OnSpawn(start);
  }
}

bool Entity::HasSpawned() {
  return hasSpawned;
}

void Entity::BattleStart()
{
  if (fieldStart) return;

  fieldStart = true;
  OnBattleStart();
}

void Entity::BattleStop()
{
  if (!fieldStart) return;
  OnBattleStop();
}

const float Entity::GetHeight() const {
  return height;
}

void Entity::SetHeight(const float height) {
  Entity::height = std::fabs(height);
}

VirtualInputState& Entity::InputState()
{
  return inputState;
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
    case Element::sword:
      return _other == Element::breaker;
      break;
    case Element::wind:
      return _other == Element::sword;
      break;
    case Element::cursor:
      return _other == Element::wind;
      break;
    case Element::breaker:
      return _other == Element::cursor;
      break;
  }
    
  return false;
}

bool Entity::HasInit() {
  return hasInit;
}

void Entity::Init() {
  hasInit = true;
}

void Entity::Update(double _elapsed) {
  ResolveFrameBattleDamage();

  if (fieldStart && ((maxHealth > 0 && health <= 0) || IsDeleted())) {
    // Ensure entity is deleted if health is zero
    if (manualDelete == false) {
      Delete();
    }

    // Ensure health is zero if marked for immediate deletion
    health = 0;

    // Ensure status effects do not play out
    stunCooldown = frames(0);
    rootCooldown = frames(0);
    invincibilityCooldown = frames(0);
  }

  // reset base color
  setColor(NoopCompositeColor(GetColorMode()));

  RefreshShader();

  if (!hit) {
    if (invincibilityCooldown > frames(0)) {
      unsigned frame = invincibilityCooldown.count() % 4;
      if (frame < 2) {
        Reveal();
      }
      else {
        Hide();
      }

      invincibilityCooldown -= from_seconds(_elapsed);

      if (invincibilityCooldown <= frames(0)) {
        Reveal();
      }
    }
  }

  if(rootCooldown > frames(0)) {
    rootCooldown -= from_seconds(_elapsed);

    // Root is cancelled if these conditions are met
    if (rootCooldown <= frames(0) || invincibilityCooldown > frames(0) || IsPassthrough()) {
      rootCooldown = frames(0);
    }
  }

  bool canUpdateThisFrame = true;

  if(stunCooldown > frames(0)) {
    canUpdateThisFrame = false;
    stunCooldown -= from_seconds(_elapsed);

    if (stunCooldown <= frames(0)) {
      stunCooldown = frames(0);
    }
  }

  // assume this is hidden, will flip to visible if not
  iceFx->Hide();
  if (freezeCooldown > frames(0)) {
    iceFxAnimation.Update(_elapsed, iceFx->getSprite());
    iceFx->Reveal();

    canUpdateThisFrame = false;
    freezeCooldown -= from_seconds(_elapsed);

    if (freezeCooldown <= frames(0)) {
      freezeCooldown = frames(0);
    }
  }

  // assume this is hidden, will flip to visible if not
  blindFx->Hide();
  if (blindCooldown > frames(0)) {
    blindFxAnimation.Update(_elapsed, blindFx->getSprite());
    blindFx->Reveal();

    blindCooldown -= from_seconds(_elapsed);

    if (blindCooldown <= frames(0)) {
      blindCooldown = frames(0);
    }
  }
  
  if(canUpdateThisFrame) {
    OnUpdate(_elapsed);
  }

  isUpdating = true;

  actionQueue.Process();

  UpdateMovement(_elapsed);

  sf::Uint8 alpha = getSprite().getColor().a;
  for (std::shared_ptr<SceneNode>& child : GetChildNodes()) {
    SpriteProxyNode* sprite = dynamic_cast<SpriteProxyNode*>(child.get());
    if (sprite) {
      sf::Color color = sprite->getColor();
      sprite->setColor(sf::Color(color.r, color.g, color.b, alpha));
    }
  }

  // Update all components
  for (std::shared_ptr<Component>& component : components) {
    // respectfully only update local components
    // anything shared with the battle scene needs to update those components
    if (component->Lifetime() == Component::lifetimes::local) {
      component->Update(_elapsed);
    }
  }

  ReleaseComponentsPendingRemoval();
  InsertComponentsPendingRegistration();
  ClearPendingComponents();

  isUpdating = false;

  // If the counterSlideOffset has changed from 0, it's due to the character
  // being deleted on a counter frame. Begin animating the counter-delete slide
  if (counterSlideOffset.x != 0 || counterSlideOffset.y != 0) {
    counterSlideDelta += static_cast<float>(_elapsed);
    
    float delta = swoosh::ease::linear(counterSlideDelta, 0.10f, 1.0f);
    sf::Vector2f offset = delta * counterSlideOffset;

    // Add this offset onto our offsets
    setPosition(tile->getPosition().x + offset.x, tile->getPosition().y + offset.y);
  }

  // If drag status is over, reset the flag
  if (!IsSliding() && slideFromDrag) slideFromDrag = false;
}


void Entity::SetPalette(const std::shared_ptr<sf::Texture>& palette)
{
  SmartShader& smartShader = GetShader();

  if (palette.get() == nullptr) {
    smartShader.SetUniform("swapPalette", false);
    swapPalette = false;
    return;
  }

  swapPalette = true;
  this->palette = palette;
  smartShader.SetUniform("swapPalette", true);
  smartShader.SetUniform("palette", this->palette);
}

std::shared_ptr<sf::Texture> Entity::GetPalette()
{
  return palette;
}

void Entity::StoreBasePalette(const std::shared_ptr<sf::Texture>& palette)
{
  basePalette = palette;
}

std::shared_ptr<sf::Texture> Entity::GetBasePalette()
{
  return basePalette;
}

void Entity::RefreshShader()
{
  std::shared_ptr<Field> field = this->field.lock();

  if (!field) {
    return;
  }

  sf::Shader* shader = Shaders().GetShader(ShaderType::BATTLE_CHARACTER);
  SmartShader& smartShader = GetShader();

  if (shader != smartShader.Get()) {
    SetShader(shader);
  }

  if (!smartShader.HasShader()) return;

  smartShader.SetUniform("swapPalette", swapPalette);
  smartShader.SetUniform("palette", palette);

  // state checks
  unsigned stunFrame = stunCooldown.count() % 4;
  unsigned rootFrame = rootCooldown.count() % 4;
  counterFrameFlag = counterFrameFlag % 4;
  counterFrameFlag++;

  bool iframes = invincibilityCooldown > frames(0);
  bool whiteout = hit && !isTimeFrozen;
  vector<float> states = {
    static_cast<float>(whiteout),                                           // WHITEOUT
    static_cast<float>(rootCooldown > frames(0) && (iframes || rootFrame)), // BLACKOUT
    static_cast<float>(stunCooldown > frames(0) && (iframes || stunFrame)), // HIGHLIGHT
    static_cast<float>(freezeCooldown > frames(0))                          // ICEOUT
  };

  smartShader.SetUniform("states", states);
  smartShader.SetUniform("additiveMode", GetColorMode() == ColorMode::additive);

  bool enabled = states[0] || states[1];

  if (enabled) return;

  if (counterable && field->DoesRevealCounterFrames() && counterFrameFlag < 2) {
    // Highlight when the character can be countered
    setColor(sf::Color(55, 55, 255, getColor().a));
  }
}

void Entity::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  // NOTE: This function does not call the parent implementation
  //       This function is a special behavior for battle characters to
  //       color their attached nodes correctly in-game

  if (!SpriteProxyNode::show) return;

  SmartShader& smartShader = GetShader();

  // combine the parent transform with the node's one
  sf::Transform combinedTransform = getTransform();

  states.transform *= combinedTransform;

  std::vector<SceneNode*> copies;
  copies.reserve(childNodes.size() + 1);

  for (std::shared_ptr<SceneNode>& child : childNodes) {
    copies.push_back(child.get());
  }

  copies.push_back((SceneNode*)this);

  std::sort(copies.begin(), copies.end(), [](SceneNode* a, SceneNode* b) { return (a->GetLayer() > b->GetLayer()); });

  // draw its children
  for (std::size_t i = 0; i < copies.size(); i++) {
    SceneNode* currNode = copies[i];

    if (!currNode) continue;

    // If it's time to draw our scene node, we draw the proxy sprite
    if (currNode == this) {
      sf::Shader* s = smartShader.Get();

      if (s) {
        states.shader = s;
      }

      target.draw(getSpriteConst(), states);
    }
    else {
      SpriteProxyNode* asSpriteProxyNode{ nullptr };
      SmartShader temp(smartShader);

      /**
      hack for now.
      form overlay nodes (like helmet and shoulder pads)
      are already colored to the desired palette. So we do not apply palette swapping.
      **/
      bool needsRevert = false;
      sf::Color tempColor = sf::Color::White;
      if (currNode->HasTag(Player::FORM_NODE_TAG)) {
        asSpriteProxyNode = dynamic_cast<SpriteProxyNode*>(currNode);

        if (asSpriteProxyNode) {
          smartShader.SetUniform("swapPalette", false);
          tempColor = asSpriteProxyNode->getColor();
          asSpriteProxyNode->setColor(sf::Color(0, 0, 0, getColor().a));
          needsRevert = true;
        }
      }

      // Apply and return shader if applicable
      sf::Shader* s = smartShader.Get();

      if (s && currNode->IsUsingParentShader()) {
        if (auto asSpriteProxyNode = dynamic_cast<SpriteProxyNode*>(currNode)) {
          asSpriteProxyNode->setColor(this->getColor());
        }

        states.shader = s;
      }

      target.draw(*currNode, states);

      // revert color
      if (asSpriteProxyNode && needsRevert) {
        asSpriteProxyNode->setColor(tempColor);
      }

      // revert uniforms from this pass
      smartShader = temp;
    }
  }
}

void Entity::SetAlpha(int value)
{
  alpha = value;
  sf::Color c = getColor();
  c.a = alpha;

  setColor(c);
}

int Entity::GetAlpha()
{
  return getColor().a;
}

bool Entity::Teleport(Battle::Tile* dest, ActionOrder order, std::function<void()> onBegin) {
  if (dest && CanMoveTo(dest)) {
    frame_time_t endlagDelay = moveEndlagDelay ? *moveEndlagDelay : frame_time_t{};
    MoveEvent event = { 0, moveStartupDelay, endlagDelay, 0, dest, onBegin };
    actionQueue.Add(event, order, ActionDiscardOp::until_eof);

    return true;
  }

  return false;
}

bool Entity::Slide(Battle::Tile* dest, 
  const frame_time_t& slideTime, const frame_time_t& endlag, ActionOrder order, std::function<void()> onBegin)
{
  if (dest && CanMoveTo(dest)) {
    frame_time_t endlagDelay = moveEndlagDelay ? *moveEndlagDelay : endlag;
    MoveEvent event = { slideTime, moveStartupDelay, endlagDelay, 0, dest, onBegin };
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
    frame_time_t endlagDelay = moveEndlagDelay ? *moveEndlagDelay : endlag;
    MoveEvent event = { jumpTime, moveStartupDelay, endlagDelay, destHeight, dest, onBegin };
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

bool Entity::RawMoveEvent(const MoveEvent& event, ActionOrder order)
{
  if (event.dest && CanMoveTo(event.dest)) {
    actionQueue.Add(event, order, ActionDiscardOp::until_eof);

    return true;
  }

  return false;
}

void Entity::HandleMoveEvent(MoveEvent& event, const ActionQueue::ExecutionType& exec)
{
  if (exec == ActionQueue::ExecutionType::interrupt) {
    FinishMove();
    return;
  }

  if (currMoveEvent.dest == nullptr && !IsRooted()) {
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
  bool valid = Teammate(next->GetTeam()) && !next->IsReservedByCharacter({ GetID() });

  if (HasAirShoe() && !next->IsEdgeTile()) {
    return valid;
  }

  return next->IsWalkable() && valid;
}

const long Entity::GetID() const
{
  return ID;
}

/** \brief Unkown team entities are friendly to all spaces @see Cubes */
bool Entity::Teammate(Team _team) const {
  return (team == Team::unknown) || (_team == Team::unknown) || (team == _team);
}

void Entity::SetTile(Battle::Tile* _tile) {
  // If this entity is not moving, we can safely
  // refresh their position to the new tile
  if(!IsMoving() && _tile) {
    setPosition(_tile->getPosition() + Entity::drawOffset);
  }

  tile = _tile;
}

Battle::Tile* Entity::GetTile(Direction dir, unsigned count) const {
  Battle::Tile* next = tile;

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

void Entity::SetDrawOffset(const sf::Vector2f& offset)
{
  drawOffset = offset;
}

void Entity::SetDrawOffset(float x, float y)
{
  drawOffset = { x, y };
}

const sf::Vector2f Entity::GetDrawOffset() const
{
  return drawOffset;
}

const bool Entity::IsSliding() const
{
  bool is_moving = currMoveEvent.IsSliding();

  return is_moving;
}

const bool Entity::IsJumping() const
{
  bool is_moving = currMoveEvent.IsJumping();

  return is_moving && currJumpHeight > 0.f;
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

void Entity::SetField(std::shared_ptr<Field> _field) {
  assert(_field && "field was nullptr");
  field = _field;
  channel = EventBus::Channel(_field->scene);
}

std::shared_ptr<Field> Entity::GetField() const {
  return field.lock();
}

bool Entity::IsOnField() const {
  return !field.expired();
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

void Entity::SetMoveDirection(Direction dir) {
  direction = dir;
}

Direction Entity::GetMoveDirection()
{
  return direction;
}

void Entity::SetFacing(Direction facing)
{
  if (facing == Direction::left) {
    neverFlip? void(0) : setScale(-2.f, 2.f); // flip standard facing right sprite
  }
  else if (facing == Direction::right) {
    neverFlip? void(0) : setScale(2.f, 2.f); // standard facing
  }
  else {
    return;
  }

  this->facing = facing;
}

Direction Entity::GetFacing()
{
  return facing;
}

Direction Entity::GetFacingAway()
{
  return Reverse(facing);
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

void Entity::Erase()
{
  flagForErase = true;
}

bool Entity::IsDeleted() const {
  return deleted;
}

bool Entity::WillEraseEOF() const
{
    return flagForErase;
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

  if (previous != nullptr && previous != next) {
    previous->RemoveEntityByID(GetID());

    // If removing an entity and the tile was broken, crack the tile
    previous->HandleMove(shared_from_this());
  }

  previous = tile;

  if (!IsMoving()) {
    setPosition(next->getPosition() + Entity::drawOffset);
  }

  next->AddEntity(shared_from_this());

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
    std::shared_ptr<Component> component = *iter;

    if (component->GetID() == ID) {
      // Safely delete component by queueing it
      queuedComponents.insert(queuedComponents.begin(), ComponentBucket{ component, ComponentBucket::Status::remove });
      return; // found and handled, quit early.
    }

    iter = std::next(iter);
  }
}

const float Entity::GetElevation() const
{
  return elevation;
}

void Entity::SetElevation(const float elevation)
{
  this->elevation = elevation;
}

std::shared_ptr<Component> Entity::RegisterComponent(std::shared_ptr<Component> c) {
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

void Entity::ClearActionQueue()
{
  actionQueue.ClearQueue(ActionQueue::CleanupType::allow_interrupts);
}

const float Entity::GetJumpHeight() const
{
  return currMoveEvent.height;
}

void Entity::ShowShadow(bool enabled)
{
  if (enabled) {
    shadow->Reveal();
  }
  else {
    shadow->Hide();
  }
}

void Entity::SetShadowSprite(Shadow type)
{
  switch (type) {
  case Entity::Shadow::none:
    shadow->Hide();
    break;
  case Entity::Shadow::small:
    {
      shadow->setTexture(Textures().LoadFromFile(TexturePaths::MISC_SMALL_SHADOW), true);
      sf::FloatRect bounds = shadow->getLocalBounds();
      shadow->setOrigin(sf::Vector2f(bounds.width * 0.5f, bounds.height * 0.5f));
    }
    break;
  case Entity::Shadow::big:
    {
      shadow->setTexture(Textures().LoadFromFile(TexturePaths::MISC_BIG_SHADOW), true);
      sf::FloatRect bounds = shadow->getLocalBounds();
      shadow->setOrigin(sf::Vector2f(bounds.width * 0.5f, bounds.height * 0.5f));
    }
    break;
  case Entity::Shadow::custom:
    // no op
  default:
    break;
  }
}

void Entity::SetShadowSprite(std::shared_ptr<sf::Texture> customShadow)
{
  shadow->setTexture(customShadow, true);
  sf::FloatRect bounds = shadow->getLocalBounds();
  shadow->setOrigin(sf::Vector2f(bounds.width*0.5f, bounds.height*0.5f));
}

void Entity::ShiftShadow() {
  // counter offset the shadow node
  shadow->setPosition(0, 0.5f * (Entity::GetElevation() + Entity::GetCurrJumpHeight()));
}

const float Entity::GetCurrJumpHeight() const
{
  return currJumpHeight;
}

void Entity::ShareTileSpace(bool enabled)
{
  canShareTile = enabled;
}

const bool Entity::CanShareTileSpace() const
{
  return canShareTile;
}

void Entity::EnableTilePush(bool enabled)
{
  canTilePush = enabled;
}

void Entity::SetName(std::string name)
{
  this->name = name;
}

const std::string Entity::GetName() const
{
  return name;
}

const bool Entity::CanTilePush() const {
  return canTilePush;
}

const bool Entity::Hit(Hit::Properties props) {

  if (!hitboxEnabled) {
    return false;
  }

  if (health <= 0) {
    return false;
  }

  const Hit::Properties original = props;

  // If in time freeze, shake immediate on any contact
  if ((props.flags & Hit::shake) == Hit::shake && IsTimeFrozen()) {
    CreateComponent<ShakingEffect>(weak_from_this());
  }
  
  for (std::shared_ptr<DefenseRule>& defense : defenses) {
    props = defense->FilterStatuses(props);
  }

  // If the character itself is also super-effective,
  // double the damage independently from tile damage
  bool isSuperEffective = IsSuperEffective(props.element);

  // super effective damage is x2
  if (isSuperEffective) {
    props.damage *= 2;
  }

  SetHealth(GetHealth() - props.damage);

  if (IsTimeFrozen()) {
    props.flags |= Hit::no_counter;
  }

  // Add to status queue for state resolution
  statusQueue.push(CombatHitProps{ original, props });

  if ((props.flags & Hit::impact) == Hit::impact) {
    this->hit = true; // flash white immediately
    RefreshShader();
  }

  if (GetHealth() <= 0) {
    SetShader(whiteout);
  }

  return true;
}

void Entity::RegisterStatusCallback(const Hit::Flags& flag, const StatusCallback& callback)
{
  statusCallbackHash[flag] = callback;
}

void Entity::ManualDelete()
{
  manualDelete = true;
}

void Entity::PrepareNextFrame()
{
  hit = false;
}

const bool Entity::UnknownTeamResolveCollision(const Entity& other) const
{
  return true; // by default unknown vs unknown spells attack eachother
}

const bool Entity::HasCollision(const Hit::Properties & props)
{
  // Pierce status hits even when passthrough or flinched
  if ((props.flags & Hit::pierce) != Hit::pierce) {
    if (invincibilityCooldown > frames(0) || IsPassthrough() || !hitboxEnabled) return false;
  }

  return true;
}

int Entity::GetHealth() const {
  return health;
}

void Entity::SetMaxHealth(int _health)
{
  maxHealth = _health;
}

const int Entity::GetMaxHealth() const
{
  return maxHealth;
}

void Entity::ResolveFrameBattleDamage()
{
  if(statusQueue.empty() || IsDeleted()) return;

  std::shared_ptr<Character> frameCounterAggressor = nullptr;
  bool frameStunCancel = false;
  bool frameFlashCancel = false;
  bool frameFreezeCancel = false;
  bool willFreeze = false;
  Hit::Drag postDragEffect{};

  std::queue<CombatHitProps> append;

  while (!statusQueue.empty() && !IsSliding()) {
    CombatHitProps props = statusQueue.front();
    statusQueue.pop();

    // a re-usable thunk for custom status effects
    auto flagCheckThunk = [props, this](const Hit::Flags& toCheck) {
      if ((props.filtered.flags & toCheck) == toCheck) {
        if (Entity::StatusCallback& func = statusCallbackHash[toCheck]) {
          func();
        }
      }
    };

    int tileDamage = 0;
    int extraDamage = 0;

    // Calculate elemental damage if the tile the character is on is super effective to it
    if (props.filtered.element == Element::fire
      && GetTile()->GetState() == TileState::grass) {
      tileDamage = props.filtered.damage;
      GetTile()->SetState(TileState::normal);
    }
    
    if (props.filtered.element == Element::elec
      && GetTile()->GetState() == TileState::ice) {
      tileDamage = props.filtered.damage;
    }

    if (props.filtered.element == Element::aqua 
      && GetTile()->GetState() == TileState::ice 
      && !frameFreezeCancel) {
      willFreeze = true;
      GetTile()->SetState(TileState::normal);
    }

    if ((props.filtered.flags & Hit::breaking) == Hit::breaking && IsIceFrozen()) {
      extraDamage = props.filtered.damage;
      frameFreezeCancel = true;
    }

    // Broadcast the hit before we apply statuses and change the entity's state flags
    if (props.filtered.damage > 0) {
      SetHealth(GetHealth() - (tileDamage + extraDamage));
      HitPublisher::Broadcast(*this, props.filtered);
    }

    // start of new scope
    {
      // Only register counter if:
      // 1. Hit type is impact
      // 2. The hitbox is allowed to counter
      // 3. The character is on a counter frame
      // 4. Hit properties has an aggressor
      // This will set the counter aggressor to be the first non-impact hit and not check again this frame
      if (IsCountered() && (props.filtered.flags & Hit::impact) == Hit::impact && !frameCounterAggressor) {
        if ((props.hitbox.flags & Hit::no_counter) == 0 && props.filtered.aggressor) {
          frameCounterAggressor = GetField()->GetCharacter(props.filtered.aggressor);
        }

        OnCountered();
        flagCheckThunk(Hit::impact);
      }

      // Requeue drag if already sliding by drag or in the middle of a move
      if ((props.filtered.flags & Hit::drag) == Hit::drag) {
        if (IsSliding()) {
          append.push({ props.hitbox, { 0, Hit::drag, Element::none, 0, props.filtered.drag } });
        }
        else {
          // requeue counter hits, if any (frameCounterAggressor is null when no counter was present)
          if (frameCounterAggressor) {
            append.push({ props.hitbox, { 0, Hit::impact, Element::none, frameCounterAggressor->GetID() } });
            frameCounterAggressor = nullptr;
          }

          // requeue drag if count is > 0
          if(props.filtered.drag.count > 0) {
            // Apply drag effect post status resolution
            postDragEffect.dir = props.filtered.drag.dir;
            postDragEffect.count = props.filtered.drag.count - 1u;
          }
        }

        flagCheckThunk(Hit::drag);

        // exclude this from the next processing step
        props.filtered.flags &= ~Hit::drag;
      }

      bool flashAndFlinch = ((props.filtered.flags & Hit::flash) == Hit::flash) && ((props.filtered.flags & Hit::flinch) == Hit::flinch);
      frameFreezeCancel = frameFreezeCancel || flashAndFlinch;

      /**
      While an attack that only flinches will not cancel stun, 
      an attack that both flinches and flashes will cancel stun. 
      This applies if the entity doesn't have SuperArmor installed. 
      If they do have armor, stun isn't cancelled.
      
      This effect is requeued for another frame if currently dragging
      */
      if ((props.filtered.flags & Hit::stun) == Hit::stun) {
        if (postDragEffect.dir != Direction::none) {
          // requeue these statuses if in the middle of a slide
          append.push({ props.hitbox, { 0, props.filtered.flags } });
        }
        else {
          // TODO: this is a specific (and expensive) check. Is there a way to prioritize this defense rule?
          /*for (auto&& d : this->defenses) {
            hasSuperArmor = hasSuperArmor || dynamic_cast<DefenseSuperArmor*>(d);
          }*/

          // assume some defense rule strips out flinch, prevent abuse of stun
          frameStunCancel = frameStunCancel ||(props.filtered.flags & Hit::flinch) == 0 && (props.hitbox.flags & Hit::flinch) == Hit::flinch;

          if ((props.filtered.flags & Hit::flash) == Hit::flash && frameStunCancel) {
            // cancel stun
            stunCooldown = frames(0);
          }
          else {
            // refresh stun
            stunCooldown = frames(120);
            flagCheckThunk(Hit::stun);
          }

          actionQueue.ClearQueue(ActionQueue::CleanupType::allow_interrupts);
        }
      }

      // exclude this from the next processing step
      props.filtered.flags &= ~Hit::stun;

      if ((props.filtered.flags & Hit::freeze) == Hit::freeze) {
        if (postDragEffect.dir != Direction::none) {
          // requeue these statuses if in the middle of a slide
          append.push({ props.hitbox, { 0, props.filtered.flags } });
        }
        else {
          // this will strip out flash in the next step
          frameFlashCancel = true;
          willFreeze = true;
          flagCheckThunk(Hit::flinch);
        }
      }

      // exclude this from the next processing step
      props.filtered.flags &= ~Hit::freeze;

      // Always negate flash if frozen this frame
      if (frameFlashCancel) {
        props.filtered.flags &= ~Hit::flash;
      }

      // Flash can be queued if dragging this frame
      if ((props.filtered.flags & Hit::flash) == Hit::flash) {
        if (postDragEffect.dir != Direction::none) {
          append.push({ props.hitbox, { 0, props.filtered.flags } });
        }
        else {
          invincibilityCooldown = frames(120); // used as a `flash` status time
          flagCheckThunk(Hit::flash);
        }
      }

      // exclude this from the next processing step
      props.filtered.flags &= ~Hit::flash;

      // Flinch is canceled if retangibility is applied
      if ((props.filtered.flags & Hit::retangible) == Hit::retangible) {
        invincibilityCooldown = frames(0);

        flagCheckThunk(Hit::retangible);
      }

      // exclude this from the next processing step
      props.filtered.flags &= ~Hit::retangible;

      if ((props.filtered.flags & Hit::bubble) == Hit::bubble) {
        if (postDragEffect.dir != Direction::none) {
          // requeue these statuses if in the middle of a slide
          append.push({ props.hitbox, { 0, props.filtered.flags } });
        }
        else {
          flagCheckThunk(Hit::bubble);
        }
      }

      // exclude this from the next processing step 
      props.filtered.flags &= ~Hit::bubble;

      if ((props.filtered.flags & Hit::root) == Hit::root) {
          rootCooldown = frames(120);
          flagCheckThunk(Hit::root);
      }

      // exclude this from the next processing step 
      props.filtered.flags &= ~Hit::root;

      // Only if not in time freeze, consider this status for delayed effect after sliding
      if ((props.filtered.flags & Hit::shake) == Hit::shake && !IsTimeFrozen()) {
        if (postDragEffect.dir != Direction::none) {
          // requeue these statuses if in the middle of a slide
          append.push({ props.hitbox, { 0, props.filtered.flags } });
        }
        else {
          CreateComponent<ShakingEffect>(weak_from_this());
          flagCheckThunk(Hit::shake);
        }
      }

      // exclude this from the next processing step 
      props.filtered.flags &= ~Hit::shake;

      // blind check 
      if ((props.filtered.flags & Hit::blind) == Hit::blind) {
        if (postDragEffect.dir != Direction::none) {
          // requeue these statuses if in the middle of a slide/drag
          append.push({ props.hitbox, { 0, props.filtered.flags } });
        }
        else {
          Blind(frames(300));
          flagCheckThunk(Hit::blind);
        }
      }

      // exclude blind from the next processing step
      props.filtered.flags &= ~Hit::blind;

      // todo: for confusion
      //if ((props.filtered.flags & Hit::confusion) == Hit::confusion) {
      //  frameStunCancel = true;
      //}

      /*
      flags already accounted for:
      - impact
      - stun
      - freeze
      - flash
      - drag
      - retangible
      - bubble
      - root
      - shake
      - blind
      Now check if the rest were triggered and invoke the
      corresponding status callbacks
      */
      flagCheckThunk(Hit::breaking);
      flagCheckThunk(Hit::pierce);
      flagCheckThunk(Hit::flinch);

      if (GetHealth() == 0) {
        postDragEffect.dir = Direction::none; // Cancel slide post-status if blowing up
      }
    }
  } // end while-loop

  if (!append.empty()) {
    statusQueue = append;
  }

  if (postDragEffect.dir != Direction::none) {
    // enemies and objects on opposing side of field are granted immunity from drag
    if (Teammate(GetTile()->GetTeam())) {
      actionQueue.ClearQueue(ActionQueue::CleanupType::allow_interrupts);
      slideFromDrag = true;
      Battle::Tile* dest = GetTile() + postDragEffect.dir;

      if (CanMoveTo(dest)) {
        // Enqueue a move action at the top of our priorities
        actionQueue.Add(MoveEvent{ frames(4), frames(0), frames(0), 0, dest, {}, true }, ActionOrder::immediate, ActionDiscardOp::until_resolve);

        std::queue<CombatHitProps> oldQueue = statusQueue;
        statusQueue = {};
        // Re-queue the drag status to be re-considered FIRST in our next combat checks
        statusQueue.push({ {}, { 0, Hit::drag, Element::none, 0, postDragEffect } });

        // append the old queue items after
        while (!oldQueue.empty()) {
          statusQueue.push(oldQueue.front());
          oldQueue.pop();
        }
      }
    }
  }

  if (GetHealth() == 0) {
    // We are dying. Prevent special fx and status animations from triggering.
    frameFreezeCancel = frameFlashCancel = frameStunCancel = true;

    while(statusQueue.size() > 0) {
      statusQueue.pop();
    }

    //FinishMove(); // cancels slide. TODO: obstacles do not use this but characters do!

    if(frameCounterAggressor) {
      // Slide entity back a few pixels
      counterSlideOffset = sf::Vector2f(50.f, 0.0f);
      CounterHitPublisher::Broadcast(*this, *frameCounterAggressor);
    }
  } else if (frameCounterAggressor) {
    CounterHitPublisher::Broadcast(*this, *frameCounterAggressor);
  }

  if (frameFreezeCancel) {
    freezeCooldown = frames(0); // end freeze effect
  }
  else if (willFreeze) {
    IceFreeze(frames(150)); // start freeze effect
  }

  if (frameFlashCancel) {
    invincibilityCooldown = frames(0); // end flash effect
  }

  if (frameStunCancel) {
    stunCooldown = frames(0); // end stun effect
  }
}

void Entity::SetHealth(const int _health) {
  std::shared_ptr<Field> fieldPtr = field.lock();

  if (fieldPtr) {
    if (!fieldPtr->isBattleActive) return;
  }

  health = _health;

  if (maxHealth == 0) {
    maxHealth = health;
  }

  if (health > maxHealth) health = maxHealth;
  if (health < 0) health = 0;
}

const bool Entity::IsHitboxAvailable() const
{
  return hitboxEnabled && GetHealth() > 0;
}

void Entity::EnableHitbox(bool enabled)
{
  hitboxEnabled = enabled;
}

void Entity::AddDefenseRule(std::shared_ptr<DefenseRule> rule)
{
  if (!rule) return;

  auto iter = std::find_if(defenses.begin(), defenses.end(), [rule](auto other) { return rule->GetPriorityLevel() == other->GetPriorityLevel(); });

  if (rule && iter == defenses.end()) {
    defenses.push_back(rule);
    std::sort(defenses.begin(), defenses.end(), [](std::shared_ptr<DefenseRule> first,std::shared_ptr<DefenseRule> second) { return first->GetPriorityLevel() < second->GetPriorityLevel(); });
  }
  else {
    (*iter)->replaced = true; // Flag that this defense rule may be valid ptr, but is no longer in use
    (*iter)->OnReplace();
    RemoveDefenseRule(*iter); // will invalidate the iterator

    // call again, adding new rule this time
    AddDefenseRule(rule);
  }
}

void Entity::RemoveDefenseRule(std::shared_ptr<DefenseRule> rule)
{
  RemoveDefenseRule(rule.get());
}

void Entity::RemoveDefenseRule(DefenseRule* rule)
{
  auto iter = std::find_if(defenses.begin(), defenses.end(), [rule](auto in) { return in.get() == rule; });

  if(iter != defenses.end())
    defenses.erase(iter);
}

void Entity::DefenseCheck(DefenseFrameStateJudge& judge, std::shared_ptr<Entity> in, const DefenseOrder& filter)
{
  std::vector<std::shared_ptr<DefenseRule>> copy = defenses;

  auto characterPtr = shared_from_base<Character>();

  for (int i = 0; i < copy.size(); i++) {
    if (copy[i]->GetDefenseOrder() == filter) {
      std::shared_ptr<DefenseRule> defenseRule = copy[i];
      judge.SetDefenseContext(defenseRule);
      defenseRule->CanBlock(judge, in, characterPtr);
    }
  }
}

bool Entity::IsCounterable()
{
  return counterable;
}

void Entity::ToggleCounter(bool on)
{
  counterable = on;
}

void Entity::NeverFlip(bool enabled)
{
  neverFlip = enabled;
}

bool Entity::IsStunned()
{
  return stunCooldown > frames(0);
}

bool Entity::IsRooted()
{
  return rootCooldown > frames(0);
}

bool Entity::IsIceFrozen() {
  return freezeCooldown > frames(0);
}

bool Entity::IsBlind()
{
  return blindCooldown > frames(0);
}

void Entity::Stun(frame_time_t maxCooldown)
{
  invincibilityCooldown = frames(0); // cancel flash
  freezeCooldown = frames(0); // cancel freeze
  stunCooldown = maxCooldown;
}

void Entity::Root(frame_time_t maxCooldown)
{
  rootCooldown = maxCooldown;
}

void Entity::IceFreeze(frame_time_t maxCooldown)
{
  invincibilityCooldown = frames(0); // cancel flash
  stunCooldown = frames(0); // cancel stun
  freezeCooldown = maxCooldown;

  const float height = GetHeight();

  static std::shared_ptr<sf::SoundBuffer> freezesfx = Audio().LoadFromFile(SoundPaths::ICE_FX);
  Audio().Play(freezesfx, AudioPriority::highest);

  if (height <= 48) {
    iceFxAnimation << "small" << Animator::Mode::Loop;
    iceFx->setPosition(0, -height/2.f);
  }
  else if (height <= 75) {
    iceFxAnimation << "medium" << Animator::Mode::Loop;
    iceFx->setPosition(0, -height/2.f);
  }
  else {
    iceFxAnimation << "large" << Animator::Mode::Loop;
    iceFx->setPosition(0, -height/2.f);
  }

  iceFxAnimation.Refresh(iceFx->getSprite());
}

void Entity::Blind(frame_time_t maxCooldown)
{
  float height = -GetHeight()/2.f;
  std::shared_ptr<AnimationComponent> anim = GetFirstComponent<AnimationComponent>();

  if (anim && anim->HasPoint("head")) {
    height = (anim->GetPoint("head") - anim->GetPoint("origin")).y;
  }

  blindCooldown = maxCooldown;
  blindFx->setPosition(0, height);
  blindFxAnimation << "default" << Animator::Mode::Loop;
  blindFxAnimation.Refresh(blindFx->getSprite());
}

bool Entity::IsCountered()
{
  return (counterable && stunCooldown <= frames(0));
}

const Battle::TileHighlight Entity::GetTileHighlightMode() const {
  return mode;
}

void Entity::HighlightTile(Battle::TileHighlight mode)
{
  this->mode = mode;
}

void Entity::SetHitboxContext(Hit::Context context)
{
  hitboxProperties.context = context;
}

Hit::Context Entity::GetHitboxContext()
{
  return hitboxProperties.context;
}

void Entity::SetHitboxProperties(Hit::Properties props)
{
  hitboxProperties = props;
  hitboxProperties.flags |= props.context.flags;
  hitboxProperties.aggressor = props.context.aggressor;
}

const Hit::Properties Entity::GetHitboxProperties() const
{
  return hitboxProperties;
}

void Entity::IgnoreCommonAggressor(bool enable = true)
{
  ignoreCommonAggressor = enable;
}

const bool Entity::WillIgnoreCommonAggressor() const
{
  return ignoreCommonAggressor;
}
