#pragma once
#include <Swoosh/Ease.h>

#include "bnScene.h"
#include "bnCamera.h"
#include "bnInputManager.h"
#include "bnAudioResourceManager.h"
#include "bnShaderResourceManager.h"
#include "bnTextureResourceManager.h"
#include "bnGame.h"
#include "bnAnimation.h"
#include "bnLanBackground.h"
#include "bnCardFolder.h"
#include "bnText.h"

/**
 * @class FolderEditScene
 * @author mav
 * @date 04/05/19
 * @brief Edit folder contents and select from card pool
 * @important the games, the card pool card count is not shared by folders
 *
 * User can select card and switch to the card pool on the right side of the scene to select cards
 * to swap out for.
 *
 * Before leaving the user is prompted to save changes
 */

class FolderEditScene : public Scene {
private:
  enum class ViewMode : int {
    folder,
    pool
  };

  // abstract interface class
  class ICardView {
  private:
    Battle::Card info;
  protected:
    void SetCard(const Battle::Card& other) { info = other; }
  public:
    ICardView(const Battle::Card& info) : info(info) {}
    virtual ~ICardView() {}

    virtual const bool IsEmpty() const = 0;
    virtual const bool GetCard(Battle::Card& copy) = 0;

    const Battle::Card& ViewCard() const { return info; }
  };

  /**
  * @class PackBucket
  * @brief Cards in a pool avoid listing duplicates by bundling them in a counted bucket
  *
  * Users can select up to all of the cards in a bucket. The bucket will remain in the list but at 0.
  */
  class PoolBucket : public ICardView {
  private:
    unsigned size{};
    unsigned maxSize{};

  public:
    PoolBucket(unsigned size, const Battle::Card& info) : ICardView(info), size(size), maxSize(size) {}
    ~PoolBucket() {}

    const bool IsEmpty() const override { return size == 0; }
    const bool GetCard(Battle::Card& copy) override { if (IsEmpty()) return false; else copy = Battle::Card(ViewCard()); size--;  return true; }
    void AddCard() { size++; size = std::min(size, maxSize); }
    const unsigned GetCount() const { return size; }
  };

  /**
  * @class FolderSlot
  * @brief A selectable row in the folder to place new cards. When removing cards, an empty slot is left behind
  */
  class FolderSlot : public ICardView {
  private:
    bool occupied{};
  public:
    FolderSlot() : ICardView(Battle::Card()) {}

    void AddCard(Battle::Card other) {
      SetCard(other);
      occupied = true;
    }

    const bool GetCard(Battle::Card& copy) override {
      if (!occupied) return false;

      copy = Battle::Card(ViewCard());
      occupied = false;

      SetCard(Battle::Card()); // null card

      return true;
    }

    const bool IsEmpty() const override {
      return !occupied;
    }
  };

private:
  std::vector<FolderSlot> folderCardSlots; /*!< Rows in the folder that can be inserted with cards or replaced */
  std::vector<PoolBucket> poolCardBuckets; /*!< Rows in the pack that represent how many of a card are left */
  bool hasFolderChanged{}; /*!< Flag if folder needs to be saved before quitting screen */
  bool isInSortMenu{}; /*!< Flag if in the sort menu */
  Camera camera;
  CardFolder& folder;

  // Menu name font
  Font font;
  Text menuLabel;

  // Selection input delays
  double maxSelectInputCooldown; // half of a second
  double selectInputCooldown;

  // Card UI font
  Font cardFont;
  Text cardLabel;

  Font numberFont;
  Text numberLabel;

  Text limitLabel;
  Text limitLabel2;

  // Card description font
  Font cardDescFont;
  Text cardDesc;

  // folder menu graphic
  sf::Sprite bg;
  sf::Sprite folderDock, packDock;
  sf::Sprite scrollbar;
  sf::Sprite cardHolder, packCardHolder;
  sf::Sprite element;
  sf::Sprite folderCursor, folderSwapCursor;
  sf::Sprite packCursor, packSwapCursor;
  sf::Sprite folderNextArrow;
  sf::Sprite packNextArrow;
  sf::Sprite folderCardCountBox;
  sf::Sprite folderSort, sortCursor;

  // Current card graphic data
  sf::Sprite card;
  sf::IntRect cardSubFrame;
  sf::Sprite cardIcon;
  swoosh::Timer cardRevealTimer;
  swoosh::Timer easeInTimer;

  std::shared_ptr<sf::Texture> noPreview, noIcon;

  struct CardView {
    int maxCardsOnScreen{ 0 };
    int currCardIndex{ 0 };
    int firstCardOnScreen{ 0 }; //!< index, the topmost card seen in the list
    int prevIndex{ -1 }; // for effect
    int numOfCards{ 0 };
    int swapCardIndex{ -1 }; // -1 for unselected, otherwise ID
  } folderView, packView;

  ViewMode currViewMode;
  ViewMode prevViewMode;

  double totalTimeElapsed;
  double frameElapsed;

  InputEvent lastKey{};
  bool extendedHold{ false }; //!< If held for a 2nd pass, scroll quickly
  bool canInteract;

  template<typename BaseType, size_t sz>
  class ISortOptions {
  protected:
    using filter = std::function<bool(const BaseType& first, const BaseType& second)>;
    using base_type_t = BaseType;
    std::array<filter, sz> filters;
    bool invert{};
    std::function<bool(const ICardView&)> pivotPred{ nullptr };
    size_t freeIdx{}, lastIndex{};
  public:
    virtual ~ISortOptions() {}

    size_t size() { return sz; }
    bool AddOption(const filter& filter) { if (freeIdx >= filters.size()) return false;  filters.at(freeIdx++) = filter; return true; }
    void SetPivotPredicate(const std::function<bool(const ICardView&)>& predicate) {
      pivotPred = predicate;
    }
    virtual void SelectOption(size_t index) = 0;
  };

  template<typename T, size_t sz>
  class SortOptions : public ISortOptions<ICardView, sz> {
    std::vector<T>& container;
  public:
    SortOptions(std::vector<T>& ref) : ISortOptions<ICardView, sz>(), container(ref) {};

    void SelectOption(size_t index) override {
      if (index >= sz) return;

      this->invert = !this->invert;
      if (index != this->lastIndex) {
        this->invert = false;
        this->lastIndex = index;
      }

      if (pivotPred) {
        auto pivot = std::partition(this->container.begin(), this->container.end(), pivotPred);
        size_t pivotDist = std::distance(this->container.begin(), pivot);

        std::vector<T> copy = std::vector<T>(this->container.begin(), pivot);

        std::sort(copy.begin(), copy.end(), this->filters.at(index));

        if (this->invert) {
          std::reverse(copy.begin(), copy.end());
        }

        std::vector<T> copy_end = std::vector<T>(this->container.begin() + pivotDist, this->container.end());

        this->container.clear();
        this->container.reserve(copy.size() + copy_end.size());
        this->container.insert(this->container.end(), copy.begin(), copy.end());
        this->container.insert(this->container.end(), copy_end.begin(), copy_end.end());

        return;
      }

      std::vector<T> copy = this->container;
      std::sort(copy.begin(), copy.end(), this->filters.at(index));

      if (this->invert) {
        std::reverse(copy.begin(), copy.end());
      }

      this->container = copy;
    }
  };

  size_t cursorSortIndex{};
  SortOptions<FolderSlot, 7u> folderSortOptions{ folderCardSlots };
  SortOptions<PoolBucket, 7u> poolSortOptions{ poolCardBuckets };

#ifdef __ANDROID__
  bool canSwipe;
  bool touchStart;

  int touchPosX;
  int touchPosStartX;

  bool releasedB;

  void StartupTouchControls();
  void ShutdownTouchControls();
#endif

  std::shared_ptr<sf::Texture> GetIconForCard(const std::string& uuid);
  std::shared_ptr<sf::Texture> GetPreviewForCard(const std::string& uuid);

  void DrawFolder(sf::RenderTarget& surface);
  void DrawPool(sf::RenderTarget& surface);
  void ComposeSortOptions();
  void ExcludeFolderDataFromPool();
  void PlaceFolderDataIntoCardSlots();
  void PlaceLibraryDataIntoBuckets();
  void WriteNewFolderData();

  template<typename ElementType>
  void RefreshCardDock(CardView& view, const std::vector<ElementType>& list);
  void RefreshCurrentCardSelection();

public:
  void onStart() override;
  void onUpdate(double elapsed) override;
  void onLeave() override;
  void onExit() override;
  void onEnter() override;
  void onResume() override;
  void onDraw(sf::RenderTexture& surface) override;
  void onEnd() override;

  FolderEditScene(swoosh::ActivityController&, CardFolder& folder);
  ~FolderEditScene();
};

template<typename T>
void FolderEditScene::RefreshCardDock(FolderEditScene::CardView& view, const std::vector<T>& list)
{
  if (view.currCardIndex < list.size()) {
    T slot = list[view.currCardIndex]; // copy data, do not mutate it

    sf::Sprite& sprite = currViewMode == ViewMode::folder ? cardHolder : packCardHolder;
    Battle::Card card;
    slot.GetCard(card); // Returns and frees the card from the bucket, this is why we needed a copy

    switch (card.GetClass()) {
    case Battle::CardClass::mega:
      sprite.setTexture(*Textures().LoadFromFile(TexturePaths::FOLDER_CHIP_HOLDER_MEGA));
      break;
    case Battle::CardClass::giga:
      sprite.setTexture(*Textures().LoadFromFile(TexturePaths::FOLDER_CHIP_HOLDER_GIGA));
      break;
    case Battle::CardClass::dark:
      sprite.setTexture(*Textures().LoadFromFile(TexturePaths::FOLDER_CHIP_HOLDER_DARK));
      break;
    default:
      sprite.setTexture(*Textures().LoadFromFile(TexturePaths::FOLDER_CHIP_HOLDER));
    }
  }
}