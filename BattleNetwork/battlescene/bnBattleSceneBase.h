#pragma once

#include <memory>
#include <type_traits>
#include <vector>
#include <map>
#include <SFML/Graphics/RenderTexture.hpp>
#include <Swoosh/ActivityController.h>
#include <Swoosh/Activity.h>
#include <Swoosh/Timer.h>

#include "../bnScene.h"
#include "../bnComponent.h"
#include "../bnPA.h"
#include "../bnMobHealthUI.h"
#include "../bnAnimation.h"
#include "../bnCamera.h"
#include "../bnCounterCombatRule.h"
#include "../bnCounterHitListener.h"
#include "../bnHitListener.h"
#include "../bnCharacterDeleteListener.h"
#include "../bnCardUseListener.h"
#include "../bnRealtimeCardUseListener.h"
#include "../bnPlayerSelectedCardsUI.h"
#include "../bnCardSelectionCust.h"
#include "../bnPlayerEmotionUI.h"
#include "../bnBattleResults.h"

// Battle scene specific classes
#include "bnBattleSceneState.h"
#include "States/bnFadeOutBattleState.h"

// forward declare statements
class Field; 
class Player;
class Mob;
class Player;
class CardFolder;
class PlayerHealthUI;
class CounterCombatRule;
class Background;

// using namespaces
using sf::RenderWindow;
using sf::VideoMode;
using sf::Clock;
using sf::Event;

using BattleResultsFunc = std::function<void(const BattleResults& results)>;

struct BattleSceneBaseProps {
  Player& player;
  PA& programAdvance;
  CardFolder* folder{ nullptr };
  Field* field{ nullptr };
  std::shared_ptr<Background> background{ nullptr };
};

/**
  @brief BattleSceneBase class provides an API for creating complex states
*/
class BattleSceneBase : 
  public Scene, 
  public HitListener,
  public CounterHitListener, 
  public CharacterDeleteListener, 
  public CardUseListener {
private:
  // general stuff
  bool quitting{ false }; //!< Determine if we are leaving the battle scene
  bool didDoubleDelete{ false }; /*!< Flag if player double deleted this frame */
  bool didTripleDelete{ false }; /*!< Flag if player tripled deleted this frame */
  bool didCounterHit{ false }; /*!< Flag if player countered an enemy this frame */
  bool isSceneInFocus{ false }; //<! Let us know if transition effects complete
  bool isPlayerDeleted{ false };
  bool highlightTiles{ true };
  bool backdropAffectBG{ false };
  int round{ 0 }; //!< Some scene types repeat battles and need to track rounds
  int totalCounterMoves{ 0 }; /*!< Track player's counters. Used for ranking. */
  int totalCounterDeletions{ 0 }; /*!< Track player's counter-deletions. Used for ranking. */
  int comboDeleteCounter{ 0 }; /*!< Deletions within 12 frames triggers double or triple deletes. */
  int randBG; /*!< If background provided by Mob data is nullptr, randomly select one */
  int lastMobSize{ 0 };
  int newMobSize{ 0 };
  double elapsed{ 0 }; /*!< total time elapsed in battle */
  double customProgress{ 0 }; /*!< Cust bar progress in seconds */
  double customDuration{ 10.0 }; /*!< Cust bar max time in seconds */
  double backdropOpacity{ 1.0 };
  double backdropFadeIncrements{ 125 }; /*!< x/255 per tick */
  double backdropMaxOpacity{ 1.0 };
  RealtimeCardUseListener cardListener; /*!< Card use listener handles one card at a time */
  PlayerSelectedCardsUI* cardUI{ nullptr }; /*!< Player's Card UI implementation */
  PlayerEmotionUI* emotionUI{ nullptr }; /*!< Player's Emotion Window */
  Camera camera; /*!< Camera object - will shake screen */
  sf::Sprite mobEdgeSprite, mobBackdropSprite; /*!< name backdrop images*/
  PA& programAdvance; /*!< PA object loads PA database and returns matching PA card from input */
  Field* field{ nullptr }; /*!< Supplied by mob info: the grid to battle on */
  Player* player{ nullptr }; /*!< Pointer to player's selected character */
  Mob* mob{ nullptr }; /*!< Mob and mob data player are fighting against */
  std::shared_ptr<Background> background{ nullptr }; /*!< Custom backgrounds provided by Mob data */
  std::shared_ptr<sf::Texture> customBarTexture; /*!< Cust gauge image */
  Font mobFont; /*!< Name of mob font */
  std::vector<std::string> mobNames; /*!< List of every non-deleted mob spawned */
  std::vector<SceneNode*> scenenodes; /*!< ui components. DO NOT DELETE. */
  std::vector<Component*> components; /*!< Components injected into the scene to track. DO NOT DELETE. */
  std::vector<std::reference_wrapper<CardUsePublisher>> cardUseSubscriptions; /*!< Share subscriptions with other CardListeners states*/
  BattleResults battleResults{};
  BattleResultsFunc onEndCallback;

  // counter stuff
  SpriteProxyNode counterReveal;
  Animation counterRevealAnim;
  CounterCombatRule* counterCombatRule{ nullptr };

  // card stuff
  CardSelectionCust cardCustGUI; /*!< Card selection GUI that has an API to interact with */

  // sprites
  swoosh::Timer comboInfoTimer; /*!< How long the info should stay on screen */
  swoosh::Timer multiDeleteTimer; /*!< Deletions start a 12 frame timer to count towards combos */
  swoosh::Timer battleTimer; /*!< Total duration of active battle time */

  // shader fx
  double shaderCooldown;
  sf::Shader& whiteShader; /*!< Fade out white */
  sf::Shader& yellowShader; /*!< Turn tiles yellow */
  sf::Shader& heatShader; /*!< Heat waves and red hue */
  sf::Shader& iceShader; /*!< Reflection in the ice */
  sf::Shader& backdropShader;
  sf::Vector2u textureSize; /*!< Size of distorton effect */

  // backdrop status enum
  enum class backdrop : int {
    fadeout = 0,
    fadein
  } backdropMode{};

  // event bus
  EventBus::Channel channel;

protected:
  using ChangeCondition = BattleSceneState::ChangeCondition;

  /*
    \brief StateNode represents a node in a graph of conditional states

    States can flow from one to another that it is linked to.
    We call two linked nodes an Edge.
    To transition from one node to the other, the linked condition must be met (true).
    We can link battle scene states together in the inherited BattleSceneBase class.
  */
  class StateNode {
    friend class BattleSceneBase;

    BattleSceneState& state; //!< The battle scene state this node represents
    BattleSceneBase& owner; //!< The scene this state refers to
  public:
    StateNode(BattleSceneBase& owner, BattleSceneState& state) : state(state), owner(owner)
    {
      state.scene = &owner;
      state.controller = &owner.getController();
    }

    StateNode(const StateNode& rhs) : state(rhs.state), owner(rhs.owner) {

    }
  };

  /*
    \brief This wrapper is just a StateNode with a few baked-in API functions to create easy conditional state transitions 
  */
  template<typename T>
  class StateNodeWrapper : public StateNode {
    T& state;

    public:
    using Class = T;

    StateNodeWrapper(BattleSceneBase& owner, T& state) : 
      state(state), StateNode(owner, state)
    {}

    StateNodeWrapper(const StateNodeWrapper& copy) :
      state(copy.state), StateNode(copy.owner, copy.state)
    {}


    /* 
        \brief Return the underlining state object as a pointer
    */
    T* operator->() {
      return &state;
    }

    /*
    \brief Return the underlining state object as a const-qualified pointer
    */
    const T* operator->() const {
      return &state;
    }

    /*
      \brief Alternative return the underlining state object as a reference
    */
    T& Unwrap() {
      return state;
    }

    /*
        \brief if input functor is a member function, then create a closure to call the function on a class object 
    */
    template<
      typename MemberFunc,
      typename = typename std::enable_if<std::is_member_function_pointer<MemberFunc>::value>::type
    >
    StateNodeWrapper& ChangeOnEvent(StateNode& next, MemberFunc when) {
      T* statePtr = &state;
      owner.Link(*this, next, 
        [statePtr, when]{
            return (statePtr->*when)();
        });
      return *this;
    }

    /* 
        \brief if input is a lambda, use the ::Link() API function already provided
    */
    template<
      typename Lambda,
      typename = typename std::enable_if<!std::is_member_function_pointer<Lambda>::value>::type
    >
    StateNodeWrapper& ChangeOnEvent(StateNode& next, Lambda&& when) {
      owner.Link(*this, next, when);
      return *this;
    }
  };

  void LoadMob(Mob& mob);

  /**
  * @brief Scans the entity list for updated components and tries to Inject them if the components require.
  */
  void ProcessNewestComponents();

  void OnCardUse(const Battle::Card& card, Character& user, long long timestamp) override final;
  void OnCounter(Character& victim, Character& aggressor) override final;
  void OnDeleteEvent(Character& pending) override final;

#ifdef __ANDROID__
  void SetupTouchControls();
  void ShutdownTouchControls();

  bool releasedB;
#endif

public:
  friend class CounterCombatRule;

  BattleSceneBase() = delete;
  BattleSceneBase(const BattleSceneBase&) = delete;
  BattleSceneBase(swoosh::ActivityController& controller, const BattleSceneBaseProps& props, BattleResultsFunc onEnd = nullptr);
  virtual ~BattleSceneBase();

  const bool DoubleDelete() const;
  const bool TripleDelete() const;
  const int ComboDeleteSize() const;
  const bool Countered() const;
  void HandleCounterLoss(Character& subject, bool playsound);
  void HighlightTiles(bool enable);

  const double GetCustomBarProgress() const;
  const double GetCustomBarDuration() const;
  void SetCustomBarProgress(double percentage);
  void SetCustomBarDuration(double maxTimeSeconds);

  void SubscribeToCardEvents(CardUsePublisher& publisher);
  void UnsubscribeFromCardEvents(CardUsePublisher& publisher);
  const std::vector<std::reference_wrapper<CardUsePublisher>>& GetCardUseSubscriptions() const;

  /**
    * @brief State boolean for BattleScene. Query if the battle is over.
    * @return true if all mob enemies are marked as deleted
    */
  const bool IsCleared();

  /**
    * @brief Get the total number of counter moves
    * @return const int
    */
  const int GetCounterCount() const;

  /**
    * @brief Query if scene is in focus (segue over)
    */
  const bool IsSceneInFocus() const;

  /**
  * Returns true if the battle scene is quitting (leaving) to the previous scene
  */
  const bool IsQuitting() const;

  /*
      \brief Use class type T as the state and perfect-forward arguments to the class 
      \return StateNodeWrapper<T> structure for easy programming
  */
  template<typename T, typename... Args>
  StateNodeWrapper<T> AddState(Args&&... args) {
    T* ptr = new T(std::forward<decltype(args)>(args)...);
    states.insert(states.begin(), ptr);
    return StateNodeWrapper<T>(*this, *ptr);
  }

  /*  
      \brief Set the current state pointer to this state node reference and begin the scene
  */
  void StartStateGraph(StateNode& start);

  /*
      \brief Update the scene and current state. If any conditions are satisfied, transition to the linked state
  */
  virtual void onStart() override;
  virtual void onLeave() override;
  virtual void onUpdate(double elapsed) override;
  virtual void onDraw(sf::RenderTexture& surface) override;
  virtual void onEnd() override;

  bool IsPlayerDeleted() const;

  Player* GetPlayer();
  Field* GetField();
  const Field* GetField() const;
  CardSelectionCust& GetCardSelectWidget();
  PlayerSelectedCardsUI& GetSelectedCardsUI();
  PlayerEmotionUI& GetEmotionWindow();
  Camera& GetCamera();
  PA& GetPA();
  BattleResults& BattleResultsObj();
  void StartBattleStepTimer();
  void StopBattleStepTimer();
  void BroadcastBattleStart();
  void BroadcastBattleStop();

  const sf::Time GetElapsedBattleTime();

  const bool FadeInBackdrop(double amount, double to, bool affectBackground);
  const bool FadeOutBackdrop(double amount);

  std::vector<std::reference_wrapper<const Character>> MobList();

  /**
    @brief Crude support card filter step
  */
  void FilterSupportCards(Battle::Card** cards, int& cardCount);

  /*
      \brief Forces the creation a fadeout state onto the state pointer and goes back to the last scene
  */
  void Quit(const FadeOut& mode);

  /**
    * @brief Inject uses double-visitor design pattern. BattleScene adds component to draw and update list.
    * @param other
    */
  void Inject(MobHealthUI& other);

  /**
  * @brief Inject uses double-visitor design pattern. BattleScene subscribes to the selected card ui.
  * @param cardUI
  */
  void Inject(SelectedCardsUI& cardUI);

  /**
    * @brief Inject uses double-visitor design pattern. This is default case.
    * @param other Adds component "other" to component update list.
    */
  void Inject(Component* other);

  /**
    * @brief When ejecting component from scene, removes it from update list
    * @param other
    */
  void Eject(Component::ID_t ID);

private:
  BattleSceneState* current{nullptr}; //!< Pointer to the current battle scene state
  const BattleSceneState* next{ nullptr };
  const BattleSceneState* last{ nullptr };
  std::vector<BattleSceneState*> states; //!< List of all battle scene states

  /*
      \brief Edge represents a link from from state A to state B when a condition is met
  */
  struct Edge {
    BattleSceneState* a{ nullptr };
    BattleSceneState* b{ nullptr };
    ChangeCondition when; //!< functor that returns boolean

    Edge(BattleSceneState* a, BattleSceneState* b, ChangeCondition when) : a(a), b(b), when(when)
    {}

    // `when` uses std::function<> which is not trivially copyable
    Edge(const Edge& cpy) : a(cpy.a), b(cpy.b), when(cpy.when) 
    {}
  };

  std::multimap<BattleSceneState*, Edge*> nodeToEdges; //!< All edges. Together, they form a graph

  /*
      \brief Creates an edge with a condition and packs it away into the scene graph
  */
  void Link(StateNode& a, StateNode& b, ChangeCondition when);
};
