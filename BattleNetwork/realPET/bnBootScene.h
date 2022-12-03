#pragma once
#include <memory>
#include <future>
#include "../bnLoaderScene.h"
#include "../bnSpriteProxyNode.h"
#include "../bnFont.h"
#include "../bnText.h"

namespace RealPET {
  class BootScene final : public LoaderScene {
  private:
    // Status flags
    bool leaving{};
    unsigned progress{}, total{};
    unsigned ellipsis{};
    double totalElapsed{};
    frame_time_t fastStartFlicker{ frames(10) };
    std::string taskStr, incomingTaskStr;

    size_t logIdx{};

    struct log {
      operator std::string& () { return text; }
      std::string text{};
      int alpha{};
    };
    std::array<log, 13> logs;

    SpriteProxyNode logoSprite, alertSprite;
    SpriteProxyNode cursorSprite;
    std::shared_ptr<sf::SoundBuffer> bootsfx;
    Text taskLabel, logLabel;

  public:
    BootScene(swoosh::ActivityController& controller, TaskGroup&& tasks);
    void onStart() override;
    void onUpdate(double elapsed) override;
    void onLeave() override;
    void onExit() override;
    void onEnter() override;
    void onResume() override;
    void onDraw(IRenderer& renderer) override;
    void onEnd() override;
    void onTaskBegin(const std::string& taskName, float progress) override;
    void onTaskComplete(const std::string& taskName, float progress) override;
  };
}