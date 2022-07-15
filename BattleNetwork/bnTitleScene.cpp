#include <Segues/DiamondTileCircle.h>
#include "bnTitleScene.h"
#include "bnConfigScene.h"
#include "bnShaderResourceManager.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"
#include "bnMessage.h"

#include "overworld/bnOverworldHomepage.h"

using namespace swoosh::types;

void TitleScene::CenterLabel()
{
  // update label position
  sf::FloatRect bounds = startLabel.GetLocalBounds();
  startLabel.setOrigin(bounds.width / 2.0f, bounds.height);
}

TitleScene::TitleScene(swoosh::ActivityController& controller, TaskGroup&& tasks) :
  startFont(Font::Style::thick),
  font(Font::Style::small),
  logLabel(font),
  startLabel(startFont),
  textbox({ 4, 255 }),
  LoaderScene(controller, std::move(tasks))
{
  // Title screen logo based on region
  std::shared_ptr<sf::Texture> logo;

  if (getController().CommandLineValue<std::string>("locale") == "jp") {
    logo = Textures().LoadFromFile("resources/scenes/title/tile.png");
  }
  else {
    logo = Textures().LoadFromFile("resources/scenes/title/tile_en.png");
  }

  std::shared_ptr<sf::Texture> bgTexture = Textures().LoadFromFile("resources/scenes/title/bg_blue.png");
  bgTexture->setRepeated(true);
  bgSprite.setTexture(bgTexture);
  bgSprite.setScale(2.f, 2.f);

  logoSprite.setTexture(logo);
  logoSprite.setOrigin(logoSprite.getLocalBounds().width / 2, logoSprite.getLocalBounds().height / 2);
  sf::Vector2f logoPos = sf::Vector2f(240.f, 160.f);
  logoSprite.setPosition(logoPos);

  progSprite.setTexture(Textures().LoadFromFile("resources/scenes/title/prog-pulse.png"));
  progSprite.setPosition(sf::Vector2f(380.f, 210.f));
  progSprite.setScale(2.f, 2.f);
  anim = Animation("resources/scenes/title/default.animation");
  anim << "default" << Animator::Mode::Loop;

  // Log output text
  font = Font(Font::Style::small);
  logLabel.setOrigin(0.f, logLabel.GetLocalBounds().height);
  logLabel.setScale(2.f, 2.f);

  // Press Start text
  startFont = Font(Font::Style::thick);

  startLabel.setPosition(sf::Vector2f(240.0f, 240.f));
  startLabel.setScale(2.f, 2.f);

  startLabel.SetString("Loading, Please Wait");
  CenterLabel();

  setView(sf::Vector2u(480, 320));
}

void TitleScene::onStart()
{
  // stream some music while we wait
  Audio().Stream("resources/loops/loop_theme.ogg");

  // Begin performing tasks in the background
  LaunchTasks();
}

void TitleScene::onUpdate(double elapsed)
{
  totalElapsed += elapsed;

  // scroll the bg
  sf::IntRect offset = bgSprite.getTextureRect();
  offset.top += 1;
  bgSprite.setTextureRect(offset);

  // update animations
  anim.Update(elapsed, progSprite.getSprite());

  // If not ready, do no proceed past this point!
  if (IsComplete() == false || progress < 100) {
    ellipsis = (ellipsis + 1) % 4;
    std::string dots = std::string(static_cast<size_t>(ellipsis) + 1, '.');

    if (progress < total) {
      progress++;
    }

    std::string percent = std::to_string(progress) + "%";
    std::string label = taskStr + ": " + percent + dots;
    startLabel.SetString(label);

    Poll();

    return;
  }
  else {
#if defined(__ANDROID__)
    startLabel.SetString("TAP SCREEN");
#else
    startLabel.SetString("PRESS START");
    CenterLabel();
#endif
  }

  if (!checkMods) {
    checkMods = true;

    PlayerPackageManager& pm = getController().PlayerPackagePartitioner().GetPartition(Game::LocalPartition);

    if (pm.Size() == 0) {
      std::filesystem::path path("resources/ow/prog");
      std::string msg = "Looks like you need a Player Mod to continue.\nDownload one and put it under:\n\n`resources/\n mods/\n players/`\nThen re-launch to start playing!";
      sf::Sprite spr;
      spr.setTexture(*Textures().LoadFromFile(path / "prog_mug.png"));
      currMessage = new Message(msg);
      currMessage->ShowEndMessageCursor();
      textbox.EnqueMessage(spr, path / "prog_mug.animation", currMessage);
      textbox.Open();
      Audio().Play(AudioType::CHIP_DESC, AudioPriority::high);
    }
  }

  textbox.Update(elapsed);

  // textbox will prevent players from continuing without mods
  if (textbox.IsOpen()) {
    if (textbox.IsEndOfBlock() && (Input().Has(InputEvents::pressed_pause) || Input().Has(InputEvents::pressed_confirm))) {
      if (!textbox.IsFinalBlock()) {
        textbox.ShowNextLines();
        Audio().Play(AudioType::CHIP_CHOOSE, AudioPriority::high);
      }
    }

    if (textbox.IsFinalBlock()) {
      currMessage->ShowEndMessageCursor(false);
    }

    // return from loop early
    return;
  }

  if (pressedStart) {
    if (fastStartFlicker > frames(0)) {
      fastStartFlicker -= from_seconds(elapsed);
    }
    else {
      // We want the next screen to be the main menu screen
      using tx = segue<DiamondTileCircle>::to<Overworld::Homepage>;
      getController().push<tx>();
      leaving = true;
      return;
    }
  }

  if (Input().Has(InputEvents::pressed_pause) && !pressedStart) {
    pressedStart = true;
    Audio().Play(AudioType::NEW_GAME);
    Audio().StopStream();
  }
}

void TitleScene::onLeave()
{
}

void TitleScene::onExit()
{
}

void TitleScene::onEnter()
{
}

void TitleScene::onResume()
{
  // stream the theme song again
  Audio().Stream("resources/loops/loop_theme.ogg");
}

void TitleScene::onDraw(sf::RenderTexture & surface)
{
  surface.draw(bgSprite);
  surface.draw(progSprite);
  surface.draw(logoSprite);

  // blink when ready
  unsigned min = pressedStart ? 5 : 30;
  unsigned max = pressedStart ? 10 : 60;
  unsigned frame = from_seconds(totalElapsed).count() % max;
  if ((frame < min || progress < 100) && !leaving) {
    sf::Vector2f lastPos = startLabel.getPosition();
    startLabel.setPosition(lastPos.x + 2.f, lastPos.y + 2.f);
    startLabel.SetColor(sf::Color::Black);
    surface.draw(startLabel);
    startLabel.setPosition(lastPos);
    startLabel.SetColor(sf::Color::White);
    surface.draw(startLabel);
  }

  if (textbox.IsOpen()) {
    surface.draw(textbox);
  }
}

void TitleScene::onEnd()
{
}

void TitleScene::onTaskBegin(const std::string & taskName, float progress)
{
  total = unsigned(progress * 100);
  taskStr = taskName;
  startLabel.SetString(this->taskStr + " 00%");
  CenterLabel();

  Logger::Logf(LogLevel::info, "[%.2f] Began task %s", progress, taskName.c_str());
}

void TitleScene::onTaskComplete(const std::string & taskName, float progress)
{
  total = unsigned(progress * 100);
  taskStr = taskName;

  Logger::Logf(LogLevel::info, "[%.2f] Completed task %s", progress, taskName.c_str());
}
