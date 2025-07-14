#include "bnDownloadScene.h"
#include "bnBufferReader.h"
#include "bnBufferWriter.h"
#include "../stx/string.h"
#include "../stx/zip_utils.h"
#include "../bnPlayer.h"
#include "../bindings/bnScriptedPlayer.h"
#include "../bnPackageManager.h"
#include "../bnPlayerPackageManager.h"
#include "../bnCardPackageManager.h"
#include "../bnBlockPackageManager.h"
#include "../bnLuaLibraryPackageManager.h"
#include "../bindings/bnScriptedCard.h"
#include "../bindings/bnScriptedBlock.h"
#include "../bnRandom.h"
#include <Segues/PixelateBlackWashFade.h>

constexpr std::string_view CACHE_FOLDER = "cache";

// class DownloadScene
DownloadScene::DownloadScene(swoosh::ActivityController& ac, const DownloadSceneProps& props) : 
  downloadSuccess(props.downloadSuccess),
  coinFlip(props.coinFlip),
  lastScreen(props.lastScreen),
  playerHash(props.playerHash),
  remotePlayer(props.remotePlayer),
  remoteBlocks(props.remotePlayerBlocks),
  label(Font::Style::small),
  Scene(ac)
{
  playerCardPackageList = props.cardPackageHashes;
  playerBlockPackageList = props.blockPackageHashes;

  std::sort(playerCardPackageList.begin(), playerCardPackageList.end());
  playerCardPackageList.erase(std::unique(playerCardPackageList.begin(), playerCardPackageList.end()), playerCardPackageList.end());

  std::sort(playerBlockPackageList.begin(), playerBlockPackageList.end());
  playerBlockPackageList.erase(std::unique(playerBlockPackageList.begin(), playerBlockPackageList.end()), playerBlockPackageList.end());

  std::string working = "Cards:";
  for (int i = 0; i < playerCardPackageList.size(); i++) {
    auto p = playerCardPackageList.at(i);
    working = working + "\n    " + p.packageId + " (" + p.md5 + ")";
  }

  Logger::Log(LogLevel::net, working);

  working = "Blocks:";
  for (int i = 0; i < playerBlockPackageList.size(); i++) {
    auto p = playerBlockPackageList.at(i);
    working = working + "\n    " + p.packageId + " (" + p.md5 + ")";
  }

  Logger::Log(LogLevel::net, working);

  downloadSuccess = false; 

  packetProcessor = props.packetProcessor;
  packetProcessor->SetKickCallback([this] {
    Logger::Logf(LogLevel::info, "Disconnected from opponent silence!");
    packetProcessor->SetKickCallback([] {});
    packetProcessor->EnableKickForSilence(false);

    this->Abort();
  });

  packetProcessor->EnableKickForSilence(true);

  lastUpdate = std::chrono::steady_clock::now();
  // send handshake + begin coinflip before reading packets
  SendHandshake();
  SendCoinFlip();

  // queued packets will come in after this
  packetProcessor->SetPacketBodyCallback([this](NetPlaySignals header, const Poco::Buffer<char>& body) {
    this->ProcessPacketBody(header, body);
  });

  bg = sf::Sprite(lastScreen);
  bg.setColor(sf::Color(255, 255, 255, 200));

  overlayTex = Textures().LoadFromFile("resources/scenes/download/overlay.png");
  overlay.setTexture(*overlayTex.get(), true);
  overlay.setScale(2.f, 2.f);

  downloadComplete = Audio().LoadFromFile("resources/sfx/compile_complete.ogg");
  downloadItem = Audio().LoadFromFile("resources/sfx/compile_item.ogg");

  setView(sf::Vector2u(480, 320));

  std::filesystem::create_directories(CACHE_FOLDER);

  ResetRemotePartitions();
}

DownloadScene::~DownloadScene()
{

}

void DownloadScene::SendHandshake()
{
  Poco::Buffer<char> buffer(0);
  BufferWriter writer;
  writer.Write(buffer, NetPlaySignals::download_handshake);

  // create a new seed based on the time to avoid using the same seed every battle
  mySeed = (unsigned int)time(0);
  writer.Write<uint32_t>(buffer, mySeed);

  writer.WriteTerminatedString(buffer, Game::Version);

  uint64_t id = packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer).second;
  packetProcessor->UpdateHandshakeID(id);

  Logger::Logf(LogLevel::info, "Sending handshake");
}

void DownloadScene::SendCoinFlip() {
  Poco::Buffer<char> buffer(0);
  BufferWriter writer;
  writer.Write(buffer, NetPlaySignals::coin_flip);

  // https://stackoverflow.com/a/43454179
  // mac os has microsecond resolution, while windows + linux have higher resolution
  // using microseconds on all platforms to make sure this works well on all platforms
  auto time = std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::system_clock::now().time_since_epoch()
  );

  getController().SeedRand(static_cast<unsigned>(time.count()));

  // this is intentionally desynced here from the SeedRand above, but syncs back up later when the scene exits to pvp
  // we want to use the same random generator due to implemenation differences of RAND_MAX
  // on linux RAND_MAX is 2147483647 while on windows it is 32767,
  // giving a far higher probability of being player 2 on linux when using rand() instead of SyncedRand()
  coinValue = SyncedRand();

  writer.Write<uint32_t>(buffer, coinValue);

  Logger::Logf(LogLevel::debug, "Coin value was %i with seed %u", coinValue, getController().GetRandSeed());

  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void DownloadScene::ResetRemotePartitions()
{
  CardPackagePartitioner& cardPartitioner = getController().CardPackagePartitioner();
  cardPartitioner.CreateNamespace(Game::RemotePartition);
  cardPartitioner.GetPartition(Game::RemotePartition).ErasePackages();

  PlayerPackagePartitioner& playerPartitioner = getController().PlayerPackagePartitioner();
  playerPartitioner.CreateNamespace(Game::RemotePartition);
  playerPartitioner.GetPartition(Game::RemotePartition).ErasePackages();

  BlockPackagePartitioner& blockPartitioner = getController().BlockPackagePartitioner();
  blockPartitioner.CreateNamespace(Game::RemotePartition);
  blockPartitioner.GetPartition(Game::RemotePartition).ErasePackages();

  LuaLibraryPackagePartitioner& libPartitioner = getController().LuaLibraryPackagePartitioner();
  libPartitioner.CreateNamespace(Game::RemotePartition);
  libPartitioner.GetPartition(Game::RemotePartition).ErasePackages();
}

CardPackageManager& DownloadScene::RemoteCardPartition()
{
  CardPackagePartitioner& partitioner = getController().CardPackagePartitioner();
  return partitioner.GetPartition(Game::RemotePartition);
}

CardPackageManager& DownloadScene::LocalCardPartition()
{
  return getController().CardPackagePartitioner().GetPartition(Game::LocalPartition);
}

BlockPackageManager& DownloadScene::RemoteBlockPartition()
{
  return getController().BlockPackagePartitioner().GetPartition(Game::RemotePartition);
}

BlockPackageManager& DownloadScene::LocalBlockPartition()
{
  return getController().BlockPackagePartitioner().GetPartition(Game::LocalPartition);
}

PlayerPackageManager& DownloadScene::RemotePlayerPartition()
{
  return getController().PlayerPackagePartitioner().GetPartition(Game::RemotePartition);
}

PlayerPackageManager& DownloadScene::LocalPlayerPartition()
{
  return getController().PlayerPackagePartitioner().GetPartition(Game::LocalPartition);
}

void DownloadScene::RemoveFromDownloadList(const std::string& id)
{
  auto iter = contentToDownload.find(id);
  if (iter != contentToDownload.end()) {
    contentToDownload.erase(iter);
  }
}

bool DownloadScene::AllTasksComplete()
{
  return coinFlipComplete && cardPackageRequested && playerPackageRequested && blockPackageRequested && contentToDownload.empty();
}

void DownloadScene::TradePlayerPackageData(const PackageHash& hash)
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write<NetPlaySignals>(buffer, NetPlaySignals::trade_player_package);
  writer.WriteTerminatedString(buffer, hash.packageId);
  writer.WriteTerminatedString(buffer, hash.md5);
  packetProcessor->SendPacket(Reliability::Reliable, buffer);
}

void DownloadScene::TradeCardPackageData(const std::vector<PackageHash>& hashes)
{
  // Upload card list to remote
  packetProcessor->SendPacket(Reliability::Reliable, SerializeListOfHashes(NetPlaySignals::trade_card_package_list, hashes));
}

void DownloadScene::TradeBlockPackageData(const std::vector<PackageHash>& hashes)
{
  // Upload card list to remote
  packetProcessor->SendPacket(Reliability::Reliable, SerializeListOfHashes(NetPlaySignals::trade_block_package_list, hashes));
}

void DownloadScene::RequestPlayerPackageData(const std::string& packageId)
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write<NetPlaySignals>(buffer, NetPlaySignals::player_package_request);
  writer.WriteTerminatedString(buffer, packageId);
  packetProcessor->SendPacket(Reliability::Reliable, buffer);
}

void DownloadScene::RequestCardPackageList(const std::vector<std::string>& packageIds)
{
  for (const std::string& pid : packageIds) {
    BufferWriter writer;
    Poco::Buffer<char> buffer{ 0 };
    writer.Write<NetPlaySignals>(buffer, NetPlaySignals::card_package_request);
    writer.WriteTerminatedString(buffer, pid);
    packetProcessor->SendPacket(Reliability::Reliable, buffer);
  }
}

void DownloadScene::RequestBlockPackageList(const std::vector<std::string>& packageIds)
{
  for (const std::string& pid : packageIds) {
    BufferWriter writer;
    Poco::Buffer<char> buffer{ 0 };
    writer.Write<NetPlaySignals>(buffer, NetPlaySignals::block_package_request);
    writer.WriteTerminatedString(buffer, pid);
    packetProcessor->SendPacket(Reliability::Reliable, buffer);
  }
}

void DownloadScene::SendDownloadComplete(bool value)
{
  if (!downloadFlagSet) {
    downloadSuccess = value;
    downloadFlagSet = true;

    Poco::Buffer<char> buffer{ 0 };
    NetPlaySignals type{ NetPlaySignals::downloads_complete };
    buffer.append((char*)&type, sizeof(NetPlaySignals));
    buffer.append((char*)&downloadSuccess, sizeof(bool));

    packetProcessor->SendPacket(Reliability::Reliable, buffer);
  }
}

void DownloadScene::SendTransition()
{
  if (!transitionSignalSent) {
    transitionSignalSent = true;

    Poco::Buffer<char> buffer{ 0 };
    NetPlaySignals type{ NetPlaySignals::download_transition };
    buffer.append((char*)&type, sizeof(NetPlaySignals));

    packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
  }
}

void DownloadScene::SendPing()
{
  Poco::Buffer<char> buffer{ 0 };
  NetPlaySignals type{ NetPlaySignals::ping };
  buffer.append((char*)&type, sizeof(NetPlaySignals));

  packetProcessor->SendPacket(Reliability::Unreliable, buffer);
}

void DownloadScene::ProcessPacketBody(NetPlaySignals header, const Poco::Buffer<char>& body)
{
  switch (header) {
  case NetPlaySignals::download_handshake:
    Logger::Logf(LogLevel::info, "Remote is sending initial handshake");
    this->RecieveHandshake(body);
    break;
  case NetPlaySignals::coin_flip:
    Logger::Logf(LogLevel::info, "Remote is sending a coin flip");
    this->RecieveCoinFlip(body);
    break;
  case NetPlaySignals::trade_card_package_list:
    Logger::Logf(LogLevel::info, "Remote is requesting to compare the card packages...");
    this->RecieveTradeCardPackageData(body);
    break;
  case NetPlaySignals::trade_block_package_list:
    Logger::Logf(LogLevel::info, "Remote is requesting to compare the block packages...");
    this->RecieveTradeBlockPackageData(body);
    break;
  case NetPlaySignals::trade_player_package:
    Logger::Logf(LogLevel::info, "Remote is requesting to compare the player packages...");
    this->RecieveTradePlayerPackageData(body);
    break;
  case NetPlaySignals::player_package_request:
    Logger::Logf(LogLevel::info, "Remote is requesting to download the player package...");
    this->RecieveRequestPlayerPackageData(body);
    break;
  case NetPlaySignals::card_package_request:
    Logger::Logf(LogLevel::info, "Remote is requesting to download a card package...");
    this->RecieveRequestCardPackageData(body);
    break;
  case NetPlaySignals::block_package_request:
    Logger::Logf(LogLevel::info, "Remote is requesting to download a block package...");
    this->RecieveRequestBlockPackageData(body);
    break;
  case NetPlaySignals::card_package_download:
    Logger::Logf(LogLevel::info, "Downloading card package...");
    this->DownloadPackageData<CardPackageManager, ScriptedCard>(body, RemoteCardPartition());
    break;
  case NetPlaySignals::block_package_download:
    Logger::Logf(LogLevel::info, "Downloading block package...");
    this->DownloadPackageData<BlockPackageManager, ScriptedBlock>(body, RemoteBlockPartition());
    break;
  case NetPlaySignals::player_package_download:
    Logger::Logf(LogLevel::info, "Downloading player package...");
    this->DownloadPlayerData(body);
    break;
  case NetPlaySignals::downloads_complete:
    this->RecieveDownloadComplete(body);
    break;
  case NetPlaySignals::download_transition:
    Logger::Logf(LogLevel::info, "Transitioning to pvp...");
    this->RecieveTransition(body);
    break;
  }
}

void DownloadScene::RecieveTradeCardPackageData(const Poco::Buffer<char>& buffer)
{
  std::vector<PackageHash> packageCardList = DeserializeListOfHashes(buffer);
  std::vector<std::string> requestList;
  CardPackageManager& packageManager = LocalCardPartition();

  for (PackageHash& remotePackage : packageCardList) {
    auto& [packageId, md5] = remotePackage;

    bool needsDownload = (packageManager.HasPackage(packageId) && DifferentHash(packageManager, packageId, md5));
    needsDownload = needsDownload || !packageManager.HasPackage(packageId);

    if (needsDownload) {
      requestList.push_back(packageId);
      contentToDownload[packageId] = "Downloading";
    }
  }

  Logger::Logf(LogLevel::info, "Recieved remote's card package list size: %d", packageCardList.size());

  // move to the next state
  if (requestList.size()) {
    remoteCardPackageList = requestList;
    Logger::Logf(LogLevel::info, "Need to download %d card packages", requestList.size());
    std::string needed = "Cards needed:";
    for (int i = 0; i < requestList.size(); i++) {
      needed = needed + "\n" + requestList[i];
    }
    RequestCardPackageList(requestList);
  }
  else {
    Logger::Logf(LogLevel::info, "Nothing to download.");
  }
  cardPackageRequested = true;
}

void DownloadScene::RecieveTradeBlockPackageData(const Poco::Buffer<char>& buffer)
{
  std::vector<PackageHash> packageBlockList = DeserializeListOfHashes(buffer);
  std::vector<std::string> requestList;
  BlockPackageManager& packageManager = LocalBlockPartition();

  for (PackageHash& remotePackage : packageBlockList) {
    auto& [packageId, md5] = remotePackage;

    bool needsDownload = (packageManager.HasPackage(packageId) && DifferentHash(packageManager, packageId, md5));
    needsDownload = needsDownload || !packageManager.HasPackage(packageId);

    if (needsDownload) {
      requestList.push_back(packageId);
      contentToDownload[packageId] = "Downloading";
      remoteBlocks.push_back(PackageAddress{ Game::RemotePartition, packageId });
    }
    else {
      remoteBlocks.push_back(PackageAddress{ Game::LocalPartition, packageId });
    }
  }

  Logger::Logf(LogLevel::info, "Recieved remote's block package list size: %d", packageBlockList.size());

  // move to the next state
  if (requestList.size()) {
    Logger::Logf(LogLevel::info, "Need to download %d block packages", requestList.size());
    std::string needed = "Blocks needed:";
    for (int i = 0; i < requestList.size(); i++) {
      needed = needed + "\n" + requestList[i];
    }

    Logger::Log(LogLevel::net, needed);
    RequestBlockPackageList(requestList);
  }
  else {
    Logger::Logf(LogLevel::info, "Nothing to download.");
  }
  blockPackageRequested = true;
}

void DownloadScene::RecieveHandshake(const Poco::Buffer<char>& buffer)
{
  BufferReader reader;
 
  unsigned int seed = reader.Read<uint32_t>(buffer);
  maxSeed = std::max(seed, mySeed);
  std::string remoteVersion = reader.ReadTerminatedString(buffer);
  if (remoteVersion.compare(Game::Version) == 0) {
    Logger::Log(LogLevel::net, "Version matched");
  }
  else {
    Logger::Log(LogLevel::critical, "Version mismatch: Local `" + std::string(Game::Version) + "` vs. Remote `" + remoteVersion + "`");
    Abort();
    return;
  }

  // mark handshake as completed
  this->remoteHandshake = true;
}

void DownloadScene::RecieveTradePlayerPackageData(const Poco::Buffer<char>& buffer)
{
  BufferReader reader;
  std::string packageId = reader.ReadTerminatedString(buffer);
  std::string md5 = reader.ReadTerminatedString(buffer);

  PlayerPackageManager& packageManager = LocalPlayerPartition();
  bool needsDownload = (packageManager.HasPackage(packageId) && DifferentHash(packageManager, packageId, md5));
  needsDownload = needsDownload || !packageManager.HasPackage(packageId);

  if (needsDownload) {
    contentToDownload[packageId] = "Downloading";
    // This will download the player data and also set the remotePlayerHash when completed successfully
    RequestPlayerPackageData(packageId);
    remotePlayer = PackageAddress{ Game::RemotePartition, packageId };
  }
  else {
    remotePlayer = PackageAddress{ Game::LocalPartition, packageId };
  }

  playerPackageRequested = true;
}

void DownloadScene::RecieveRequestPlayerPackageData(const Poco::Buffer<char>& buffer)
{
  BufferReader reader;
  std::string packageId = reader.ReadTerminatedString(buffer);

  if (packageId.size()) {
    Logger::Logf(LogLevel::info, "Recieved download request for player hash %s", packageId.c_str());
    packetProcessor->SendPacket(Reliability::BigData,
      SerializePackageData(
        packageId,
        NetPlaySignals::player_package_download,
        LocalPlayerPartition()
      )
    );
  }
}

void DownloadScene::RecieveRequestCardPackageData(const Poco::Buffer<char>& buffer)
{
  BufferReader reader;
  std::string packageId = reader.ReadTerminatedString(buffer);

  if (!packageId.empty()) {
    Logger::Logf(LogLevel::info, "Recieved download request for %s card package", packageId.c_str());

    packetProcessor->SendPacket(Reliability::BigData,
      SerializePackageData<CardPackageManager>(
        packageId,
        NetPlaySignals::card_package_download,
        LocalCardPartition()
      )
    );
  }
}

void DownloadScene::RecieveRequestBlockPackageData(const Poco::Buffer<char>& buffer)
{
  BufferReader reader;
  std::string packageId = reader.ReadTerminatedString(buffer);

  if (!packageId.empty()) {
    Logger::Logf(LogLevel::info, "Recieved download request for %s block package", packageId.c_str());

    packetProcessor->SendPacket(Reliability::BigData,
      SerializePackageData<BlockPackageManager>(
        packageId,
        NetPlaySignals::block_package_download,
        LocalBlockPartition()
        )
    );
  }
}

void DownloadScene::RecieveDownloadComplete(const Poco::Buffer<char>& buffer)
{
  bool result{};
  std::memcpy(&result, buffer.begin(), sizeof(bool));

  if (result) {
    remoteSuccess = true;
  }
  else {
    // downloadSuccess ref will tell matchmaking scene we failed
    Abort();
  }

  Logger::Logf(LogLevel::info, "Remote says download complete. Result: %s", result ? "Success" : "Fail");
}

void DownloadScene::RecieveTransition(const Poco::Buffer<char>& buffer)
{
  packetProcessor->SetPacketBodyCallback(nullptr); // queue next packets for pvp
  transitionToPvp = true;

  // sync seed
  getController().SeedRand(maxSeed); 
  Logger::Logf(LogLevel::debug, "Using seed %d", maxSeed);
}

void DownloadScene::RecieveCoinFlip(const Poco::Buffer<char>& buffer)
{
  BufferReader reader;
  unsigned int remoteValue = reader.Read<uint32_t>(buffer);

  // We can't both have the same result
  if (remoteValue == coinValue) {
    SendCoinFlip();
    return;
  }

  coinFlip = coinValue > remoteValue;
  coinFlipComplete = true;

  Logger::Logf(LogLevel::debug, "Coin flip completed. Local value: %d. Remote value: %d. Final value: %d", coinValue, remoteValue, coinFlip);
}

void DownloadScene::DownloadPlayerData(const Poco::Buffer<char>& buffer)
{
  BufferReader reader;
  std::string packageId = reader.ReadTerminatedString(buffer);

  if (packageId.empty()) return;
  RemoveFromDownloadList(packageId);

  size_t file_len = reader.Read<uint32_t>(buffer);
  std::string path = "cache/" + stx::rand_alphanum(12) + ".zip";

  std::fstream file;
  file.open(path, std::ios::out | std::ios::binary);

  stx::result_t<std::string> result(std::nullptr_t{}, "Unset");

  if (file.is_open()) {
    while (file_len > 0) {
      char byte = reader.Read<char>(buffer);
      file << byte;
      file_len--;
    }

    file.close();

    result = RemotePlayerPartition().LoadPackageFromZip<ScriptedPlayer>(path);
  }
  
  if (result.is_error()) {
    Logger::Logf(LogLevel::critical, "Failed to download custom navi with package ID %s: %s", packageId.c_str(), result.error_cstr());

    // There was a problem creating the file
    SendDownloadComplete(false);
  }
  else {
    Logger::Log(LogLevel::net, "Successfully downloaded opponent's Player mod");
  }
}

std::vector<PackageHash> DownloadScene::DeserializeListOfHashes(const Poco::Buffer<char>& buffer)
{
  size_t len{};
  size_t read{};
  std::vector<PackageHash> list;

  // list length
  std::memcpy(&len, buffer.begin() + read, sizeof(size_t));
  read += sizeof(size_t);

  while (len > 0) {
    // package id
    size_t id_len{};
    std::memcpy(&id_len, buffer.begin() + read, sizeof(size_t));
    read += sizeof(size_t);

    std::string id = std::string(buffer.begin() + read, id_len);
    read += id_len;

    // md5
    size_t md5_len{};
    std::memcpy(&md5_len, buffer.begin() + read, sizeof(size_t));
    read += sizeof(size_t);

    std::string md5 = std::string(buffer.begin() + read, md5_len);
    read += md5_len;

    list.push_back({ id, md5 });

    len--;
  }

  return list;
}

Poco::Buffer<char> DownloadScene::SerializeListOfHashes(NetPlaySignals header, const std::vector<PackageHash>& list)
{
  Poco::Buffer<char> data{ 0 };

  // header
  data.append((char*)&header, sizeof(NetPlaySignals));

  // list length
  size_t len = list.size();
  data.append((char*)&len, sizeof(size_t));

  for(const PackageHash& hash : list) {
    // package id
    size_t sz = hash.packageId.length();
    data.append((char*)&sz, sizeof(size_t));
    data.append(hash.packageId.c_str(), hash.packageId.length());

    // md5
    sz = hash.md5.length();
    data.append((char*)&sz, sizeof(size_t));
    data.append(hash.md5.c_str(), hash.md5.length());
  }

  return data;
}

template<template<typename> class PackageManagerType, class MetaType>
bool DownloadScene::DifferentHash(PackageManagerType<MetaType>& packageManager, const std::string& packageId, const std::string& desiredFingerprint)
{
  MetaType& package = packageManager.FindPackageByID(packageId);
  return package.GetPackageFingerprint() != desiredFingerprint;
}

template<typename PackageManagerType, typename ScriptedDataType>
void DownloadScene::DownloadPackageData(const Poco::Buffer<char>& buffer, PackageManagerType& pm)
{
  BufferReader reader;
  std::string packageId = reader.ReadTerminatedString(buffer);

  if (packageId.empty()) return;
  RemoveFromDownloadList(packageId);

  size_t file_len = reader.Read<uint32_t>(buffer);
  std::string path = "cache/" + stx::rand_alphanum(12) + ".zip";

  std::fstream file;
  file.open(path, std::ios::out | std::ios::binary);

  stx::result_t<std::string> result(std::nullptr_t{}, "Unset");

  if (file.is_open()) {
    while (file_len > 0) {
      char byte = reader.Read<char>(buffer);
      file << byte;
      file_len--;
    }

    file.close();

    result = pm.template LoadPackageFromZip<ScriptedDataType>(path);
  }

  if (result.is_error()) {
    Logger::Logf(LogLevel::critical, "Failed to download package with package ID %s: %s", packageId.c_str(), result.error_cstr());

    // There was a problem creating the file
    SendDownloadComplete(false);
  }
}

template<typename PackageManagerType>
Poco::Buffer<char> DownloadScene::SerializePackageData(const std::string& packageId, NetPlaySignals header, PackageManagerType& packageManager)
{
  Poco::Buffer<char> buffer{ 0 };
  std::vector<char> fileBuffer;

  BufferWriter writer;
  size_t len = 0;
  
  auto result = packageManager.GetPackageFilePath(packageId);
  if (result.is_error()) {
    Logger::Logf(LogLevel::critical, "Could not serialize package: %s", result.error_cstr());

    // Give the remote client a headsup abort
    SendDownloadComplete(false);
  }
  else {
    std::string path = result.value();
    
    if (auto result = stx::zip(path, path + ".zip"); result.value()) {
      path = path + ".zip";

      std::ifstream fs(path, std::ios::binary | std::ios::ate);
      std::ifstream::pos_type pos = fs.tellg();
      len = pos;
      fileBuffer.resize(len);
      fs.seekg(0, std::ios::beg);
      fs.read(&fileBuffer[0], pos);
    }
  }

  // header
  writer.Write(buffer, header);

  // package name
  writer.WriteTerminatedString(buffer, packageId);

  // file size
  writer.Write<uint32_t>(buffer, len);

  // file contents
  writer.WriteBytes<char>(buffer, fileBuffer.data(), fileBuffer.size());

  return buffer;
}

void DownloadScene::Abort()
{
  if (!aborting) {
    for (auto& [key, value] : contentToDownload) {
      value = "Failed";
    }
    Logger::Logf(LogLevel::critical, "Aborting");
    SendDownloadComplete(false);
    aborting = true;
  }
}

void DownloadScene::onUpdate(double elapsed)
{
  auto now = std::chrono::steady_clock::now();
  auto dif = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch() - lastUpdate.time_since_epoch());

  lastUpdate = now;

  Logger::Log(LogLevel::net, "DownloadScene tick after " + std::to_string(dif.count()) + " ms");
  if (!(packetProcessor->IsHandshakeAck() && remoteHandshake) && !aborting) return;

  // Don't trade data until handshake has arrived.
  // Handshake is ignored and connection aborted on version mismatch, so this
  // will keep package data from trading even though we're going to abort.
  if (!hasTradedData && remoteHandshake) {
    hasTradedData = true;

    this->TradeBlockPackageData(playerBlockPackageList);
    this->TradeCardPackageData(playerCardPackageList);
    this->TradePlayerPackageData(playerHash);
    return;
  }

  SendPing();

  if (aborting || sf::Keyboard::isKeyPressed(sf::Keyboard::Escape)) {
    abortingCountdown -= from_seconds(elapsed);
    if (abortingCountdown <= frames(0)) {
      // abort match
      using effect = swoosh::types::segue<PixelateBlackWashFade>;
      getController().pop<effect>();
    }

    return;
  }
  
  if (AllTasksComplete()) {
    SendDownloadComplete(true);

    if (downloadSuccess && remoteSuccess) {
      SendTransition();

      if (!downloadSoundPlayed && remainingTokens >= maxTokens && maxTokens > 0) {
        downloadSoundPlayed = true;
        Audio().Play(downloadComplete);
      }
    }
  }

  if(!downloadSoundPlayed) {
    // If this sfx hasn't played the "download" isn't caught up in the render step,
    // so play some busy sounds while we wait
    Audio().Play(downloadItem, AudioPriority::high);
  }

  elapsedFrames += from_seconds(elapsed);

  if (transitionToPvp) {
    // re-using aborting countdown as a way to delay the transition
    abortingCountdown -= from_seconds(elapsed);
    if (abortingCountdown <= frames(0)) {
      getController().pop();
    }
  }
}

void DownloadScene::onDraw(sf::RenderTexture& surface)
{
  if (inView) {
    surface.draw(bg);
  }

  float w = static_cast<float>(getController().getVirtualWindowSize().x);
  float h = 30;

  // re-position
  sf::FloatRect bounds = label.GetLocalBounds();
  label.SetColor(sf::Color::White);

  sf::Sprite icon;

  // speed up if screen is about to leave
  int tokens = !transitionToPvp ? elapsedFrames.count() * 2 : (elapsedFrames.count() * (100 / std::max(1, (int)abortingCountdown.count())));
  remainingTokens = tokens;
  maxTokens = 0;

  CardPackageManager& packageManager = RemoteCardPartition();
  for (std::string& id : remoteCardPackageList) {
    if (!packageManager.HasPackage(id)) continue;

    CardMeta& meta = packageManager.FindPackageByID(id);
    std::string str = meta.properties.shortname + " - " + id;
    maxTokens += str.size();
    if (str.size() > remainingTokens) {
      str = str.substr(0, remainingTokens);
    }
    label.SetString(str);
    remainingTokens = std::max(0u, (unsigned)(remainingTokens - str.size()));

    sf::FloatRect bounds = label.GetLocalBounds();
    if (std::shared_ptr<sf::Texture> iconTexture = meta.GetIconTexture()) {
      icon.setTexture(*iconTexture, true);
      float iconHeight = icon.getLocalBounds().height;
      icon.setPosition(20, h);
      icon.setOrigin(0, iconHeight/4);
      surface.draw(icon);
    }

    label.setPosition(20 + 16 + 2, h);

    h += 15.0f;

    surface.draw(label);
  }
  surface.draw(overlay);
}

void DownloadScene::onLeave()
{
}

void DownloadScene::onExit()
{
}

void DownloadScene::onEnter()
{
}

void DownloadScene::onStart()
{
  inView = true;
}

void DownloadScene::onResume()
{
}

void DownloadScene::onEnd()
{
  packetProcessor->SetPacketBodyCallback(nullptr);
  packetProcessor->SetKickCallback(nullptr);
}
