#include <list>
#include <functional>

#include "bnTile.h"
#include "bnEntity.h"
#include "bnCharacter.h"
#include "bnObstacle.h"
#include "bnSpell.h"
#include "bnArtifact.h"
#include "bnDefenseFrameStateJudge.h"
#include "bnDefenseRule.h"

#include "bnPlayer.h"
#include "bnExplosion.h"
#include "bnVolcanoErupt.h"

#include "bnAudioResourceManager.h"
#include "bnTextureResourceManager.h"
#include "bnField.h"

#define TILE_WIDTH 40.0f
#define TILE_HEIGHT 30.0f
#define START_X 0.0f
#define START_Y 144.f
#define Y_OFFSET 10.0f
#define COOLDOWN 10.0
#define FLICKER 3.0

namespace Battle {
  double Tile::brokenCooldownLength = COOLDOWN;
  double Tile::teamCooldownLength = COOLDOWN;
  double Tile::flickerTeamCooldownLength = FLICKER;

  Tile::Tile(int _x, int _y) : 
    SpriteProxyNode(),
    animation() {
    totalElapsed = 0;
    x = _x;
    y = _y;

    if (x == 0 || x == 7 || y == 0 || y == 4) {
      state = TileState::hidden;
    }
    else {
      state = TileState::normal;
    }

    entities = vector<std::shared_ptr<Entity>>();
    setScale(2.f, 2.f);
    width = TILE_WIDTH * getScale().x;
    height = TILE_HEIGHT * getScale().y;
    setOrigin(TILE_WIDTH / 2.0f, TILE_HEIGHT / 2.0f);
    setPosition((width/2.0f) + ((x - 1) * width) + START_X, (height/2.0f) + ((y - 1) * (height - Y_OFFSET)) + START_Y);
    willHighlight = false;
    isTimeFrozen = true;
    isBattleOver = false;
    brokenCooldown = 0;
    flickerTeamCooldown = teamCooldown = 0;
    red_team_atlas = blue_team_atlas = nullptr; // Set by field

    burncycle = 0.12; // milliseconds
    elapsedBurnTime = burncycle;

    highlightMode = TileHighlight::none;

    volcanoSprite = std::make_shared<SpriteProxyNode>();
    volcanoErupt = Animation("resources/tiles/volcano.animation");

    auto resetVolcanoThunk = [this](int seconds) {
      if (!isBattleOver) {
        this->volcanoErupt.SetFrame(1, this->volcanoSprite->getSprite()); // start over
        volcanoEruptTimer = seconds;
        
        std::shared_ptr<Field> field = fieldWeak.lock();

        if (field && state == TileState::volcano) {
          field->AddEntity(std::make_shared<VolcanoErupt>(), *this);
        }
      }
      else {
        RemoveNode(volcanoSprite);
      }
    };

    if (team == Team::blue) {
      resetVolcanoThunk(1); // blue goes first
    }
    else {
      resetVolcanoThunk(2); // then red
    }

    // On anim end, reset the timer
    volcanoErupt << "FLICKER" << Animator::Mode::Loop << [this, resetVolcanoThunk]() {
      resetVolcanoThunk(2);
    };

    volcanoSprite->setTexture(Textures().LoadFromFile("resources/tiles/volcano.png"));
    volcanoSprite->SetLayer(-1); // in front of tile

    volcanoErupt.Refresh(volcanoSprite->getSprite());

    // debugging, I'll make the tiles transparent
    // setColor(sf::Color(255, 255, 255, 100));
  }

  Tile& Tile::operator=(const Tile & other)
  {
    x = other.x;
    y = other.y;

    totalElapsed = other.totalElapsed;
    team = other.team;
    ogTeam = other.ogTeam;
    state = other.state;
    RefreshTexture();
    animation.Refresh(getSprite());
    entities = other.entities;
    setScale(2.f, 2.f);
    width = other.width;
    height = other.height;
    animState = other.animState;
    setPosition(((x - 1) * width) + START_X, ((y - 1) * (height - Y_OFFSET)) + START_Y);
    willHighlight = other.willHighlight;
    reserved = other.reserved;
    characters = other.characters;
    spells = other.spells;
    entities = other.entities;
    isTimeFrozen = other.isTimeFrozen;
    isBattleOver = other.isBattleOver;
    isBattleStarted = other.isBattleStarted;
    brokenCooldown = other.brokenCooldown;
    teamCooldown = other.teamCooldown;
    flickerTeamCooldown = other.flickerTeamCooldown;
    red_team_atlas = red_team_perm = other.red_team_perm;
    blue_team_atlas = blue_team_perm = other.blue_team_perm;
    unk_team_atlas = unk_team_perm = other.unk_team_perm;
    animation = other.animation;
    burncycle = other.burncycle;
    elapsedBurnTime = other.elapsedBurnTime;
    highlightMode = other.highlightMode;

    volcanoErupt = other.volcanoErupt;
    volcanoEruptTimer = other.volcanoEruptTimer;

    volcanoSprite->setTexture(other.volcanoSprite->getTexture());
    volcanoSprite->SetLayer(-1); // in front of tile

    volcanoErupt.Refresh(volcanoSprite->getSprite());

    return *this;
  }


  Tile::Tile(const Tile & other)
  {
    *this = other;
  }

  Tile::~Tile() {
    entities.clear();
    spells.clear();
    artifacts.clear();
    characters.clear();
  }

  void Tile::SetField(std::weak_ptr<Field> field) {
    fieldWeak = field;
  }

  const int Tile::GetX() const {
    return x;
  }

  const int Tile::GetY() const {
    return y;
  }

  Tile* Tile::GetTile(Direction dir, unsigned count)
  {
    Tile* next = this;

    while (count > 0) {
      next = next + dir;
      count--;
    }

    return next;
  }

  const Team Tile::GetTeam() const {
    return team;
  }

  void Tile::HandleMove(std::shared_ptr<Entity> entity)
  {
    // If removing an entity and the tile was broken, crack the tile
    if (reserved.size() == 0 && dynamic_cast<Character*>(entity.get()) != nullptr && (IsCracked() && !(entity->HasFloatShoe() || entity->HasAirShoe()))) {
      SetState(TileState::broken);
      Audio().Play(AudioType::PANEL_CRACK);
    }
  }

  void Tile::PerspectiveFlip(bool state)
  {
    isPerspectiveFlipped = state;
    if (state) {
      red_team_atlas = blue_team_perm;
      blue_team_atlas = red_team_perm;
    }
    else {
      red_team_atlas = red_team_perm;
      blue_team_atlas = blue_team_perm;
    }

    RefreshTexture();
    animation.Refresh(getSprite());
  }

  void Tile::SetTeam(Team _team, bool useFlicker) {
    if (ogTeam == Team::unset) {
      ogTeam = _team;
      team = _team;
    }

    if (IsEdgeTile() || state == TileState::hidden) return;

    // You cannot steal the player's back columns
    if (x == 1 || x == 6) return;

    // Check if no characters on the opposing team are on this tile
    if (GetTeam() == Team::unknown || GetTeam() != _team) {
      size_t size = FindEntities([this, _team](std::shared_ptr<Entity>& in) {
        Character* isCharacter = dynamic_cast<Character*>(in.get());
        return isCharacter && in->GetTeam() != _team;
      }).size();

      if (size == 0 && reserved.size() == 0) {
        team = _team;
        
        if(useFlicker) {
          flickerTeamCooldown = flickerTeamCooldownLength;
        }
        else {
          flickerTeamCooldown = 0; // cancel 
          teamCooldown = teamCooldownLength;
        }
      }

      RefreshTexture();
      animation.Refresh(getSprite());
    }
  }

  void Tile::SetFacing(Direction facing)
  {
    if (ogFacing == Direction::none) {
      ogFacing = facing;
    }

    this->facing = facing;
  }

  Direction Tile::GetFacing()
  {
    return facing;
  }

  float Tile::GetWidth() const {
    return width;
  }

  const size_t Tile::GetEntityCount() const
  {
    return entities.size();
  }

  float Tile::GetHeight() const {
    return height;
  }

  const TileState Tile::GetState() const {
    return state;
  }

  void Tile::SetState(TileState _state) {
    if (IsEdgeTile()) {
      return;
    }

    if (_state != TileState::hidden && state == TileState::hidden) {
      return;
    }

    if (_state == TileState::broken) {
      if(characters.size() || reserved.size()) {
        return;
      } else {
        brokenCooldown = brokenCooldownLength;
      }
    }

    if (_state == TileState::cracked && (state == TileState::empty || state == TileState::broken)) {
      return;
    }

    if (_state == TileState::volcano) {
      AddNode(volcanoSprite);
    }
    else {
      RemoveNode(volcanoSprite);
    }

    state = _state;
  }

  // Set the right texture based on the team color and state
  void Tile::RefreshTexture() {
    if (state == TileState::hidden) {
      animState = "";
      setScale(sf::Vector2f(0, 0));
      animation.SetAnimation(animState);
      return;
    }

    // Hack to toggle between team color without rewriting redundant code
    Team currTeam = team;
    Team otherTeam = (team == Team::unknown) ? Team::unknown : (team == Team::red) ? Team::blue : Team::red;
    std::string prevAnimState = animState;

    ((int)(flickerTeamCooldown * 100) % 2 == 0 && flickerTeamCooldown <= flickerTeamCooldownLength) ? currTeam : currTeam = otherTeam;

    if (state == TileState::broken) {
      // Broken tiles flicker when they regen
      animState = ((int)(brokenCooldown * 100) % 2 == 0 && brokenCooldown <= FLICKER) ? std::move(GetAnimState(TileState::normal)) : std::move(GetAnimState(state));
    }
    else {
      animState = std::move(GetAnimState(state));
    }

    if (prevAnimState != animState) {
      animation.SetAnimation(animState);
      animation << Animator::Mode::Loop;
    }

    switch (currTeam) {
    case Team::red:
      setTexture(red_team_atlas);
      break;
    case Team::blue:
      setTexture(blue_team_atlas);
      break;
    default:
      setTexture(unk_team_atlas);
    }
  }

  bool Tile::IsWalkable() const {
    return !IsHole() && !IsEdgeTile();
  }

  bool Tile::IsCracked() const {
    return state == TileState::cracked;
  }

  bool Tile::IsEdgeTile() const
  {
    return GetX() == 0 || GetX() == 7 || GetY() == 0 || GetY() == 4;
  }

  bool Tile::IsHole() const
  {
    return state == TileState::hidden || state == TileState::broken || state == TileState::empty;
  }

  bool Tile::IsHighlighted() const {
    return willHighlight;
  }

  void Tile::RequestHighlight(TileHighlight mode)
  {
    highlightMode = mode;
  }

  bool Tile::IsReservedByCharacter(std::vector<Entity::ID_t> exclude)
  {
    if (exclude.empty()) {
      return !reserved.empty();
    }

    for (Entity::ID_t reserver_id : reserved) {
      if (std::find(exclude.begin(), exclude.end(), reserver_id) == exclude.end()) {
        return true;
      }
    }

    return false;
  }

  void Tile::AddEntity(std::shared_ptr<Entity> _entity) {
    if (!_entity || ContainsEntity(_entity)) {
      return;
    }

    if (!_entity->IsOnField()) {
      throw std::runtime_error("Entity must be spawned on the field before adding directly to a tile");
    }

    if (_entity->WillEraseEOF()) {
      return;
    }

    if (Spell* spell = dynamic_cast<Spell*>(_entity.get())) {
      spells.push_back(spell);
    } else if(auto artifact = dynamic_cast<Artifact*>(_entity.get())) {
      artifacts.push_back(artifact);
    } else if(auto obstacle = std::dynamic_pointer_cast<Obstacle>(_entity)) {
      characters.push_back(obstacle);
      spells.push_back(_entity.get());
    } else if(auto character = std::dynamic_pointer_cast<Character>(_entity)) {
      characters.push_back(character);
    }

    _entity->SetTile(this);

    // May be part of the spawn routine
    // First tile set means entity is live and ready to go
    _entity->Spawn(*this);

    auto reservedIter = reserved.find(_entity->GetID());
    if (reservedIter != reserved.end()) { reserved.erase(reservedIter); }
    entities.push_back(_entity);
  }

  bool Tile::RemoveEntityByID(Entity::ID_t ID)
  {
    bool modified = false;

    if (std::shared_ptr<Field> field = fieldWeak.lock()) {
      // This is for queued entities that have not been spawned yet
      // But are requested to be removed on the same frame
      field->TileRequestsRemovalOfQueued(this, ID);
    }

    // If the entity was in the reserved list, remove it
    auto reservedIter = reserved.find(ID);
    if (reservedIter != reserved.end()) { reserved.erase(reservedIter); }

    auto itEnt    = find_if(entities.begin(), entities.end(), [ID](std::shared_ptr<Entity> in) { return in->GetID() == ID; });
    auto itSpell  = find_if(spells.begin(), spells.end(), [ID](Entity* in) { return in->GetID() == ID; });
    auto itChar   = find_if(characters.begin(), characters.end(), [ID](std::shared_ptr<Entity> in) { return in->GetID() == ID; });
    auto itArt    = find_if(artifacts.begin(), artifacts.end(), [ID](Entity* in) { return in->GetID() == ID; });
    auto itDelete = find_if(deletingCharacters.begin(), deletingCharacters.end(), [ID](Entity* in) { return in->GetID() == ID; });
    
    if (itDelete != deletingCharacters.end()) {
      deletingCharacters.erase(itDelete);
    }

    if (itEnt != entities.end()) {
      entities.erase(itEnt);
      modified = true;
    }

    if (itSpell != spells.end()) {
      spells.erase(itSpell);

      auto tagged = std::find_if(taggedAttackers.begin(), taggedAttackers.end(), [ID](Entity::ID_t in) { return ID == in; });
      if (tagged != taggedAttackers.end()) {
        taggedAttackers.erase(tagged);
      }
    }

    if (itChar != characters.end()) {
      characters.erase(itChar);
    }

    if (itArt != artifacts.end()) {
      artifacts.erase(itArt);
    }

    return modified;
  }

  bool Tile::ContainsEntity(const std::shared_ptr<Entity> _entity) const {
    return find(entities.begin(), entities.end(), _entity) != entities.end();
  }

  void Tile::ReserveEntityByID(long ID)
  {
    reserved.insert(ID);
  }

  void Tile::AffectEntities(Entity& attacker) {
    if (!attacker.HasSpawned()) {
      // should check to see if the entity is on the field, but it's cheaper to check if the entity has spawned
      throw std::runtime_error("Attacker must be on the field");
    }

    if (std::find_if(taggedAttackers.begin(), taggedAttackers.end(), [&attacker](int ID) { return ID == attacker.GetID(); }) != taggedAttackers.end())
      return;
    if (std::find_if(queuedAttackers.begin(), queuedAttackers.end(), [&attacker](int ID) { return ID == attacker.GetID(); }) != queuedAttackers.end())
      return;
    queuedAttackers.push_back(attacker.GetID());
  }

  void Tile::Update(Field& field, double _elapsed) {
    willHighlight = false;
    totalElapsed += _elapsed;

    if (!isTimeFrozen && isBattleStarted) {
      // LAVA TILES
      elapsedBurnTime -= _elapsed;

      // VOLCANO 
      volcanoEruptTimer -= _elapsed;

      if (volcanoEruptTimer <= 0) {
        volcanoErupt.Update(_elapsed, volcanoSprite->getSprite());
      }

      // set the offset to be at center origin
      volcanoSprite->setPosition(sf::Vector2f(TILE_WIDTH/2.f, 0));
    }

    // We need a copy because we WILL invalidate the iterator
    using CharacterSet = std::set<Character*, EntityComparitor>;
    const CharacterSet deletingCharsCopy = deletingCharacters;
    for (Character* character : deletingCharsCopy) {
      // Can remove the character from the tile's deleting queue
      field.UpdateEntityOnce(*character, _elapsed);
    }

    // Update our tile animation and texture
    if (!isTimeFrozen) {
      if (teamCooldown > 0) {
        teamCooldown -= 1.0 * _elapsed;
        if (teamCooldown < 0) teamCooldown = 0;
      }

      if (flickerTeamCooldown > 0) {
        flickerTeamCooldown -= 1.0 * _elapsed;
        if (flickerTeamCooldown < 0) flickerTeamCooldown = 0;
      }

      if (state == TileState::broken) {
        brokenCooldown -= 1.0f* _elapsed;

        if (brokenCooldown < 0) { brokenCooldown = 0; state = TileState::normal; }
      }
    }

    RefreshTexture();
    animation.SyncTime(from_seconds(totalElapsed));
    animation.Refresh(this->getSprite());

    switch (highlightMode) {
    case TileHighlight::solid:
      willHighlight = true;
      break;
    case TileHighlight::flash:
      willHighlight = (int)(totalElapsed * 15) % 2 == 0;
      break;
    default:
      willHighlight = false;
    }

    if (state == TileState::hidden) {
      willHighlight = false;
    }

    // animation will want to override the sprite's origin. Use setOrigin() to fix this.
    setOrigin(TILE_WIDTH / 2.0f, TILE_HEIGHT / 2.0f);
    highlightMode = TileHighlight::none;

    // Process tile behaviors
    vector<std::shared_ptr<Character>> characters_copy = characters;
    for (std::shared_ptr<Character>& character : characters_copy) {
      if (!character->IsTimeFrozen()) {
        HandleTileBehaviors(field, *character);
      }
    }
  }

  void Tile::ToggleTimeFreeze(bool state)
  {
    if (isTimeFrozen == state) return;
    isTimeFrozen = state;

    for (std::shared_ptr<Entity>& entity : entities) {
      entity->ToggleTimeFreeze(isTimeFrozen);
    }
    willHighlight = false;
  }

  void Tile::BattleStart()
  {
    for (std::shared_ptr<Entity>& e : entities) {
        e->BattleStart();
    }
    isBattleOver = false;
    isBattleStarted = true;
  }

  void Tile::BattleStop() {
    if (isBattleOver) return;

    std::vector<std::shared_ptr<Entity>> copy = entities;
    for (std::shared_ptr<Entity>& e : copy) {
      e->BattleStop();
      e->ClearActionQueue();
    }
    isBattleOver = true;
  }

  void Tile::SetupGraphics(
    std::shared_ptr<sf::Texture> redTeam, 
    std::shared_ptr<sf::Texture> blueTeam, 
    std::shared_ptr<sf::Texture> unknown, 
    const Animation& anim)
  {
    animation = anim;
    blue_team_atlas = blue_team_perm = blueTeam;
    red_team_atlas = red_team_perm = redTeam;
    unk_team_atlas = unk_team_perm = unknown;
    useParentShader = true;
  }

  void Tile::HandleTileBehaviors(Field& field, Obstacle& obst) {
    if (!isTimeFrozen || !isBattleOver || !(state == TileState::hidden)) {

      // DIRECTIONAL TILES
      Direction directional = Direction::none;
      bool notMoving = !obst.IsMoving();

      switch (GetState()) {
      case TileState::directionDown:
        directional = Direction::down;
        break;
      case TileState::directionUp:
        directional = Direction::up;
        break;
      case TileState::directionLeft:
        directional = Direction::left;
        break;
      case TileState::directionRight:
        directional = Direction::right;
        break;
      }

      if (directional != Direction::none) {
        if (obst.WillSlideOnTiles()) {
          if (!obst.HasAirShoe() && !obst.HasFloatShoe()) {
            if (!obst.IsSliding() && notMoving) {
              MoveEvent event{ frames(3), frames(0), frames(0), 0, obst.GetTile() + directional };
              obst.Entity::RawMoveEvent(event, ActionOrder::involuntary);
            }
          }
        }
      }
    }
  }

  void Tile::HandleTileBehaviors(Field& field, Character& character)
  {
    // Obstacles cannot be considered
    if (dynamic_cast<Obstacle*>(&character)) return;
    if (isTimeFrozen || state == TileState::hidden) return; 

    /*
     Special tile rules for directional pads
     Only if the entity isn't moving this frame (has a null next tile)
     and if they are not floating, we push the entity in a specific direction
     */

    if (!isBattleOver) {
      // LAVA & POISON TILES
      if (!character.HasFloatShoe()) {
        if (GetState() == TileState::poison) {
          if (elapsedBurnTime <= 0) {
            if (character.Hit(Hit::Properties({ 1, Hit::pierce, Element::none, 0, Direction::none }))) {
              elapsedBurnTime = burncycle;
            }
          }
        }
        else {
          elapsedBurnTime = 0;
        }

        if (GetState() == TileState::lava) {
          Hit::Properties props = { 50, Hit::flash | Hit::flinch, Element::none, 0, Direction::none };
          if (character.HasCollision(props)) {
            character.Hit(props);
            field.AddEntity(std::make_shared<Explosion>(), GetX(), GetY());
            SetState(TileState::normal);
          }
        }
      }

      // DIRECTIONAL TILES
      Direction directional = Direction::none;

      switch (GetState()) {
      case TileState::directionDown:
        directional = Direction::down;
        break;
      case TileState::directionUp:
        directional = Direction::up;
        break;
      case TileState::directionLeft:
        directional = Direction::left;
        break;
      case TileState::directionRight:
        directional = Direction::right;
        break;
      }

      if (directional != Direction::none) {
        bool notMoving = !character.IsMoving();
        if (character.WillSlideOnTiles()) {
          if (!character.HasAirShoe() && !character.HasFloatShoe()) {
            if (notMoving && !character.IsSliding()) {
              MoveEvent event{ frames(3), frames(0), frames(0), 0, character.GetTile() + directional };
              character.RawMoveEvent(event, ActionOrder::involuntary);
            }
          }
        }
      }
    }
  }

  std::vector<std::shared_ptr<Entity>> Tile::FindEntities(std::function<bool(std::shared_ptr<Entity>& e)> query)
  {
    std::vector<std::shared_ptr<Entity>> res;

    for(auto iter = entities.begin(); iter != entities.end(); iter++ ) {
      if (query(*iter) && (*iter)->IsHitboxAvailable()) {
        res.push_back(*iter);
      }
    }

    return res;
  }

  std::vector<std::shared_ptr<Character>> Tile::FindCharacters(std::function<bool(std::shared_ptr<Character>& e)> query)
  {
    std::vector<std::shared_ptr<Character>> res;

    for (auto iter = characters.begin(); iter != characters.end(); iter++) {
      // skip obstacle types...
      auto spell_iter = std::find_if(spells.begin(), spells.end(), [character = *iter](Entity* other) {
        return other->GetID() == character->GetID();
      });

      if (spell_iter != spells.end()) continue;

      if (query(*iter) && (*iter)->IsHitboxAvailable()) {
        res.push_back(*iter);
      }
    }

    return res;
  }

  std::vector<std::shared_ptr<Obstacle>> Tile::FindObstacles(std::function<bool(std::shared_ptr<Obstacle>& e)> query)
  {
    std::vector<std::shared_ptr<Obstacle>> res;

    for (auto iter = characters.begin(); iter != characters.end(); iter++) {
      // collect only obstacle types...
      auto spell_iter = std::find_if(spells.begin(), spells.end(), [character = *iter](Entity* other) {
        return other->GetID() == character->GetID();
      });

      if (spell_iter == spells.end()) continue;

      std::shared_ptr<Obstacle> as_obstacle = std::dynamic_pointer_cast<Obstacle>(*iter);
      if (as_obstacle && query(as_obstacle) && as_obstacle->IsHitboxAvailable()) {
        res.push_back(as_obstacle);
      }
    }

    return res;
  }

  int Tile::Distance(Battle::Tile& other)
  {
    return std::abs(other.GetX() - GetX()) + std::abs(other.GetY() - GetY());
  }

  Tile* Tile::operator+(const Direction& dir)
  {
    std::shared_ptr<Field> field = fieldWeak.lock();

    if (!field) {
      return nullptr;
    }

    switch (dir) {
    case Direction::down:
    {
      Tile* next = field->GetAt(x, y + 1);
      return next;
    }
    case Direction::down_left:
    {
      Tile* next = field->GetAt(x - 1, y + 1);
      return next;
    }
    case Direction::down_right:
    {
      Tile* next = field->GetAt(x + 1, y + 1);
      return next;
    }
    case Direction::left:
    {
      Tile* next = field->GetAt(x - 1, y);
      return next;
    }
    case Direction::right:
    {
      Tile* next = field->GetAt(x + 1, y);
      return next;
    }
    case Direction::up:
    {
      Tile* next = field->GetAt(x, y - 1);
      return next;
    }
    case Direction::up_left:
    {
      Tile* next = field->GetAt(x - 1, y - 1);
      return next;
    }
    case Direction::up_right:
    {
      Tile* next = field->GetAt(x + 1, y - 1);
      return next;
    }
    case Direction::none:
    default:
      return this;
      break;
    }

    return nullptr;
  }

  Tile* Tile::Offset(int x, int y)
  {
    std::shared_ptr<Field> field = fieldWeak.lock();

    if (!field) {
      return nullptr;
    }

    return field->GetAt(this->x + x, this->y + y);
  }

  std::string Tile::GetAnimState(const TileState state)
  {
    if (state == TileState::hidden) return "";

    std::string str = "row_" + std::to_string(4 - GetY()) + "_";

    switch (state) {
    case TileState::broken:
      str = str + "broken";
      break;
    case TileState::cracked:
      str = str + "cracked";
      break;
    case TileState::empty:
      str = str + "empty";
      break;
    case TileState::grass:
      str = str + "grass";
      break;
    case TileState::ice:
      str = str + "ice";
      break;
    case TileState::lava:
      str = str + "lava";
      break;
    case TileState::normal:
      str = str + "normal";
      break;
    case TileState::poison:
      str = str + "poison";
      break;
    case TileState::directionDown:
      str = str + "direction_down";
      break;
    case TileState::directionLeft:
      str = str + (isPerspectiveFlipped? "direction_right" : "direction_left");
      break;
    case TileState::directionRight:
      str = str + (isPerspectiveFlipped ? "direction_left" : "direction_right");
      break;
    case TileState::directionUp:
      str = str + "direction_up";
      break;
    case TileState::volcano:
      str = str + "volcano";
      break;
    case TileState::holy:
      str = str + "holy";
      break;
    default:
      str = str + "normal";
    }

    if (GetX() == 0 || GetX() == 7 || GetY() == 0 || GetY() == 4) {
      str = "row_1_normal";
    }

    return str;
  }

  void Tile::PrepareNextFrame(Field& field)
  {
    int i = 0;

    // Step through the entity bucket (all entity types)
    while (i < entities.size()) {
      std::shared_ptr<Entity>& ptr = entities[i];
      ptr->PrepareNextFrame();

      Entity::ID_t ID = ptr->GetID();

      if (ptr->IsDeleted()) {
        // TODO: make hash of entity ID to character pointers and then grab character by entity's ID...
        Character* character = dynamic_cast<Character*>(ptr.get());

        if (character && deletingCharacters.find(character) == deletingCharacters.end()) {
          field.CharacterDeletePublisher::Broadcast(*character);

          if (ptr->WillEraseEOF()) {
            // prevent this entity from being broadcast again while any deletion animations take place
            // TODO: this could be written better
            deletingCharacters.insert(character);
          }
        }
      }

      // If the entity is marked for removal
      if (ptr->WillEraseEOF()) {
        if (RemoveEntityByID(ID)) {
          // Don't track this entity anymore
          field.ForgetEntity(ID);
          continue;
        }
      }  

      i++;
    }
  }

  void Tile::ExecuteAllAttacks(Field& field)
  {
    // Spells dont cause damage when the battle is over
    if (isBattleOver) return;

    // Now that spells and characters have updated and moved, they are due to check for attack outcomes
    std::vector<std::shared_ptr<Character>> characters_copy = characters; // may be modified after hitboxes are resolved

    for (std::shared_ptr<Character>& character : characters_copy) {
      // the entity is a character (can be hit) and the team isn't the same
      // we see if it passes defense checks, then call attack

      bool retangible = false;
      DefenseFrameStateJudge judge; // judge for this character's defenses

      for (Entity::ID_t ID : queuedAttackers) {
        std::shared_ptr<Entity> attacker = field.GetEntity(ID);

        if (!attacker) {
          Logger::Logf(LogLevel::debug, "Attacker %d missing from field", ID);
          continue;
        }

        if (!character->IsHitboxAvailable())
          continue;

        if (character->GetID() == attacker->GetID()) // Case: prevent attackers from attacking themselves
          continue;

        bool unknownTeams = character->GetTeam() == Team::unknown;
        unknownTeams = unknownTeams && (attacker->GetTeam() == Team::unknown);

        if (character->GetTeam() == attacker->GetTeam() && !unknownTeams) // Case: prevent friendly fire
          continue;

        if (unknownTeams && !character->UnknownTeamResolveCollision(*attacker)) // Case: unknown vs unknown need further inspection
          continue;

        character->DefenseCheck(judge, attacker, DefenseOrder::always);

        bool alreadyTagged = false;

        if (judge.IsDamageBlocked()) {
          alreadyTagged = true;
          // Tag the attacker
          // only ignore attackers that have already hit something on a tile
          // this is similar to the hitbox being removed in mmbn mechanics
          taggedAttackers.push_back(attacker->GetID());
        }

        // Collision here means "we are able to hit" 
        // either with a hitbox that can pierce a defense or by tangibility
        Hit::Properties props = attacker->GetHitboxProperties();
        if (!character->HasCollision(props)) continue;

        // Obstacles can hit eachother, even on the same team
        // Some obstacles shouldn't collide if they come from the same enemy (like Bubbles from the same Starfish)
        bool sharesCommonAggressor = attacker->GetHitboxProperties().aggressor == character->GetHitboxProperties().aggressor;
          
        // If ICA is false or they do not share a common aggressor, let the obstacles invoke the collision effect routine
        if (!(sharesCommonAggressor && character->WillIgnoreCommonAggressor())) {
          attacker->OnCollision(character);
        }

        if (!alreadyTagged) {
          // If not collided by the earlier defense types, tag it now
          // since we have a definite collision
          taggedAttackers.push_back(attacker->GetID());
        }

        // Retangible flag takes characters out of passthrough status
        retangible = retangible || ((props.flags & Hit::retangible) == Hit::retangible);

        // The attacker passed at least one defense check
        character->DefenseCheck(judge, attacker, DefenseOrder::collisionOnly);

        if (!judge.IsDamageBlocked()) {

          // We make sure to apply any tile bonuses at this stage
          if (GetState() == TileState::holy) {
            Hit::Properties props = attacker->GetHitboxProperties();
            props.damage /= 2;
            attacker->SetHitboxProperties(props);
          }

          // Attack() routine has Hit() which immediately subtracts HP
          if (isTimeFrozen) {
            Hit::Properties props = attacker->GetHitboxProperties();
            props.flags |= Hit::shake;
            attacker->SetHitboxProperties(props);
          }

          attacker->Attack(character);

          // we restore the hitbox properties
          attacker->SetHitboxProperties(props);
        }

        judge.PrepareForNextAttack();
      } // end each spell loop

      judge.ExecuteAllTriggers();

      if (retangible) character->SetPassthrough(false);
    } // end each character loop

    // empty previous frame queue to be used this current frame
    queuedAttackers.clear();
    // taggedAttackers.clear();
  }

  void Tile::UpdateSpells(Field& field, const double elapsed)
  {
    vector<Entity*> spells_copy = spells;
    for (Entity* spell : spells_copy) {
      int request = (int)spell->GetTileHighlightMode();

      if (!spell->IsTimeFrozen()) {
        if (request > (int)highlightMode) {
          highlightMode = (TileHighlight)request;
        }

        Element hitboxElement = spell->GetElement();
        if (hitboxElement == Element::aqua && state == TileState::volcano) {
          SetState(TileState::normal);
        }

        field.UpdateEntityOnce(*spell, elapsed);
      }
    }
  }

  void Tile::UpdateArtifacts(Field& field, const double elapsed)
  {
    vector<Artifact*> artifacts_copy = artifacts;
    for (Artifact* artifact : artifacts_copy) {
      // artifacts are special effects and do not stop for TimeFreeze events
      field.UpdateEntityOnce(*artifact, elapsed);
    }
  }

  void Tile::UpdateCharacters(Field& field, const double elapsed)
  {
    vector<std::shared_ptr<Character>> characters_copy = characters;
    for (std::shared_ptr<Character>& character : characters_copy) {
      if (!character->IsTimeFrozen()) {
        // Allow user input to move them out of tiles if they are frame perfect
        field.UpdateEntityOnce(*character, elapsed);
      }
    }
  }
}
