#include "bnMettaurAttackState.h"
#include "bnWave.h"
#include "bnTile.h"
#include "bnField.h"
#include "bnMettaurIdleState.h"
#include "bnOwnedCardsUI.h"

MettaurAttackState::MettaurAttackState() : AIState<Mettaur>() { ; }
MettaurAttackState::~MettaurAttackState() { ; }

void MettaurAttackState::OnEnter(Mettaur& met) {
  auto metPtr = &met;
  int r = rand() % 10;

  // proof of concept using cards
  if (SelectedCardsUI* ui = met.GetFirstComponent<OwnedCardsUI>(); ui && ui->Peek().has_value() && r > 1) {
    ui->UseNextCard();
    this->usingCard = true;
  }
  else {
    auto onAttack = [this, metPtr]() {DoAttack(*metPtr); };
    auto onFinish = [this, metPtr]() { metPtr->ChangeState<MettaurIdleState>(); };

    auto& animation = *met.GetFirstComponent<AnimationComponent>();

    animation.SetAnimation("ATTACK", onFinish);

    animation.AddCallback(10, onAttack, true);
    animation.SetCounterFrameRange(6, 11);
  }
}

void MettaurAttackState::OnUpdate(double _elapsed, Mettaur& met) {
  if (this->usingCard) {
    met.IsLockoutAnimationComplete();
    met.ChangeState<MettaurIdleState>();
  }
}

void MettaurAttackState::OnLeave(Mettaur& met) { 
  met.EndMyTurn();
}

void MettaurAttackState::DoAttack(Mettaur& met) {
  if (met.GetField()->GetAt(met.tile->GetX() - 1, met.tile->GetY())->IsWalkable()) {
    double speed = 1.0;
    int damage = 10;

    switch (met.GetRank()) {
    case Mettaur::Rank::SP: {
      speed = 1.2;
      damage = 40;
    }
      break;
    case Mettaur::Rank::Rare2: {
      speed = 1.6;
      damage = 301;
    }
      break;
    default:
      break;
    }

    Wave* spell = new Wave(met.team, speed, damage);

    auto props = spell->GetHitboxProperties();
    props.aggressor = met.GetID();
    spell->SetHitboxProperties(props);

    if (met.GetRank() == Mettaur::Rank::SP) {
      spell->CrackTiles(true);
    }
    else if (met.GetRank() == Mettaur::Rank::Rare2) {
      spell->PoisonTiles(true);
    }

    spell->SetDirection(Direction::left);
    met.field->AddEntity(*spell, met.tile->GetX() - 1, met.tile->GetY());
  }
}
