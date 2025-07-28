/*! \brief Optionally displays the cards over the character and publishes on behalf of the character
 */
#pragma once
#include <SFML/Graphics.hpp>
#include <sstream>
#include <vector>
#include <optional>
#include "bnUIComponent.h"
#include "bnCardUsePublisher.h"
#include "bnSpriteProxyNode.h"
#include "bnText.h"

using std::ostringstream;
using std::vector;
using sf::Sprite;
using sf::Texture;
using sf::Drawable;

class Card;
class Character;
class CardPackagePartitioner;
class BattleSceneBase;

class SelectedCardsUI : public CardActionUsePublisher, public UIComponent {
public:
  /**
   * \param character Character to attach to
   */
  SelectedCardsUI(std::weak_ptr<Character> owner, CardPackagePartitioner* partition);
  
  /**
   * @brief destructor
   */
  virtual ~SelectedCardsUI();

  void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

  /**
   * @brief Hold START to spread the cards out
   * @param _elapsed in seconds
   */
  void OnUpdate(double _elapsed) override;
  
  /**
   * @brief Set the cards. Updates card cursor to 0.
   * @param incoming List of cards
   */
  void LoadCards(std::vector<Battle::Card> incoming);
  
  /**
   * @brief Broadcasts the card at the cursor curr. Increases curr.
   * @return True if there was a card to use
   */
  virtual bool UseNextCard();

  /**
 * @brief Broadcasts the card information to all listeners
 * @param card being used
 * @param user using the card
 */
  virtual void Broadcast(std::shared_ptr<CardAction> action);

  /**
   * @brief Does nothing at this time
   */
  void Inject(BattleSceneBase&) override;

  /**
  * @brief Increases the next card's damage property
  */
  void SetMultiplier(unsigned mult);

  /**
  * @brief Return a const reference to the next card, if valid
  * @preconditions Assumes the card can be used and currCard < cardCount!
  */
  std::optional<std::reference_wrapper<const Battle::Card>> Peek();

  //!< Returns true if there was a card to play, false if empty
  bool HandlePlayEvent(std::shared_ptr<Character> from);
  void DropNextCard();

  /**
  * @brief Returns the uuids of all the remaining cards
  */
  std::vector<Battle::Card> GetRemainingCards();

protected:

  const int GetCurrentCardIndex() const;
  const unsigned GetMultiplier() const;
  std::vector<Battle::Card>& GetSelectedCards() const;
  SpriteProxyNode& IconNode() const;
  SpriteProxyNode& FrameNode() const;
  CardPackagePartitioner* partition{ nullptr };
  std::shared_ptr<sf::Texture> noIcon;

private:
  double elapsed{}; /*!< Used by draw function, delta time since last update frame */
  std::shared_ptr<std::vector<Battle::Card>> selectedCards; /*!< Current list of cards. */
  int curr{}; /*!< Card cursor index */
  unsigned multiplierValue{ 1 };
  mutable bool firstFrame{ true }; /*!< If true, this UI graphic is being drawn for the first time*/
  mutable SpriteProxyNode icon;
  mutable SpriteProxyNode frame; /*!< Sprite for the card icon and the black border */
};
