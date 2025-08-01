#ifdef BN_MOD_SUPPORT
#include "bnUserTypeBaseCardAction.h"

#include "bnWeakWrapper.h"
#include "bnWeakWrapperChild.h"
#include "bnUserTypeAnimation.h"
#include "bnScriptedCharacter.h"
#include "bnScriptedPlayer.h"
#include "../bnCardAction.h"
#include "../bnBusterCardAction.h"
#include "../bnScriptResourceManager.h"
#include "../bnSolHelpers.h"

using CardActionAttachmentWrapper = WeakWrapperChild<CardAction, CardAction::Attachment>;

template<typename TemplateCardAction, typename ...Args> 
static WeakWrapper<CardAction> construct(std::shared_ptr<Character> character, Args&&... args) {
  auto cardAction = std::make_shared<TemplateCardAction>(character, std::forward<Args>(args)...);

  auto wrappedCardAction = WeakWrapper<CardAction>(std::static_pointer_cast<CardAction>(cardAction));
  wrappedCardAction.Own();
  return wrappedCardAction;
}

void DefineBaseCardActionUserType(sol::state& state, sol::table& battle_namespace) {
  // make sure to copy changes to bnUserTypeScriptedCardAction
  const auto& card_action_record = battle_namespace.new_usertype<WeakWrapper<CardAction>>("BaseCardAction",
    sol::meta_function::index, []( sol::table table, const std::string key ) { 
      ScriptResourceManager::PrintInvalidAccessMessage( table, "BaseCardAction", key );
    },
    sol::meta_function::new_index, []( sol::table table, const std::string key, sol::object obj ) { 
      ScriptResourceManager::PrintInvalidAssignMessage( table, "BaseCardAction", key );
    },
    "set_lockout", [](WeakWrapper<CardAction>& cardAction, const CardAction::LockoutProperties& props) {
      cardAction.Unwrap()->SetLockout(props);
    },
    "set_lockout_group", [](WeakWrapper<CardAction>& cardAction, const CardAction::LockoutGroup& group) {
      cardAction.Unwrap()->SetLockoutGroup(group);
    },
    "override_animation_frames", [](WeakWrapper<CardAction>& cardAction, std::list<OverrideFrame> frameData) {
      cardAction.Unwrap()->OverrideAnimationFrames(frameData);
    },
    "add_attachment", [](WeakWrapper<CardAction>& cardAction, const std::string& point) -> CardActionAttachmentWrapper {
      auto& attachment = cardAction.Unwrap()->AddAttachment(point);
      return CardActionAttachmentWrapper(cardAction.GetWeak(), attachment);
    },
    "add_anim_action", [](WeakWrapper<CardAction>& cardAction, int frame, sol::object actionObject) {
      ExpectLuaFunction(actionObject);

      cardAction.Unwrap()->AddAnimAction(frame, [actionObject]{
        sol::protected_function action = actionObject;
        auto result = action();

        if (!result.valid()) {
          sol::error error = result;
          Logger::Log(LogLevel::critical, error.what());
        }
      });
    },
    "add_step", [](WeakWrapper<CardAction>& cardAction, WeakWrapper<CardAction::Step> step) {
      cardAction.Unwrap()->AddStep(step.UnwrapAndRelease());
    },
    "end_action", [](WeakWrapper<CardAction>& cardAction) {
      cardAction.Unwrap()->EndAction();
    },
    "get_actor", [](WeakWrapper<CardAction>& cardAction) -> WeakWrapper<Character> {
      return WeakWrapper(cardAction.Unwrap()->GetActor());
    },
    "set_metadata", [](WeakWrapper<CardAction>& cardAction, const Battle::Card::Properties& meta) {
      cardAction.Unwrap()->SetMetaData(Battle::Card(meta));
    },
    "copy_metadata", [](WeakWrapper<CardAction>& cardAction) -> Battle::Card::Properties {
      return cardAction.Unwrap()->GetMetaData().GetProps();
    }
  );

  battle_namespace.new_usertype<CardActionAttachmentWrapper>("Attachment",
    sol::meta_function::index, []( sol::table table, const std::string key ) { 
      ScriptResourceManager::PrintInvalidAccessMessage( table, "Attachment", key );
    },
    sol::meta_function::new_index, []( sol::table table, const std::string key, sol::object obj ) { 
      ScriptResourceManager::PrintInvalidAssignMessage( table, "Attachment", key );
    },
    "add_attachment", [](CardActionAttachmentWrapper& wrapper, const std::string& node) -> CardActionAttachmentWrapper {
      return CardActionAttachmentWrapper(wrapper.GetParentWeak(), wrapper.Unwrap().AddAttachment(node));
    },
    "sprite", [](CardActionAttachmentWrapper& wrapper) -> WeakWrapper<SpriteProxyNode> {
      return wrapper.Unwrap().GetSpriteNode();
    },
    "get_animation", [](CardActionAttachmentWrapper& wrapper) -> AnimationWrapper {
      return AnimationWrapper(wrapper.GetParentWeak(), wrapper.Unwrap().GetAnimationObject());
    }
  );

  battle_namespace.new_usertype<WeakWrapper<CardAction::Step>>("Step",
    sol::factories([]() -> WeakWrapper<CardAction::Step> {
      auto step = std::make_shared<CardAction::Step>();

      auto wrappedStep = WeakWrapper(step);
      wrappedStep.Own();
      return wrappedStep;
    }),
    sol::meta_function::index, []( sol::table table, const std::string key ) { 
      ScriptResourceManager::PrintInvalidAccessMessage( table, "Step", key );
    },
    sol::meta_function::new_index, []( sol::table table, const std::string key, sol::object obj ) { 
      ScriptResourceManager::PrintInvalidAssignMessage( table, "Step", key );
    },
    "update_func", sol::property(
      // write only, reading might cause lifetime issues
      [](WeakWrapper<CardAction::Step> step, sol::object callbackObject) {
        ExpectLuaFunction(callbackObject);

        step.Unwrap()->updateFunc = [callbackObject] (std::shared_ptr<CardAction::Step> step, double dt) {
          sol::protected_function callback = callbackObject;

          auto result = callback(WeakWrapper(step), dt);

          if (!result.valid()) {
            sol::error error = result;
            Logger::Log(LogLevel::critical, error.what());
          }
        };
      }
    ),
    "draw_func", sol::property(
      // write only, reading might cause lifetime issues
      [](WeakWrapper<CardAction::Step> step, sol::object callbackObject) {
        ExpectLuaFunction(callbackObject);

        step.Unwrap()->drawFunc = [callbackObject] (std::shared_ptr<CardAction::Step> step, sf::RenderTexture& rt) {
          sol::protected_function callback = callbackObject;

          auto result = callback(WeakWrapper(step), rt);

          if (!result.valid()) {
            sol::error error = result;
            Logger::Log(LogLevel::critical, error.what());
          }
        };
      }
    ),
    "complete_step", [](WeakWrapper<CardAction::Step> step) {
      step.Unwrap()->CompleteStep();
    }
  );

  battle_namespace.new_usertype<BusterCardAction>("Buster",
    sol::factories(
      [](WeakWrapper<Character> character, bool charged, int dmg) -> WeakWrapper<CardAction>
          { return construct<BusterCardAction>(character.Unwrap(), charged, dmg); },
      [](WeakWrapper<ScriptedCharacter> character, bool charged, int dmg) -> WeakWrapper<CardAction>
          { return construct<BusterCardAction>(character.Unwrap(), charged, dmg); },
      [](WeakWrapper<ScriptedPlayer> character, bool charged, int dmg) -> WeakWrapper<CardAction>
          { return construct<BusterCardAction>(character.Unwrap(), charged, dmg); }
    )
  );

  state.set_function("make_animation_lockout",
    []() { return CardAction::LockoutProperties{ CardAction::LockoutType::animation }; }
  );

  state.set_function("make_async_lockout",
    [](double cooldown){ return CardAction::LockoutProperties{ CardAction::LockoutType::async, cooldown }; }
  );

  state.set_function("make_sequence_lockout",
    [](){ return CardAction::LockoutProperties{ CardAction::LockoutType::sequence }; }
  );

  state.new_enum("LockType",
    "Animation", CardAction::LockoutType::animation,
    "Async", CardAction::LockoutType::async,
    "Sequence", CardAction::LockoutType::sequence
  );

  state.new_enum("Lockout",
    "Weapons", CardAction::LockoutGroup::weapon,
    "Cards", CardAction::LockoutGroup::card,
    "Abilities", CardAction::LockoutGroup::ability
  );
}
#endif
