#pragma once

#include <string>
#include <tuple>
#include <vector>
#include "bnElements.h"

using std::string;

class BattleSceneBase;
class SelectedCardsUI;

/**
 * @class Card
 * @author mav
 * @date 05/05/19
 * @brief Describes card record entry in database of loaded cards
 * 
 * This will be expanded to load the corresponding script information
 * 
 */
namespace Battle {
  enum class CardClass : unsigned {
    standard = 1,
    mega,
    giga,
    dark
  };

  class Card {
  public:
    struct Properties {
      std::string uuid;
      unsigned damage{ 0 };
      unsigned limit{ 0 };
      char code{ '*' };
      bool canBoost{ true }; /*!< Can this card be boosted by other cards? */
      bool timeFreeze{ false }; /*!< Does this card rely on action items to resolve before resuming the battle scene? */
      bool skipTimeFreezeIntro{ false }; /*! Skips the fade in/out and name appearing for this card */
      bool counterable{ true };  /*!< During the tf intro, can this card be countered? */
      string shortname;
      string action; 
      string description;
      string verboseDescription;
      Element element{ Element::none }, secondaryElement{ Element::none };
      CardClass cardClass{ CardClass::standard };
      std::vector<std::string> metaClasses; /*!< Cards can be tagged with additional user information*/
    };

    /**
      * @brief Cards are not designed to have default or partial data. Must provide all at once.
      */
    Card(const Card::Properties& props);

    /**
      * @brief copies card data
      */
    Card(const Battle::Card& copy);

    /**
      * @brief card with no data
      */
    Card();

    ~Card();

    Card::Properties& GetProps(); // Modded props
    Card::Properties& GetBaseProps(); // Unmodded props

    // const qualified
    const Card::Properties& GetProps() const;
    const Card::Properties& GetBaseProps() const;

    /**
      * @brief Get extra card description. Shows up on library.
      * @return string
      */
    const string GetVerboseDescription() const;

    /**
      * @brief Returns short description. shows up on card.
      * @return string
      */
    const string GetDescription() const;

    /**
      * @brief Name of card
      * @return string
      */
    const string GetShortName() const;

    /**
      * @brief Code of card
      * @return char
      */
    const char GetCode() const;

    /**
      * @brief Strength of card
      * @return signed
      */
    const signed GetDamage() const;

    const CardClass GetClass() const;

    const unsigned GetLimit() const;

    const std::string GetAction() const;

    const bool CanBoost() const;

    /**
      * @brief Card ID of card
      * @return string
      */
    const string GetUUID() const;

    /**
      * @brief Element of card
      * @return Element
      */
    const Element GetElement() const;

    /**
    * @brief Secondary element of card
    * @return Element
    *
    * @warning This element is used internally. If this is the same as the primary element, this element returns None
    */
    const Element GetSecondaryElement() const;

    /**
    * @brief Query if card summons one or more navis
    * @returns true if flagged as a navi card, false otherwise
    */
    const bool IsNaviSummon() const;

    /**
    * @brief Query if card should freeze time
    * @returns true if flagged as a time freeze card, false otherwise
    */
    const bool IsTimeFreeze() const;

    /**
    * @brief Query if card is tagged with user-defined information
    * @returns true if flagged as input meta type, false otherwise
    */
    const bool IsTaggedAs(const std::string& meta) const;

    /**
    * @brief Comparator for std map
    */
    struct Compare
    {
      bool operator()(const Battle::Card& lhs, const Battle::Card& rhs) const noexcept;
    };

    /**
    * @brief determine if cards are equal (by name and code)
    */
    const bool operator==(const Battle::Card& rhs) const noexcept {
      return std::tie(props.shortname, props.code) == std::tie(rhs.props.shortname, rhs.props.code);
    }

    /**
    * @brief determine if this card is less than another (by name and code)
    */
    const bool operator<(const Battle::Card& rhs) const noexcept {
      return std::tie(props.shortname, props.code) < std::tie(rhs.props.shortname, rhs.props.code);
    }

    void ModDamage(int modifier);
    void MultiplyDamage(unsigned int multiplier);
    const unsigned GetMultiplier() const;

    friend struct Compare;

  private:
    Properties unmodded;
    Properties props;
    unsigned int multiplier{ 1 };
  };
}