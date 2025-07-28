#pragma once

#include "../../battlescene/bnNetworkBattleSceneState.h"
#include <functional>

struct BattleSceneState;
class NetworkBattleScene;

/*
    \brief This state will resync with the remote player before starting the next state
*/
struct NetworkSyncBattleState final : public NetworkBattleSceneState {
  NetworkBattleScene* scene{ nullptr };
  std::function<void(const BattleSceneState*)> onStartCallback, onEndCallback;
  bool requestedSync{}, remoteRequestedSync{};
  frame_time_t syncFrame, flicker;

  NetworkSyncBattleState(NetworkBattleScene* scene);
  ~NetworkSyncBattleState();
  void SetStartCallback(std::function<void(const BattleSceneState*)>);
  void SetEndCallback(std::function<void(const BattleSceneState*)>);
  bool IsReady() const;
  void MarkSyncRequested();
  void MarkRemoteSyncRequested();
  frame_time_t GetSyncFrame() const;
  bool SetSyncFrame(frame_time_t);
  void onStart(const BattleSceneState* next) override;
  void onEnd(const BattleSceneState* last) override;
  void onUpdate(double elapsed) override;
  void onDraw(sf::RenderTexture& surface) override;
};