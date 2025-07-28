#pragma once

#include <memory>
#include <type_traits>
#include <vector>
#include <map>
#include <SFML/Graphics/RenderTexture.hpp>
#include <Swoosh/ActivityController.h>
#include <Swoosh/Activity.h>
#include <Swoosh/Timer.h>

#include "../bnEntity.h"
#include "../bnScene.h"
#include "../bnComponent.h"
#include "../bnPA.h"
#include "../bnMobHealthUI.h"
#include "../bnAnimation.h"
#include "../bnCamera.h"
#include "../bnCounterCombatRule.h"
#include "../bnCounterHitListener.h"
#include "../bnHitListener.h"
#include "../bnCharacterSpawnListener.h"
#include "../bnCharacterDeleteListener.h"
#include "../bnCardUseListener.h"
#include "../bnRealtimeCardUseListener.h"
#include "../bnPlayerSelectedCardsUI.h"
#include "../bnCardSelectionCust.h"
#include "../bnPlayerEmotionUI.h"
#include "../bnBattleResults.h"
#include "../bnEventBus.h"

// Battle scene specific classes
#include "bnBattleSceneState.h"
#include "States/bnFadeOutBattleState.h"

// forward declare statements
class Field; 
class Player;
class Mob;
class Player;
class CardFolder;
class PlayerHealthUIComponent;
class CounterCombatRule;
class Background;

// using namespaces
using sf::RenderWindow;
using sf::VideoMode;
using sf::Clock;
using sf::Event;

// alias
using BattleResultsFunc = std::function<void(const BattleResults& results)>;

/**
  @brief Tracks form data so the card select knows when or when not to animate the player
*/
struct TrackedFormData {
  int selectedForm{ -1 };
  bool animationComplete{ true };
};

struct BattleSceneBaseProps {
  std::shared_ptr<Player> player;
  PA& programAdvance;
  std::unique_ptr<CardFolder> folder{ nullptr };
  std::shared_ptr<Field> field{ nullptr };
  std::shared_ptr<Background> background{ nullptr };
};

/**
  @brief BattleSceneBase class provides an API for creating complex states
*/
class BattleSceneBase : 
  public Scene, 
  public HitListener,
  public CounterHitListener, 
  public CharacterSpawnListener,
  public CharacterDeleteListener, 
  public CardActionUseListener {
private:
  // general stuff
  bool skipFrame{ false };
  bool quitting{ false }; //!< Determine if we are leaving the battle scene
  bool didDoubleDelete{ false }; /*!< Flag if player double deleted this frame */
  bool didTripleDelete{ false }; /*!< Flag if player tripled deleted this frame */
  bool didCounterHit{ false }; /*!< Flag if player countered an enemy this frame */
  bool isSceneInFocus{ false }; //<! Let us know if transition effects complete
  bool isPlayerDeleted{ false };
  bool highlightTiles{ true };
  bool backdropAffectBG{ false };
  bool perspectiveFlip{ false }; //!< if true, view from blue team's perspective
  bool hasPlayerSpawned{ false };
  int round{ 0 }; //!< Some scene types repeat battles and need to track rounds
  int turn{ 0 }; //!< How many turns per round (inbetween card selection)
  int totalCounterMoves{ 0 }; /*!< Track player's counters. Used for ranking. */
  int totalCounterDeletions{ 0 }; /*!< Track player's counter-deletions. Used for ranking. */
  int comboDeleteCounter{ 0 }; /*!< Deletions within 12 frames triggers double or triple deletes. */
  int randBG; /*!< If background provided by Mob data is nullptr, randomly select one */
  int lastRedTeamMobSize{ 0 }, lastBlueTeamMobSize{ 0 };
  int newRedTeamMobSize{ 0 }, newBlueTeamMobSize{ 0 };
  frame_time_t frameNumber{ 0 };
  double elapsed{ 0 }; /*!< total time elapsed in battle */
  double customProgress{ 0 }; /*!< Cust bar progress in seconds */
  double customDuration{ 10.0 }; /*!< Cust bar max time in seconds */
  double customFullAnimDelta{ 0 }; /*!< For animating a complete cust bar*/
  double backdropOpacity{ 1.0 };
  double backdropFadeIncrements{ 125 }; /*!< x/255 per tick */
  double backdropMaxOpacity{ 1.0 };
  RealtimeCardActionUseListener cardActionListener; /*!< Card use listener handles one card at a time */
  std::shared_ptr<PlayerSelectedCardsUI> cardUI{ nullptr }; /*!< Player's Card UI implementation */
  std::shared_ptr<PlayerEmotionUI> emotionUI{ nullptr }; /*!< Player's Emotion Window */
  Camera camera; /*!< Camera object - will shake screen */
  sf::Sprite mobEdgeSprite, mobBackdropSprite; /*!< name backdrop images*/
  PA& programAdvance; /*!< PA object loads PA database and returns matching PA card from input */
  std::shared_ptr<Field> field{ nullptr }; /*!< Supplied by mob info: the grid to battle on */
  std::shared_ptr<Player> localPlayer; /*!< Local player */
  std::vector<Entity::ID_t> deletingRedMobs, deletingBlueMobs; /*!< mobs untrack enemies but we need to know when they fully finish deleting*/
  std::vector<std::shared_ptr<Player>> otherPlayers; /*!< Player array supports multiplayer */
  std::map<Player*, TrackedFormData> allPlayerFormsHash;
  std::map<Player*, Team> allPlayerTeamHash; /*!< Check previous frames teams for traitors */
  Mob* redTeamMob{ nullptr }; /*!< Mob and mob data opposing team are fighting against */
  Mob* blueTeamMob{ nullptr }; /*!< Mob and mob data player are fighting against */
  std::shared_ptr<Background> background{ nullptr }; /*!< Custom backgrounds provided by Mob data */
  std::shared_ptr<sf::Texture> customBarTexture; /*!< Cust gauge image */
  Font mobFont; /*!< Name of mob font */
  std::vector<InputEvent> queuedLocalEvents; /*!< Local player input events*/
  std::vector<std::string> mobNames; /*!< List of every non-deleted mob spawned */
  std::vector<std::shared_ptr<SceneNode>> scenenodes; /*!< ui components. */
  std::vector<std::shared_ptr<Component>> components; /*!< Components injected into the scene to track. */
  std::vector<std::reference_wrapper<CardActionUsePublisher>> cardUseSubscriptions; /*!< Share subscriptions with other CardListeners states*/
  BattleResults battleResults{};
  BattleResultsFunc onEndCallback;

  // cust gauge 
  bool isGaugeFull{ false };
  SpriteProxyNode customBar;
  sf::Shader* customBarShader; /*!< Cust gauge shaders */

  // counter stuff
  std::shared_ptr<SpriteProxyNode> counterReveal;
  Animation counterRevealAnim;
  std::shared_ptr<CounterCombatRule> counterCombatRule{ nullptr };

  // card stuff
  CardSelectionCust cardCustGUI; /*!< Card selection GUI that has an API to interact with */

  // sprites
  frame_time_stopwatch_t comboInfoTimer; /*!< How long the info should stay on screen */
  frame_time_stopwatch_t multiDeleteTimer; /*!< Deletions start a 12 frame timer to count towards combos */
  frame_time_stopwatch_t battleTimer; /*!< Total duration of active battle time */

  // shader fx
  double shaderCooldown;
  sf::Shader* whiteShader; /*!< Fade out white */
  sf::Shader* yellowShader; /*!< Turn tiles yellow */
  sf::Shader* heatShader; /*!< Heat waves and red hue */
  sf::Shader* iceShader; /*!< Reflection in the ice */
  sf::Shader* backdropShader;
  sf::Vector2u textureSize; /*!< Size of distorton effect */

  // backdrop status enum
  enum class backdrop : int {
    fadeout = 0,
    fadein
  } backdropMode{};

  // event bus
  EventBus::Channel channel;

  sf::Vector2f PerspectiveOffset(const sf::Vector2f& pos);
  sf::Vector2f PerspectiveOrigin(const sf::Vector2f& origin, const sf::FloatRect& size);
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

  virtual void Init() = 0;
  void SpawnLocalPlayer(int x, int y);
  void SpawnOtherPlayer(std::shared_ptr<Player> player, int x, int y);

  void LoadBlueTeamMob(Mob& mob);
  void LoadRedTeamMob(Mob& mob);

  /**
  * @brief Scans the entity list for updated components and tries to Inject them if the components require.
  */
  void ProcessNewestComponents();
  void FlushLocalPlayerInputQueue();
  std::vector<InputEvent> ProcessLocalPlayerInputQueue(unsigned int lag = 0, bool gatherInput = true);
  void OnCardActionUsed(std::shared_ptr<CardAction> action, uint64_t timestamp) override final;
  void OnCounter(Entity& victim, Entity& aggressor) override final;
  void OnSpawnEvent(std::shared_ptr<Character>& spawned) override final;
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
  BattleSceneBase(swoosh::ActivityController& controller, BattleSceneBaseProps& props, BattleResultsFunc onEnd = nullptr);
  virtual ~BattleSceneBase();

  const bool IsCustGaugeFull() const;
  const bool DoubleDelete() const;
  const bool TripleDelete() const;
  const int ComboDeleteSize() const;
  const bool Countered() const;
  void HandleCounterLoss(Entity& subject, bool playsound);
  void HighlightTiles(bool enable);

  const double GetCustomBarProgress() const;
  const double GetCustomBarDuration() const;
  void SetCustomBarProgress(double value);
  void SetCustomBarDuration(double maxTimeSeconds);

  void DrawCustGauage(sf::RenderTexture& surface);
  void SubscribeToCardActions(CardActionUsePublisher& publisher);
  const std::vector<std::reference_wrapper<CardActionUsePublisher>>& GetCardActionSubscriptions() const;
  TrackedFormData& GetPlayerFormData(const std::shared_ptr<Player>& player);
  Team& GetPlayerTeamData(const std::shared_ptr<Player>& player);
  std::shared_ptr<Player> GetPlayerFromEntityID(Entity::ID_t ID);

  /**
    * @brief State boolean for BattleScene. Query if the battle is over.
    * @return true if all mob enemies are marked as deleted and removed from field.
    */
  const bool IsRedTeamCleared() const;

  /**
  * @brief State boolean for BattleScene. Query if the battle is over.
  * @return true if all mob enemies are marked as deleted and removed from field.
  */
  const bool IsBlueTeamCleared() const;

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

  // Define what happens on scenes that need to inspect pre-filtered card selections
  virtual void OnSelectNewCards(const std::shared_ptr<Player>& player, std::vector<Battle::Card>& cards) {};

  void DrawWithPerspective(sf::Sprite& sprite, sf::RenderTarget& surf);
  void DrawWithPerspective(sf::Shape& shape, sf::RenderTarget& surf);
  void DrawWithPerspective(Text& text, sf::RenderTarget& surf);
  void PerspectiveFlip(bool flipped);
  bool TrackOtherPlayer(std::shared_ptr<Player>& other);
  void UntrackOtherPlayer(std::shared_ptr<Player>& other);
  void UntrackMobCharacter(std::shared_ptr<Character>& character);
  bool IsPlayerDeleted() const;

  std::shared_ptr<Player> GetLocalPlayer();
  const std::shared_ptr<Player> GetLocalPlayer() const;
  std::vector<std::shared_ptr<Player>> GetOtherPlayers();
  std::vector<std::shared_ptr<Player>> GetAllPlayers();
  std::shared_ptr<Field> GetField();
  const std::shared_ptr<Field> GetField() const;
  CardSelectionCust& GetCardSelectWidget();
  PlayerSelectedCardsUI& GetSelectedCardsUI();
  PlayerEmotionUI& GetEmotionWindow();
  Camera& GetCamera();
  PA& GetPA();
  BattleResults& BattleResultsObj();
  const BattleSceneState* GetCurrentState() const;
  const int GetTurnCount();
  const int GetRoundCount();
  const frame_time_t FrameNumber() const;
  void StartBattleStepTimer();
  void StopBattleStepTimer();
  void BroadcastBattleStart();
  void BroadcastBattleStop();
  virtual void IncrementTurnCount();
  virtual void IncrementRoundCount();
  void SkipFrame();
  void IncrementFrame();
  const frame_time_t GetElapsedBattleFrames();

  const bool FadeInBackdrop(double amount, double to, bool affectBackground);
  const bool FadeOutBackdrop(double amount);

  std::vector<std::reference_wrapper<const Character>> RedTeamMobList();
  std::vector<std::reference_wrapper<const Character>> BlueTeamMobList();

  /**
    @brief Crude support card filter step
  */
  void FilterSupportCards(const std::shared_ptr<Player>& player, std::vector<Battle::Card>& cards);

  /*
      \brief Forces the creation a fadeout state onto the state pointer and goes back to the last scene
  */
  void Quit(const FadeOut& mode);

  /**
    * @brief Inject uses double-visitor design pattern. BattleScene adds component to draw and update list.
    * @param other
    */
  void Inject(std::shared_ptr<MobHealthUI> other);

  /**
  * @brief Inject uses double-visitor design pattern. BattleScene subscribes to the selected card ui.
  * @param cardUI
  */
  void Inject(std::shared_ptr<SelectedCardsUI> cardUI);

  /**
    * @brief Inject uses double-visitor design pattern. This is default case.
    * @param other Adds component "other" to component update list.
    */
  void Inject(std::shared_ptr<Component> other);

  /**
    * @brief When ejecting component from scene, removes it from update list
    * @param other
    */
  void Eject(Component::ID_t ID);

  /**
  * @brief Dynamically track new character on the field
  * @param newCharacter
  */
  void TrackCharacter(std::shared_ptr<Character> newCharacter);

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
