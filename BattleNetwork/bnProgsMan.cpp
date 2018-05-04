#include "bnProgsMan.h"
#include "bnTile.h"
#include "bnField.h"
#include "bnWave.h"
#include "bnProgBomb.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"
#include "bnEngine.h"

#define RESOURCE_NAME "progsman"
#define RESOURCE_PATH "resources/mobs/progsman/progsman.animation"
#define SHADER_FRAG_PATH "resources/shaders/white.frag.txt"

#define PROGS_COOLDOWN 1000.0f
#define PROGS_ATTACK_COOLDOWN 2222.f
#define PROGS_WAIT_COOLDOWN 100.0f
#define PROGS_ATTACK_DELAY 500.0f

#define EXPLODE_ANIMATION_SPRITES 16
#define EXPLODE_ANIMATION_WIDTH 59
#define EXPLODE_ANIMATION_HEIGHT 59

ProgsMan::ProgsMan(void)
  : animationComponent(AnimationComponent(this)) {
  Entity::team = Team::RED;
  health = 20;
  hitHeight = 0;
  direction = Direction::DOWN;
  state = MobState::MOB_IDLE;
  textureType = TextureType::MOB_PROGSMAN_IDLE;
  healthUI = new MobHealthUI(this);

  cooldown = 0;
  attackCooldown = 0;
  waitCooldown = 0;
  explosionProgress = 0.0f;
  explosionProgress2 = 0.0f;
  attackDelay = 0.0f;

  blinker = 0.0f;
  x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f;

  setTexture(*TextureResourceManager::GetInstance().GetTexture(textureType));
  setScale(2.f, 2.f);

  explosion.setTexture(*TextureResourceManager::GetInstance().GetTexture(TextureType::MOB_EXPLOSION));
  explosion.setScale(0.0f, 0.0f);
  explosion2.setTexture(*TextureResourceManager::GetInstance().GetTexture(TextureType::MOB_EXPLOSION));
  explosion2.setScale(0.0f, 0.0f);
  int i = 0;
  int y = 0;
  for (int x = 0; x < EXPLODE_ANIMATION_SPRITES; x++) {
    if (x == 8) {
      y += EXPLODE_ANIMATION_HEIGHT;
      i = 0;
    }
    explode.addFrame(0.3f, IntRect(EXPLODE_ANIMATION_WIDTH*i, y, EXPLODE_ANIMATION_WIDTH, EXPLODE_ANIMATION_HEIGHT));
    i++;
  }

  this->SetHealth(health);

  //Components setup and load
  animationComponent.setup(RESOURCE_NAME, RESOURCE_PATH);
  animationComponent.load();

  if (!whiteout.loadFromFile(SHADER_FRAG_PATH, sf::Shader::Fragment)) {
    Logger::Logf("Error loading shader: %s\n", SHADER_FRAG_PATH);
  } else {
    whiteout.setUniform("texture", sf::Shader::CurrentTexture);
  }

  target = nullptr;
}

ProgsMan::~ProgsMan(void) {
}

void ProgsMan::SetTarget(Entity* _target) {
  target = _target;
}

int* ProgsMan::getAnimOffset() {
  ProgsMan* mob = this;

  int* res = new int[2];

  res[0] = res[1] = 0;

  if (mob->GetTextureType() == TextureType::MOB_PROGSMAN_IDLE) {
    res[0] = 75;
    res[1] = 115;
  } else if (mob->GetTextureType() == TextureType::MOB_PROGSMAN_PUNCH) {
    res[0] = 125;
    res[1] = 125;
  } else if (mob->GetTextureType() == TextureType::MOB_PROGSMAN_MOVE) {
    res[0] = 75;
    res[1] = 115;
  } else if (mob->GetTextureType() == TextureType::MOB_PROGSMAN_THROW) {
    res[0] = 75;
    res[1] = 115;
  }

  return res;
}

void ProgsMan::Update(float _elapsed) {
  //Explode animation then set deleted to true once it finishes
  if (health <= 0) {
    if ((int)_elapsed * 1000 % 2 == 0) {
      SetShader(&whiteout);
    } else {
      SetShader(nullptr);
    }

    blinker = 0.0f;
    blinker += 0.01f;
    if (blinker >= 0.5f) {
      setColor(sf::Color(255, 255, 255, 128));
    } else setColor(sf::Color(255, 255, 255, 255));
    if (blinker >= 1.f) blinker = 0.0f;

    if (explosionProgress == 0.0f) {
      AudioResourceManager::GetInstance().Play(AudioType::EXPLODE);

      x1 = tile->getPosition().x - 20.0f;
      y1 = tile->getPosition().y - 35.f;
      healthUI->setScale(0.0f, 0.0f);
    }
    if (explosionProgress2 == 0.0f) {
      x2 = tile->getPosition().x;
      y2 = tile->getPosition().y - 50.0f;
    }
    explosionProgress += 0.020f;
    if (explosionProgress >= 0.3f) {
      explosionProgress2 += 0.020f;
      if (explosionProgress >= 0.9f) {
        setScale(0.0f, 0.0f);
      }
      if (explosionProgress2 >= 1.f) {
        deleted = true;
        return;
      }
      explosion2.setScale(2.f, 2.f);
      explosion2.setPosition(x2, y2);
      explode(explosion2, explosionProgress2);
    }

    if (explosionProgress <= 1.f) {
      explosion.setScale(2.f, 2.f);
      explosion.setPosition(x1, y1);
      explode(explosion, explosionProgress);
    }
    return;
  }

  //Cooldown until mob's movement catches up to actual position (avoid walking through spells)
  if (previous) {
    if (previous->IsCracked()) previous->SetState(TileState::BROKEN);
    previous->RemoveEntity(this);
    previous = nullptr;
  }

  //Cooldown between moving and attacking
  waitCooldown += _elapsed;
  if (waitCooldown >= PROGS_WAIT_COOLDOWN) {
    cooldown += _elapsed;
    attackCooldown += _elapsed;
    if (cooldown >= PROGS_COOLDOWN) {
      if (state != MobState::MOB_ATTACKING) {
        if (Move(direction)) {
          cooldown = 0.0f;
          waitCooldown = 0;
          state = MobState::MOB_MOVING;
        }
      }
    } else if (attackCooldown >= PROGS_ATTACK_COOLDOWN) {
      Tile* forward = field->GetAt(tile->GetX() - 1, tile->GetY());

      if (state == MobState::MOB_IDLE) {
        int random = rand() % 5;

        if (random == 4) {
          state = MobState::MOB_THROW;

          // Todo: spell that begins at pos as a ball 
          // and moves to target tile when anim is over 
        } else if (forward->IsWalkable()) {
          state = MobState::MOB_ATTACKING;
        } else {
          state = MobState::MOB_MOVING;
          cooldown = 0;
          attackCooldown = 0.0f;
        }
      }
    } else {
      if (cooldown >= 0.5f)
        state = MobState::MOB_IDLE;
    }
  }

  //Delay for animation to look cooler (only do wave at the end of animation)
  if (state == MobState::MOB_ATTACKING) {
    attackDelay += _elapsed;
    if (attackDelay >= PROGS_ATTACK_DELAY) {
      Attack();
      waitCooldown = 0;
      attackDelay = 0.0f;
      attackCooldown = 0;
      cooldown = 0;
    }
  }

  //Delay for animation to look cooler (only throw item at end of animation)
  if (state == MobState::MOB_THROW) {
    attackDelay += _elapsed;
    if (attackDelay >= PROGS_ATTACK_DELAY) {
      Attack();
      waitCooldown = 0;
      attackDelay = 0.0f;
      attackCooldown = 0;
      cooldown = 0;
    }
  }

  RefreshTexture();
  healthUI->Update();
  SetShader(nullptr);
  Entity::Update(_elapsed);
}

bool ProgsMan::Move(Direction _direction) {
  bool moved = false;
  Tile* temp = tile;
  Tile* next = nullptr;

  int random = rand() % 4;

  switch (random) {
  case 0:
    _direction = Direction::UP;
    break;
  case 1:
    _direction = Direction::LEFT;
    break;
  case 2:
    _direction = Direction::RIGHT;
    break;
  case 3:
    _direction = Direction::DOWN;
    break;
  }

  if (_direction == Direction::UP) {
    if (tile->GetY() - 1 > 0) {
      next = field->GetAt(tile->GetX(), tile->GetY() - 1);
      if (Teammate(next->GetTeam()) && next->IsWalkable())
        if (!next->ContainsEntityType<ProgsMan>()) {
          SetTile(next);
        } else {
          next = nullptr;
          direction = Direction::DOWN;
        } else
          next = nullptr;
    } else {
      direction = Direction::DOWN;
    }
  } else if (_direction == Direction::LEFT) {
    if (tile->GetX() - 1 > 0) {
      next = field->GetAt(tile->GetX() - 1, tile->GetY());
      if (Teammate(next->GetTeam()) && next->IsWalkable())
        SetTile(next);
      else
        next = nullptr;
    }
  } else if (_direction == Direction::DOWN) {
    if (tile->GetY() + 1 <= (int)field->GetHeight()) {
      next = field->GetAt(tile->GetX(), tile->GetY() + 1);
      if (Teammate(next->GetTeam()) && next->IsWalkable())
        if (!next->ContainsEntityType<ProgsMan>()) {
          SetTile(next);
        } else {
          next = nullptr;
          direction = Direction::UP;
        } else
          next = nullptr;
    } else {
      direction = Direction::UP;
    }
  } else if (_direction == Direction::RIGHT) {
    if (tile->GetX() + 1 <= (int)field->GetWidth()) {
      next = field->GetAt(tile->GetX() + 1, tile->GetY());
      if (Teammate(next->GetTeam()) && next->IsWalkable())
        SetTile(next);
      else
        next = nullptr;
    }
  }

  if (next) {
    previous = temp;
    moved = true;
  }

  tile->AddEntity(this);
  return moved;
}

void ProgsMan::Attack() {
  if (state == MobState::MOB_THROW) {
    if (target != nullptr) {
      Tile* targetTile = target->GetTile();
      Tile* aimTile = field->GetAt(targetTile->GetX(), targetTile->GetY());
      ProgBomb* spell = new ProgBomb(field, team, aimTile, 1.0);
      spell->SetDirection(Direction::LEFT);
      field->AddEntity(spell, tile->GetX(), tile->GetY());
      spell->PrepareThrowPath();
    }
  } else {
    if (tile->GetX() + 1 <= (int)field->GetWidth() + 1) {
      Spell* spell = new Wave(field, team);
      spell->SetDirection(Direction::LEFT);
      field->AddEntity(spell, tile->GetX() - 1, tile->GetY());

      Tile* next = 0;
      next = field->GetAt(tile->GetX() - 1, tile->GetY());

      Entity* entity = 0;

      while (next->GetNextEntity(entity)) {
        Player* isPlayer = dynamic_cast<Player*>(entity);

        if (isPlayer) {
          isPlayer->Move(Direction::LEFT);
          isPlayer->Hit(20);
        }
      }
    }
  }
}

void ProgsMan::RefreshTexture() {
  if (state == MobState::MOB_IDLE) {
    textureType = TextureType::MOB_PROGSMAN_IDLE;
  } else if (state == MobState::MOB_MOVING) {
    textureType = TextureType::MOB_PROGSMAN_MOVE;
  } else if (state == MobState::MOB_ATTACKING) {
    textureType = TextureType::MOB_PROGSMAN_PUNCH;
  } else if (state == MobState::MOB_THROW) {
    textureType = TextureType::MOB_PROGSMAN_THROW;
  }

  setTexture(*TextureResourceManager::GetInstance().GetTexture(textureType));

  if (textureType == TextureType::MOB_PROGSMAN_IDLE) {
    setPosition(tile->getPosition().x + tile->GetWidth() / 2.0f - 65.0f, tile->getPosition().y + tile->GetHeight() / 2.0f - 115.0f);
    hitHeight = getLocalBounds().height;
  } else if (textureType == TextureType::MOB_PROGSMAN_MOVE) {
    setPosition(tile->getPosition().x + tile->GetWidth() / 2.0f - 65.0f, tile->getPosition().y + tile->GetHeight() / 2.0f - 125.0f);
  } else if (textureType == TextureType::MOB_PROGSMAN_PUNCH) {
    setPosition(tile->getPosition().x + tile->GetWidth() / 2.0f - 115.0f, tile->getPosition().y + tile->GetHeight() / 2.0f - 125.0f);
    hitHeight = getLocalBounds().height;
  } else if (textureType == TextureType::MOB_PROGSMAN_THROW) {
    setPosition(tile->getPosition().x + tile->GetWidth() / 2.0f - 115.0f, tile->getPosition().y + tile->GetHeight() / 2.0f - 125.0f);
    hitHeight = getLocalBounds().height;
  }
}

vector<Drawable*> ProgsMan::GetMiscComponents() {
  vector<Drawable*> drawables = vector<Drawable*>();
  drawables.push_back(healthUI);

  if (explosionProgress > 0.0f) {
    drawables.push_back(&explosion);
  }
  if (explosionProgress2 > 0.0f) {
    drawables.push_back(&explosion2);
  }

  return drawables;
}

int ProgsMan::GetStateFromString(string _string) {
  int size = 5;
  string MOB_STATE_STRINGS[] = { "MOB_MOVING", "MOB_IDLE", "MOB_HIT", "MOB_ATTACKING", "MOB_THROW" };
  for (int i = 0; i < size; i++) {
    if (_string == MOB_STATE_STRINGS[i]) {
      return static_cast<MobState>(i);
    }
  }
  Logger::Failf("Failed to find corresponding enum: %s\n", _string);
  return -1;
}

TextureType ProgsMan::GetTextureType() const {
  return textureType;
}

MobState ProgsMan::GetMobState() const {
  return state;
}

int ProgsMan::GetHealth() const {
  return health;
}

void ProgsMan::SetHealth(int _health) {
  health = _health;
}

int ProgsMan::Hit(int _damage) {
  SetShader(&whiteout);
  (health - _damage < 0) ? health = 0 : health -= _damage;
  return health;
}

float ProgsMan::GetHitHeight() const {
  return hitHeight;
}