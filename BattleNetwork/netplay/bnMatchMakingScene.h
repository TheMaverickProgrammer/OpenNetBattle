#pragma once
#include <Swoosh/Activity.h>
#include <Poco/Buffer.h>

#include "../bnFont.h"
#include "../bnText.h"
#include "../bnDrawWindow.h"
#include "../bnAudioResourceManager.h"
#include "../bnTextureResourceManager.h"
#include "../bnBackground.h"
#include "../bnSpriteProxyNode.h"
#include "../bnPA.h"
#include "../bnAnimatedTextBox.h"
#include "../bnCardFolder.h" 
#include "../bnScene.h"

#include "bnNetPlayConfig.h"
#include "bnMatchMakingPacketProcessor.h"
/**
 * @class MatchMakingScene
 * @author mav
 * @date 08/22/2020
 * @brief Connect with a remote and battle eachother
 */
class MatchMakingScene : public Scene {
private:
  enum class ReturningScene {
    DownloadScene,
    BattleScene,
    Null
  };

  ReturningScene returningFrom{ ReturningScene::Null };
  bool isScreenReady{ false };
  bool closing{ false };
  bool remoteIsReady{ false };
  bool clientIsReady{ false };
  bool isInFlashyVSIntro{ false };
  bool isInBattleStartup{ false };
  bool hasProcessedCards{ false };
  bool canProceedToBattle{ false };
  bool infoMode{ true }; // we start here and then allow the player to toggle
  bool handshakeComplete{ false };
  bool playVS{ true };
  unsigned pvpCoinFlip{};
  double sequenceTimer{ 0.0 }; // in seconds
  double flashCooldown{ 0 };
  size_t selectionIndex{ 0 }; // 0 = text input field widget
  std::string myIP, theirIP; // IP strings for textbox
  std::string selectedNaviId;
  PackageAddress remoteNaviPackage;
  std::vector<PackageAddress> remotePlayerBlocks;
  std::unique_ptr<CardFolder> folder;
  PA& pa;
  NetPlayConfig netplayconfig;
  Background* gridBG;
  SpriteProxyNode navigator;
  SpriteProxyNode ui;
  SpriteProxyNode vs, vsFaded;
  SpriteProxyNode greenBg;
  SpriteProxyNode clientPreview;
  SpriteProxyNode remotePreview;
  Animation uiAnim;
  AnimatedTextBox textbox;
  Text text, id;
  sf::Texture screen;
  std::shared_ptr<MatchMaking::PacketProcessor> packetProcessor;

  // event responses
  void HandleInfoMode();
  void HandleJoinMode();
  void HandleReady();
  void HandleCancel();
  void HandleGetIPFailure();
  void HandleCopyEvent();
  void HandlePasteEvent();

  // netplay comm.
  void ProcessPacketBody(NetPlaySignals header, const Poco::Buffer<char>& body);
  void SendConnectSignal();
  void SendHandshakeSignal(); // sent until we recieve a handshake
  void RecieveConnectSignal(const Poco::Buffer<char>&);
  void RecieveHandshakeSignal();

  // custom drawing
  void DrawIDInputWidget(IRenderer& renderer);
  void DrawCopyPasteWidget(IRenderer& renderer);

  const bool IsValidIPv4(const std::string& ip) const;

  void Reset();

public:
  friend class MatchMaking::PacketProcessor;

  MatchMakingScene(swoosh::ActivityController&, const std::string& naviId, std::unique_ptr<CardFolder>, PA&);
  ~MatchMakingScene();

  /**
 * @brief
 */
  void onStart();
  
  /**
   * @brief
   * @param elapsed in seconds
   */
  void onUpdate(double elapsed);
  
  void onLeave();
  void onEnter();
  void onResume();
  void onExit();

  /**
   * @brief Draws graphic
   * @param surface
   */
  void onDraw(IRenderer& renderer);
  
  void onEnd();
};
