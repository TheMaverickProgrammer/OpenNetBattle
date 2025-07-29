#ifdef BN_MOD_SUPPORT

#include "bnUserTypeCardMeta.h"
#include "../bnCardPackageManager.h"
#include "../bnGame.h"

void DefineCardMetaUserTypes(ScriptResourceManager* scriptManager, sol::state& state, sol::table& battle_namespace, std::function<void(const std::string& packageId)> setPackageId) {
  const auto& cardpropsmeta_table = battle_namespace.new_usertype<Battle::Card::Properties>("CardProperties",
    sol::meta_function::index, []( sol::table table, const std::string key ) { 
      ScriptResourceManager::PrintInvalidAccessMessage( table, "CardProperties", key );
    },
    sol::meta_function::new_index, []( sol::table table, const std::string key, sol::object obj ) { 
      ScriptResourceManager::PrintInvalidAssignMessage( table, "CardProperties", key );
    },
    "from_card", [scriptManager] (const std::string& fqn) -> Battle::Card::Properties {
      auto cardPackages = &scriptManager->GetCardPackagePartitioner().GetPartition(Game::LocalPartition);

      if (!cardPackages) {
        Logger::Log(LogLevel::critical, "Battle.CardProperties.from_card() was called but CardPackageManager was nullptr!");
        return {};
      }

      if (!cardPackages->HasPackage(fqn)) {
        Logger::Log(LogLevel::critical, "Battle.CardProperties.from_card() was called with unknown package " + fqn);
        return {};
      }

      return cardPackages->FindPackageByID(fqn).GetCardProperties();
    },
    "action", &Battle::Card::Properties::action,
    "can_boost", &Battle::Card::Properties::canBoost,
    "card_class", &Battle::Card::Properties::cardClass,
    "damage", &Battle::Card::Properties::damage,
    "description", &Battle::Card::Properties::description,
    "element", &Battle::Card::Properties::element,
    "limit", &Battle::Card::Properties::limit,
    "meta_classes", &Battle::Card::Properties::metaClasses,
    "secondary_element", &Battle::Card::Properties::secondaryElement,
    "shortname", &Battle::Card::Properties::shortname,
    "time_freeze", &Battle::Card::Properties::timeFreeze,
    "skip_time_freeze_intro", &Battle::Card::Properties::skipTimeFreezeIntro,
    "counterable", &Battle::Card::Properties::counterable,
    "long_description", &Battle::Card::Properties::verboseDescription
  );

  const auto& cardmeta_table = battle_namespace.new_usertype<CardMeta>("CardMeta",
    sol::meta_function::index, []( sol::table table, const std::string key ) { 
      ScriptResourceManager::PrintInvalidAccessMessage( table, "CardMeta", key );
    },
    sol::meta_function::new_index, []( sol::table table, const std::string key, sol::object obj ) { 
      ScriptResourceManager::PrintInvalidAssignMessage( table, "CardMeta", key );
    },
    "filter_hand_step", &CardMeta::filterHandStep,
    "get_card_props", &CardMeta::GetCardProperties,
    "set_preview_texture", &CardMeta::SetPreviewTexture,
    "set_icon_texture", &CardMeta::SetIconTexture,
    "set_codes", &CardMeta::SetCodes,
    "declare_package_id", [setPackageId] (CardMeta& meta, const std::string& packageId) {
      setPackageId(packageId);
      meta.SetPackageID(packageId);
    }
  );

  const auto& adjacent_card_table = battle_namespace.new_usertype<AdjacentCards>("AdjacentCards",
    "has_card_to_left", [](AdjacentCards& adjCards) {
      return adjCards.hasCardToLeft;
    },
    "has_card_to_right", [](AdjacentCards& adjCards) {
      return adjCards.hasCardToRight;
    },
    "discard_left", [](AdjacentCards& adjCards) {
      adjCards.deleteLeft = true;
    },
    "discard_right", [](AdjacentCards& adjCards) {
      adjCards.deleteRight = true;
    },
    "discard_incoming", [](AdjacentCards& adjCards) {
      adjCards.deleteThisCard = true;
    },
    "left_card", &AdjacentCards::leftCard,
    "right_card", &AdjacentCards::rightCard
  );

  state.new_enum("CardClass",
    "Standard", Battle::CardClass::standard,
    "Mega", Battle::CardClass::mega,
    "Giga", Battle::CardClass::giga,
    "Dark", Battle::CardClass::dark
  );
}

#endif