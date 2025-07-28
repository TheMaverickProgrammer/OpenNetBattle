#pragma once

#include <Swoosh/Activity.h>
#include <Swoosh/Ease.h>
#include <Swoosh/Timer.h>
#include <Swoosh/EmbedGLSL.h>

#include <time.h>
#include <queue>
#include <typeinfo>
#include <SFML/Graphics.hpp>
#include <Poco/Net/DatagramSocket.h>
#include <Poco/Buffer.h>

#include "bnNetPlayPacketProcessor.h"
#include "../bnLoaderScene.h"
#include "../bnText.h"
#include "../../bnInputManager.h"
#include "../../bnDrawWindow.h"
#include "../../bnSceneNode.h"
#include "../bnPackageAddress.h"

class CardPackageManager;
class PlayerPackageManager;
class BlockPackageManager;

struct DownloadSceneProps;

class DownloadScene final : public Scene {
private:
  bool& downloadSuccess;
  bool inView{};
  bool downloadFlagSet{}, transitionSignalSent{}, transitionToPvp{}, aborting{}, remoteSuccess{}, remoteHandshake{}, hasTradedData{}, coinFlipComplete{};
  bool playerPackageRequested{}, cardPackageRequested{}, blockPackageRequested{};
  bool downloadSoundPlayed{};
  unsigned& coinFlip;
  unsigned coinValue{}, remainingTokens{}, maxTokens{};
  unsigned mySeed{}, maxSeed{};
  frame_time_t elapsedFrames{};
  frame_time_t abortingCountdown{frames(150)};
  size_t tries{}; //!< After so many attempts, quit the download...
  size_t packetAckId{};
  PackageHash playerHash;
  PackageAddress& remotePlayer;
  std::vector<PackageAddress>& remoteBlocks;
  std::vector<PackageHash> playerCardPackageList, playerBlockPackageList;
  std::vector<std::string> remoteCardPackageList, remoteBlockPackageList;
  std::map<std::string, std::string> contentToDownload;
  Text label;
  sf::Sprite bg, overlay; // background
  sf::RenderTexture surface;
  sf::Texture lastScreen;
  std::shared_ptr<sf::Texture> overlayTex;
  std::shared_ptr<sf::SoundBuffer> downloadItem, downloadComplete;
  std::shared_ptr<Netplay::PacketProcessor> packetProcessor;

  void ResetRemotePartitions(); // prepare remote namespace for incoming mods
  CardPackageManager& RemoteCardPartition();
  CardPackageManager& LocalCardPartition();
  BlockPackageManager& RemoteBlockPartition();
  BlockPackageManager& LocalBlockPartition();
  PlayerPackageManager& RemotePlayerPartition();
  PlayerPackageManager& LocalPlayerPartition();

  template<template<typename> class PackageManagerType, class MetaType>
  bool DifferentHash(PackageManagerType<MetaType>& packageManager, const std::string& packageId, const std::string& desiredFingerprint);
  bool AllTasksComplete();
  void RemoveFromDownloadList(const std::string& id);

  // Send direct data
  void SendHandshake();
  void SendPing(); //!< keep connections alive while clients download data
  void SendDownloadComplete(bool success);
  void SendTransition();
  void SendCoinFlip();

  // Initiate trades
  void TradePlayerPackageData(const PackageHash& hash);
  void TradeCardPackageData(const std::vector<PackageHash>& hashes);
  void TradeBlockPackageData(const std::vector<PackageHash>& hashes);


  // Initiate requests
  void RequestPlayerPackageData(const std::string& hash);
  void RequestCardPackageList(const std::vector<std::string>& hashes);
  void RequestBlockPackageList(const std::vector<std::string>& hashes);

  // Handle recieve 
  void RecieveHandshake(const Poco::Buffer<char>& buffer);
  void RecieveTradePlayerPackageData(const Poco::Buffer<char>& buffer);
  void RecieveTradeCardPackageData(const Poco::Buffer<char>& buffer);
  void RecieveTradeBlockPackageData(const Poco::Buffer<char>& buffer);
  void RecieveRequestPlayerPackageData(const Poco::Buffer<char>& buffer);
  void RecieveRequestCardPackageData(const Poco::Buffer<char>& buffer);
  void RecieveRequestBlockPackageData(const Poco::Buffer<char>& buffer);
  void RecieveDownloadComplete(const Poco::Buffer<char>& buffer);
  void RecieveTransition(const Poco::Buffer<char>& buffer);
  void RecieveCoinFlip(const Poco::Buffer<char>& buffer);

  // Downloads
  void DownloadPlayerData(const Poco::Buffer<char>& buffer);

  template<typename PackageManagerType, typename ScriptedDataType>
  void DownloadPackageData(const Poco::Buffer<char>& buffer, PackageManagerType& pm);

  // Serializers
  std::vector<PackageHash> DeserializeListOfHashes(const Poco::Buffer<char>& buffer);
  Poco::Buffer<char> SerializeListOfHashes(NetPlaySignals header, const std::vector<PackageHash>& list);

  template<typename PackageManagerType>
  Poco::Buffer<char> SerializePackageData(const std::string& packageId, NetPlaySignals header, PackageManagerType& pm);

  // Aux
  void Abort();
  void ProcessPacketBody(NetPlaySignals header, const Poco::Buffer<char>& body);

public:
  void onUpdate(double elapsed) override final;
  void onDraw(sf::RenderTexture& surface) override final;
  void onLeave() override;
  void onExit() override;
  void onEnter() override;
  void onStart() override;
  void onResume() override;
  void onEnd() override;

  /**
   * @brief Construct scene from previous screen's contents
   */
  DownloadScene(swoosh::ActivityController&, const DownloadSceneProps& props);
  ~DownloadScene();
};

struct DownloadSceneProps {
  std::vector<PackageHash> cardPackageHashes;
  std::vector<PackageHash> blockPackageHashes;
  PackageHash playerHash;
  Poco::Net::SocketAddress remoteAddress;
  std::shared_ptr<Netplay::PacketProcessor> packetProcessor;
  sf::Texture lastScreen;
  bool& downloadSuccess;
  unsigned& coinFlip;
  PackageAddress& remotePlayer;
  std::vector<PackageAddress>& remotePlayerBlocks;
};
