#include "bnMettaur.h"
#include "bnMettaurIdleState.h"
#include "bnMettaurAttackState.h"
#include "bnMettaurMoveState.h"
#include "bnExplodeState.h"
#include "bnTile.h"
#include "bnField.h"
#include "bnWave.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"
#include "bnDefenseVirusBody.h"
#include "bnGame.h"

const std::string RESOURCE_PATH = "resources/mobs/mettaur/mettaur.animation";

Mettaur::Mettaur(Rank _rank) :  
  AI<Mettaur>(this), 
  TurnOrderTrait<Mettaur>(), 
  Character(_rank) {
  name = "Mettaur";
  SetTeam(Team::blue);

  animation = CreateComponent<AnimationComponent>(this);

  if (GetRank() == Rank::_2) {
    SetHealth(80);
    setTexture(Textures().LoadTextureFromFile("resources/mobs/mettaur/mettaur2.png"));
  }
  else if (GetRank() == Rank::_3) {
    SetHealth(120);
    setTexture(Textures().LoadTextureFromFile("resources/mobs/mettaur/mettaur3.png"));
  } else if (GetRank() == Rank::SP) {
    SetHealth(200);
    setTexture(Textures().LoadTextureFromFile("resources/mobs/mettaur/mettaur4.png"));
  }
  else if (GetRank() == Rank::Rare1) {
    SetHealth(300);
    setTexture(Textures().LoadTextureFromFile("resources/mobs/mettaur/mettaur5.png"));
  }
  else if (GetRank() == Rank::Rare2) {
    SetHealth(500);
    setTexture(Textures().LoadTextureFromFile("resources/mobs/mettaur/mettaur6.png"));
  }
  else {
    // Rank 1
    SetHealth(40);
    setTexture(Textures().LoadTextureFromFile("resources/mobs/mettaur/mettaur.png"));
  }

  //Components setup and load
  animation->SetPath(RESOURCE_PATH);
  animation->Reload();
  animation->SetAnimation("IDLE");

  setScale(2.f, 2.f);

  SetHealth(health);

  animation->OnUpdate(0);

  virusBody = new DefenseVirusBody();
  AddDefenseRule(virusBody);
  SetHeight(getSprite().getGlobalBounds().height);
}

Mettaur::~Mettaur() {
  delete virusBody;
}

void Mettaur::OnDelete() {
  RemoveDefenseRule(virusBody);
  ChangeState<ExplodeState<Mettaur>>();
}

void Mettaur::OnUpdate(double _elapsed) {
  setPosition(tile->getPosition().x, tile->getPosition().y);
  setPosition(getPosition() + tileOffset);

  AI<Mettaur>::Update(_elapsed);
}

const float Mettaur::GetHeight() const {
  return 22.f;
}