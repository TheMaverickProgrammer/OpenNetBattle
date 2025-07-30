#ifdef BN_MOD_SUPPORT
#include "bnUserTypeBasicCharacter.h"

#include "bnWeakWrapper.h"
#include "bnUserTypeEntity.h"
#include "bnScriptedCardAction.h"
#include "../bnCharacter.h"

void DefineBasicCharacterUserType(sol::table& battle_namespace) {
  auto table = battle_namespace.new_usertype<WeakWrapper<Character>>("BasicCharacter",
    "card_action_event", sol::overload(
      [](WeakWrapper<Character>& character, WeakWrapper<ScriptedCardAction>& cardAction, ActionOrder order) {
        character.Unwrap()->AddAction(CardEvent{ cardAction.UnwrapAndRelease() }, order);
      },
      [](WeakWrapper<Character>& character, WeakWrapper<CardAction>& cardAction, ActionOrder order) {
        character.Unwrap()->AddAction(CardEvent{ cardAction.UnwrapAndRelease() }, order);
      }
    ),
    "can_attack", [](WeakWrapper<Character>& character) {
      auto characterPtr = character.Unwrap();
      return characterPtr->CanAttack();
    },
    "get_rank", [](WeakWrapper<Character>& character) -> Character::Rank {
      return character.Unwrap()->GetRank();
    }
  );

  DefineEntityFunctionsOn(table);
}
#endif