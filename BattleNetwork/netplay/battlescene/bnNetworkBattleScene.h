#pragma once

#include <Swoosh/Activity.h>
#include <Swoosh/Ease.h>
#include <Swoosh/Timer.h>
#include <time.h>
#include <typeinfo>
#include <SFML/Graphics.hpp>
#include <Poco/Net/DatagramSocket.h>
#include <Poco/Buffer.h>

#include "../../bnMobHealthUI.h"
#include "../../bnCounterHitListener.h"
#include "../../bnCharacterDeleteListener.h"
#include "../../bnBackground.h"
#include "../../bnLanBackground.h"
#include "../../bnGraveyardBackground.h"
#include "../../bnVirusBackground.h"
#include "../../bnCamera.h"
#include "../../bnInputManager.h"
#include "../../bnCardSelectionCust.h"
#include "../../bnCardFolder.h"
#include "../../bnShaderResourceManager.h"
#include "../../bnPA.h"
#include "../../bnDrawWindow.h"
#include "../../bnSceneNode.h"
#include "../../bnBattleResults.h"
#include "../../battlescene/bnBattleSceneBase.h"
#include "../../bnMob.h"
#include "../../bnField.h"
#include "../../bnPlayer.h"
#include "../../bnSelectedCardsUI.h"
#include "../../bnCounterHitListener.h"
#include "../../bnCharacterDeleteListener.h"
#include "../../bnNaviRegistration.h"
#include "../../battlescene/States/bnCharacterTransformBattleState.h"

#include "../bnNetPlayFlags.h"
#include "../bnNetPlayConfig.h"
#include "../bnNetPlaySignals.h"
#include "../bnNetPlayPacketProcessor.h"

using sf::RenderWindow;
using sf::VideoMode;
using sf::Clock;
using sf::Event;

struct CombatBattleState;
struct TimeFreezeBattleState;
struct NetworkSyncBattleState;
struct CardComboBattleState;
struct BattleStartBattleState;
class CardSelectBattleState;

class Mob;
class Player;
class PlayerHealthUI;
class NetworkCardUseListener; 

struct NetworkBattleSceneProps {
  BattleSceneBaseProps base;
  sf::Sprite mug; // speaker mugshot
  Animation anim; // mugshot animation
  std::shared_ptr<sf::Texture> emotion; // emotion atlas image
  NetPlayConfig& netconfig;
  std::shared_ptr<Netplay::PacketProcessor> packetProcessor;
};

class NetworkBattleScene final : public BattleSceneBase {
private:
  friend struct NetworkSyncBattleState;
  friend class NetworkCardUseListener;
  friend class PlayerInputReplicator;
  
  bool waitingForCardSelectScreen{}; //!< Flag to count down before changing states
  frame_time_t roundStartDelay{}; //!< How long to wait on opponent's animations before starting the next round
  frame_time_t cardStateDelay{}; //!< Timer that counts down until the card select state opens up
  frame_time_t packetTime{}; //!< When a packet was sent. Compare the time sent vs the recent ACK for accurate connectivity
  Text ping;
  SelectedNavi selectedNavi; //!< the type of navi we selected
  NetPlayFlags remoteState; //!< remote state flags to ensure stability
  std::vector<Player*> players; //!< Track all players
  std::vector<std::shared_ptr<TrackedFormData>> trackedForms;
  SpriteProxyNode pingIndicator;
  SelectedCardsUI* remoteCardUsePublisher{ nullptr };
  Battle::Card** remoteHand{ nullptr }; // TODO: THIS IS HACKING TO GET IT TO WORK PLS REMOVE LATER
  Player *remotePlayer{ nullptr }; //!< their player pawn
  Mob* mob{ nullptr }; //!< Our managed mob structure for PVP
  CombatBattleState* combatPtr{ nullptr };
  TimeFreezeBattleState* timeFreezePtr{ nullptr };
  NetworkSyncBattleState* syncStatePtr{ nullptr };
  CardComboBattleState* cardComboStatePtr{ nullptr };
  CardSelectBattleState* cardStatePtr{ nullptr };
  BattleStartBattleState* startStatePtr{ nullptr };
  std::shared_ptr<Netplay::PacketProcessor> packetProcessor;

  void sendHandshakeSignal(); // send player data to start the next round
  void sendInputEvents(const std::vector<InputEvent>& events); // send our key or gamepad events
  void sendConnectSignal(const SelectedNavi navi);
  void sendChangedFormSignal(const int form);
  void sendHPSignal(const int hp);
  void sendTileCoordSignal(const int x, const int y);
  void sendRequestedCardSelectSignal(); 
  void sendLoserSignal(); // if we die, let them know

  void recieveHandshakeSignal(const Poco::Buffer<char>& buffer);
  void recieveInputEvent(const Poco::Buffer<char>& buffer); 
  void recieveConnectSignal(const Poco::Buffer<char>&);
  void recieveChangedFormSignal(const Poco::Buffer<char>&);
  void recieveHPSignal(const Poco::Buffer<char>&);
  void recieveTileCoordSignal(const Poco::Buffer<char>&);
  void recieveLoserSignal(); // if they die, update our state flags
  void recieveRequestedCardSelectSignal(); // if the remote opens card select, we should be too
  void processPacketBody(NetPlaySignals header, const Poco::Buffer<char>&);
  void UpdatePingIndicator(frame_time_t frames);
  void ResetPacketProcessor();
public:
  using BattleSceneBase::ProcessNewestComponents;

  void OnHit(Character& victim, const Hit::Properties& props) override final;
  void onUpdate(double elapsed) override final;
  void onDraw(sf::RenderTexture& surface) override final;
  void onExit() override;
  void onEnter() override;
  void onStart() override;
  void onResume() override;
  void onEnd() override;

  void Inject(PlayerInputReplicator& pub);

  const NetPlayFlags& GetRemoteStateFlags();
  const double GetAvgLatency() const;

  /**
   * @brief Construct scene with selected player, generated mob data, and the folder to use
   */
  NetworkBattleScene(swoosh::ActivityController&, const NetworkBattleSceneProps& props, BattleResultsFunc onEnd=nullptr);
  
  /**
   * @brief Clears all nodes and components
   */
  ~NetworkBattleScene();

};