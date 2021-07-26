#include "bnThunder.h"
#include "bnTile.h"
#include "bnField.h"
#include "bnObstacle.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"

Thunder::Thunder(Team _team) : Spell(_team) {
  SetLayer(0);

  setTexture(Textures().GetTexture(TextureType::SPELL_THUNDER));
  setScale(2.f, 2.f);

  elapsed = 0;

  HighlightTile(Battle::Tile::Highlight::solid);
  
  // Thunder is removed in roughly 7 seconds
  timeout = sf::seconds(20.f / 3.f);

  animation = Animation("resources/spells/thunder.animation");
  animation.SetAnimation("DEFAULT");
  animation << Animator::Mode::Loop;

  Audio().Play(AudioType::THUNDER);

  animation.Update(0, getSprite());
}

Thunder::~Thunder() {
}

void Thunder::OnUpdate(double _elapsed) {

  if (elapsed > timeout.asSeconds()) {
    Delete();
  }

  elapsed += _elapsed;
  
  // The origin is the center of the sprite. Raise thunder upwards 15 pixels 
  // (keep in mind scale is 2, e.g. 15 * 2 = 30)
  setPosition(tile->getPosition().x + tileOffset.x, tile->getPosition().y + tileOffset.y - 30.0f);

  animation.Update(_elapsed, getSprite());

  // Find target if we don't have one
  if (!target) {
    // Find all characters that are not on our team and not an obstacle
    auto query = [&](Entity* e) {
        return (e->GetTeam() != team && dynamic_cast<Character*>(e) && !dynamic_cast<Obstacle*>(e));
    };

    auto list = field->FindEntities(query);

    for (auto l : list) {
      if (!target) { 
        target = l; 
      }
      else {
        // If the distance to one enemy is shorter than the other, target the shortest enemy path
        int currentDist = abs(tile->GetX() - l->GetTile()->GetX()) + abs(tile->GetY() - l->GetTile()->GetY());
        int targetDist = abs(tile->GetX() - target->GetTile()->GetX()) + abs(tile->GetY() - target->GetTile()->GetY());

        if (currentDist < targetDist) {
          target = l;
        }
      }
    }

    // We have found a target
    // Create a notifier so we can null the target when they are deleted
    if (target) {
      auto targetRemoveCallback = [](Entity& target, Entity& observer) {
        Thunder& self = dynamic_cast<Thunder&>(observer);
        self.target = nullptr;
      };

      field->NotifyOnDelete(target->GetID(), this->GetID(), targetRemoveCallback);
    }
  }

  // If sliding is flagged to false, we know we've ended a move
  auto direction = GetDirection();
  if (!IsSliding()) {
    if (target) {
      if (target->GetTile()) {
        if (target->GetTile()->GetX() < tile->GetX()) {
          direction = Direction::left;
        }
        else if (target->GetTile()->GetX() > tile->GetX()) {
          direction = Direction::right;
        }
        else if (target->GetTile()->GetY() < tile->GetY()) {
          direction = Direction::up;
        }
        else if (target->GetTile()->GetY() > tile->GetY()) {
          direction = Direction::down;
        }

        // Poll if target is flagged for deletion, remove our mark
        if (target->IsDeleted()) {
          target = nullptr;
        }
      }
    }
    else {
      // If there are no targets, aimlessly move right or left
      // depending on the team
      if (GetTeam() == Team::red) {
        direction = Direction::right;
      }
      else {
        direction = Direction::left;
      }
    }

    if(direction != GetDirection()) {
      SetDirection(direction);
    }

    // Always slide to the tile we're moving to
    if (!IsSliding()) {
      Slide(GetTile() + GetDirection(), frames(60), frames(0));
    }
  }

  // Always affect the tile we're occupying
  tile->AffectEntities(this);
}

bool Thunder::CanMoveTo(Battle::Tile* tile) {
  return true;
}

void Thunder::Attack(Character* _entity) {
  // Only attack entities on this tile that are not on our team
  if (_entity && _entity->GetTeam() != GetTeam()) {
    Hit::Properties props;
    
    // Thunder stuns and recoils
    props.flags |= Hit::flinch | Hit::stun;
    
    // Thunder has electric properties
    props.element = Element::elec;
    
    // Attack does 40 units of damage
    props.damage = 40;

    // If entity was successfully hit
    if (_entity->Hit(props)) {
      // Mark us for deletion
      Delete();
    }
  }
}

void Thunder::OnDelete()
{
  Remove();
}
