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
#include "../../bnPlayerPackageManager.h"
#include "../../battlescene/States/bnCharacterTransformBattleState.h"

#include "../bnNetPlayFlags.h"
#include "../bnNetPlayConfig.h"
#include "../bnNetPlaySignals.h"
#include "../bnNetPlayPacketProcessor.h"

using sf::RenderWindow;
using sf::VideoMode;
using sf::Clock;
using sf::Event;

// state forward decl.
struct CombatBattleState;
struct TimeFreezeBattleState;
struct NetworkSyncBattleState;
struct CardComboBattleState;
struct BattleStartBattleState;
struct BattleOverBattleState;
class CardSelectBattleState;

// battle classes forward decl.
class Mob;
class Player;
class PlayerHealthUI;
class NetworkCardUseListener; 

struct NetworkPlayerSpawnData {
  std::vector<PackageAddress> blocks;
  std::shared_ptr<Player> player;
  int x{}, y{}; //!< grid pos
};

struct NetworkBattleSceneProps {
  BattleSceneBaseProps base;
  sf::Sprite mug; // speaker mugshot
  Animation anim; // mugshot animation
  std::shared_ptr<sf::Texture> emotion; // emotion atlas image
  std::shared_ptr<Netplay::PacketProcessor> packetProcessor;
  std::vector<NetworkPlayerSpawnData> spawnOrder;  // The order to spawn players in
};

struct FrameInputData {
  unsigned int frameNumber{};
  std::vector<InputEvent> events;
};

static bool operator<(const FrameInputData& lhs, const FrameInputData& rhs) {
  return lhs.frameNumber < rhs.frameNumber;
}

class NetworkBattleScene final : public BattleSceneBase {
private:
  friend class NetworkCardUseListener;
  friend class PlayerInputReplicator;
  
  NetworkBattleSceneProps props;

  bool skipFrame{ false }; //!< true if the scene is skipping frames - use this boolean to ensure gamestates are in sync before moving between them
  bool localPlayerDecross{ false }, remotePlayerDecross{ false };
  bool ignoreLockStep{}; //!< Used when battles are over to allow both clients to continue streaming the game ending
  frame_time_t roundStartDelay{}; //!< How long to wait on opponent's animations before starting the next round
  frame_time_t remoteFrameNumber{}, maxRemoteFrameNumber{}, lastSentFrameNumber{};
  Text ping, frameNumText;
  NetPlayFlags remoteState; //!< remote state flags to ensure stability
  SpriteProxyNode pingIndicator;
  std::shared_ptr<SelectedCardsUI> remoteCardActionUsePublisher{ nullptr };
  std::vector<Battle::Card> remoteHand;
  std::vector<FrameInputData> remoteInputQueue;
  std::vector<std::string> prefilteredCardSelection;
  std::shared_ptr<Player> remotePlayer{ nullptr }; //!< their player pawn
  std::vector<NetworkPlayerSpawnData> spawnOrder;
  Mob* mob{ nullptr }; //!< Our managed mob structure for PVP
  CombatBattleState* combatPtr{ nullptr };
  TimeFreezeBattleState* timeFreezePtr{ nullptr };
  std::vector<NetworkSyncBattleState*> syncStates;
  CardComboBattleState* cardComboStatePtr{ nullptr };
  CardSelectBattleState* cardStatePtr{ nullptr };
  BattleStartBattleState* startStatePtr{ nullptr };
  std::shared_ptr<Netplay::PacketProcessor> packetProcessor;

  // Custom init steps
  void Init() override final;

  // netcode recieve funcs
  void ReceiveHandshakeSignal(const Poco::Buffer<char>& buffer);
  void ReceiveSyncSignal(const Poco::Buffer<char>& buffer);
  void ReceiveFrameData(const Poco::Buffer<char>& buffer);

  void ProcessPacketBody(NetPlaySignals header, const Poco::Buffer<char>&);
  bool IsRemoteBehind();
  void UpdatePingIndicator(frame_time_t frames);
  
  // This utilized BattleSceneBase::SpawnOtherPlayer() but adds some setup for networking
  void SpawnRemotePlayer(std::shared_ptr<Player> newRemotePlayer, int x, int y);

  // We need to intercept the filtered cards for netplay
  void OnSelectNewCards(const std::shared_ptr<Player>& player, std::vector<Battle::Card>& cards);

  // Battle state hooks
  std::function<bool()> HookPlayerWon(CombatBattleState& combat, BattleOverBattleState& over);
  std::function<bool()> HookPlayerLost(CombatBattleState& combat, BattleOverBattleState& over);
  std::function<bool()> HookPlayerDecrosses(CharacterTransformBattleState& forms);
  std::function<bool()> HookFormChangeEnd(CharacterTransformBattleState& form, CardSelectBattleState& cardSelect);
  std::function<bool()> HookOnCardSelectEvent();
public:
  using BattleSceneBase::ProcessNewestComponents;

  // netcode send funcs
  void SendHandshakeSignal(uint8_t syncStateIndex); // send player data to start the next round
  void SendSyncSignal(uint8_t syncStateIndex);
  void SendFrameData(std::vector<InputEvent>& events, unsigned int frameNumber); // send our key or gamepad events along with frame data
  void SendPingSignal();

  void OnHit(Entity& victim, const Hit::Properties& props) override final;
  void onUpdate(double elapsed) override final;
  void onDraw(IRenderer& renderer) override final;
  void onExit() override;
  void onEnter() override;
  void onStart() override;
  void onResume() override;
  void onEnd() override;

  const NetPlayFlags& GetRemoteStateFlags();
  const double GetAvgLatency() const;

  /**
   * @brief Construct scene with selected player, generated mob data, and the folder to use
   */
  NetworkBattleScene(swoosh::ActivityController&, NetworkBattleSceneProps& props, BattleResultsFunc onEnd=nullptr);
  
  /**
   * @brief Clears all nodes and components
   */
  ~NetworkBattleScene();

};