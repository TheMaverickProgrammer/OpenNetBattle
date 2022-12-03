#include "bnBootScene.h"
#include "bnHomepageScene.h"
#include "../bnConfigScene.h"
#include "../bnShaderResourceManager.h"
#include "../bnTextureResourceManager.h"
#include "../bnAudioResourceManager.h"
#include "../bnMessage.h"

#include <Segues/SwipeIn.h>

using namespace swoosh::types;

namespace RealPET {
  BootScene::BootScene(swoosh::ActivityController& controller, TaskGroup&& tasks) :
    taskLabel(Font::Style::small),
    logLabel(Font::Style::thin),
    LoaderScene(controller, std::move(tasks))
  {
    // Title screen logo based on region
    std::shared_ptr<sf::Texture> logo;
    logo = Textures().LoadFromFile(TexturePaths::PET_LOGO);
 
    logoSprite.setTexture(logo);
    logoSprite.setOrigin(logoSprite.getLocalBounds().width / 2, logoSprite.getLocalBounds().height / 2);
    sf::Vector2f logoPos = sf::Vector2f(240.f, 160.f);
    logoSprite.setPosition(logoPos);

    bootsfx = Audio().LoadFromFile(SoundPaths::PET_LOGO);

    // Log output text
    float taskLabelH = taskLabel.GetLocalBounds().height;
    taskLabel.setOrigin(0.f, taskLabelH);
    taskLabel.setScale(2.f, 2.f);
    taskLabel.setPosition(0, 10.f);
    taskLabel.SetString("");

    float logLabelH = logLabel.GetLocalBounds().height;
    logLabel.setPosition(0, taskLabelH*2.f);
    logLabel.setScale(1.f, 1.f);
    logLabel.setOrigin(0.f, logLabelH);

    setView(sf::Vector2u(480, 320));
  }

  void BootScene::onStart()
  {
    // stream some music while we wait
    Audio().Play(bootsfx, AudioPriority::highest);

    // Begin performing tasks in the background
    LaunchTasks();
  }

  void BootScene::onUpdate(double elapsed)
  {
    totalElapsed += elapsed;

    for (log& L : logs) {
      L.alpha = std::max(0, L.alpha - 1);
    }

    if (Logger::GetNextLog(logs[logIdx])) {
      logs[logIdx].alpha = 255; // refresh
      logIdx = (logIdx + 1) % logs.max_size();
    }

    // If not ready, do no proceed past this point!
    if (IsComplete() == false || progress < 100) {
      ellipsis = (ellipsis + 1) % 4;
      std::string dots = std::string(static_cast<size_t>(ellipsis) + 1, '.');

      if (progress < total) {
        progress++;
      }

      std::string percent = std::to_string(progress) + "%";
      std::string label = taskStr + ": " + percent + dots;
      taskLabel.SetString(label);

      Poll();

      return;
    }
    else {
      taskLabel.SetString("BOOT OK. Press Any Button");

      bool anyKeyPress = Input().GetAnyKey() != sf::Keyboard::Unknown || Input().GetAnyGamepadButton() != Gamepad::BAD_CODE;
      if (anyKeyPress && !leaving) {
        Audio().Play(AudioType::CHIP_SELECT);

        // We want the next screen to be the main menu screen
        using tx = segue<SwipeIn<direction::down>, milli<500>>;
        getController().push<tx::to<RealPET::Homepage>>();
        leaving = true;
      }
    }
  }

  void BootScene::onLeave()
  {
  }

  void BootScene::onExit()
  {
  }

  void BootScene::onEnter()
  {
  }

  void BootScene::onResume()
  {
  }

  void BootScene::onDraw(IRenderer& renderer)
  {
    // draw logs in the background
    float padding = 1.5f;
    sf::Vector2f pos{0, 160*2.f};
    for (log& L : logs) {
      logLabel.SetString(L.text);
      logLabel.setPosition(pos);

      sf::Color color = sf::Color::White;

      if (L.text.find("[DEBUG]") != std::string::npos) {
        color = sf::Color::Yellow;
      }
      else if (L.text.find("[CRITICAL]") != std::string::npos) {
        color = sf::Color::Red;
      }

      color.a = L.alpha;

      logLabel.SetColor(color);

      renderer.submit(Clone(logLabel));

      pos.y -= logLabel.GetLocalBounds().height * padding;
    }

    // draw boot logo on top
    renderer.submit(&logoSprite);

    // draw task status label
    if (!leaving) {
      sf::Vector2f lastPos = taskLabel.getPosition();
      taskLabel.setPosition(lastPos.x + 2.f, lastPos.y + 2.f);
      taskLabel.SetColor(sf::Color::Black);
      renderer.submit(Clone(taskLabel));
      taskLabel.setPosition(lastPos);
      taskLabel.SetColor(sf::Color::White);
      renderer.submit(&taskLabel);
    }
  }

  void BootScene::onEnd()
  {
  }

  void BootScene::onTaskBegin(const std::string& taskName, float progress)
  {
    total = unsigned(progress * 100);
    taskStr = taskName;
    taskLabel.SetString(this->taskStr + " 00%");

    Logger::Logf(LogLevel::info, "[%.2f] Began task %s", progress, taskName.c_str());
  }

  void BootScene::onTaskComplete(const std::string& taskName, float progress)
  {
    total = unsigned(progress * 100);
    taskStr = taskName;

    Logger::Logf(LogLevel::info, "[%.2f] Completed task %s", progress, taskName.c_str());
  }
}