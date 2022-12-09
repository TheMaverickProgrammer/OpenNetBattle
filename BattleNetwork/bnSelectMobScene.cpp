#include <Swoosh/ActivityController.h>
#include "bnSelectMobScene.h"
#include "bnMobPackageManager.h"
#include "bnGameSession.h"
#include "battlescene/bnMobBattleScene.h"
#include "battlescene/bnFreedomMissionMobScene.h"
#include "Android/bnTouchArea.h"
#include "../../bnBlockPackageManager.h"
#include "../../bnPlayerCustScene.h"

#include <Poco/TextIterator.h>
#include <Poco/UTF8Encoding.h>
#include <Poco/UnicodeConverter.h>

constexpr float PIXEL_MAX = 50.0f;
constexpr float PIXEL_SPEED = 180.0f;

using namespace swoosh::types;

SelectMobScene::SelectMobScene(swoosh::ActivityController& controller, SelectMobScene::Properties props) :
  elapsed(0),
  selectedFolder(std::move(props.folder)),
  selectedNaviId(props.naviId),
  programAdvance(props.pa),
  defaultBackground(props.background),
  font(Font::Style::wide),
  menuLabel("BATTLE SELECT", font),
  uiFont(Font::Style::thick),
  mobLabel("No Data", uiFont),
  attackLabel("1", uiFont),
  speedLabel("1", uiFont),
  hpLabel("1", uiFont),
  textbox(180, 22),
  Scene(controller)
{
  // Menu name font
  menuLabel.setPosition(sf::Vector2f(20.f, 8.0f));
  menuLabel.setScale(2.f, 2.f);

  navigator.setTexture(Textures().LoadFromFile(TexturePaths::MUG_NAVIGATOR));
  navigator.setScale(2.0f, 2.0f);
  navigator.setPosition(10.0f, 208.0f);

  navigatorAnimator = Animation("resources/ui/navigator.animation");
  navigatorAnimator.Reload();
  navigatorAnimator.SetAnimation("TALK");
  navigatorAnimator << Animator::Mode::Loop;

  cursor.setTexture(Textures().LoadFromFile(TexturePaths::FOLDER_CURSOR));
  cursor.setScale(2.f, 2.f);

  // Selection input delays
  maxSelectInputCooldown = 0.5; // half of a second
  selectInputCooldown = maxSelectInputCooldown;

  // MOB UI font
  mobLabel.setScale(2.f, 2.f);
  attackLabel.setScale(2.f, 2.f);
  speedLabel.setScale(2.f, 2.f);
  hpLabel.setScale(2.f, 2.f);

  maxNumberCooldown = 0.5;
  numberCooldown = maxNumberCooldown; // half a second

  // select menu graphic
  bg.setTexture(Textures().LoadFromFile(TexturePaths::BATTLE_SELECT_BG));
  bg.setScale(2.f, 2.f);

  gotoNextScene = true;
  doOnce = false; // wait until the scene starts or resumes
  showMob = false;

  factor = PIXEL_MAX;
  shader = Shaders().GetShader(ShaderType::TEXEL_PIXEL_BLUR);

  // Current selection index
  mobSelectionId = getController().MobPackagePartitioner().GetPartition(Game::LocalPartition).FirstValidPackage();

  // Text box navigator
  textbox.Stop();
  textbox.setPosition(100, 205);
  textbox.setScale(2.f, 2.f);
  textbox.SetTextColor(sf::Color::Black);

#ifdef __ANDROID__
  touchPosX = 0;
  canSwipe = false;
#endif

  setView(sf::Vector2u(480, 320));
}

SelectMobScene::~SelectMobScene() {
}

void SelectMobScene::onUpdate(double elapsed) {
  SelectMobScene::elapsed += elapsed;

  navigatorAnimator.Update(elapsed, navigator.getSprite());

  textbox.Update((float)elapsed);

  std::string prevSelectId = mobSelectionId;

#ifndef __ANDROID__
  // Scene keyboard controls
  if (!gotoNextScene) {
    if (Input().Has(InputEvents::pressed_ui_left)) {
      selectInputCooldown -= elapsed;

      if (selectInputCooldown <= 0) {
        // Go to previous mob
        selectInputCooldown = maxSelectInputCooldown;
        mobSelectionId = getController().MobPackagePartitioner().GetPartition(Game::LocalPartition).GetPackageBefore(mobSelectionId);

        // Number scramble effect
        numberCooldown = maxNumberCooldown;
      }
    }
    else if (Input().Has(InputEvents::pressed_ui_right)) {
      selectInputCooldown -= elapsed;

      if (selectInputCooldown <= 0) {
        // Go to next mob
        selectInputCooldown = maxSelectInputCooldown;
        mobSelectionId = getController().MobPackagePartitioner().GetPartition(Game::LocalPartition).GetPackageAfter(mobSelectionId);

        // Number scramble effect
        numberCooldown = maxNumberCooldown;
      }
    }
    else {
      selectInputCooldown = 0;
    }

    if (Input().Has(InputEvents::pressed_cancel)) {
      // Fade out black and go back to the menu
      gotoNextScene = true;
      Audio().Play(AudioType::CHIP_DESC_CLOSE);

      using effect = segue<BlackWashFade, milliseconds<500>>;
      getController().pop<effect>();
    }
  }
#else
    // Scene keyboard controls
    if (!gotoNextScene) {
        if ((touchPosX - touchPosStartX) < -100 && canSwipe) {
            canSwipe = false;

            // Go to previous mob
            selectInputCooldown = maxSelectInputCooldown;
            mobSelectionIndex++;

            // Number scramble effect
            numberCooldown = maxNumberCooldown;
        }
        else if ((touchPosStartX - touchPosX) < -100 && canSwipe) {
            canSwipe = false;

            // Go to next mob
            selectInputCooldown = maxSelectInputCooldown;
            mobSelectionIndex--;

            // Number scramble effect
            numberCooldown = maxNumberCooldown;
        }

        if (Input().Has(PRESSED_B)) {
            // Fade out black and go back to the menu
            gotoNextScene = true;
            Audio().Play(AudioType::CHIP_DESC_CLOSE);
            using segue = swoosh::intent::segue<BlackWashFade, swoosh::intent::milli<500>>;
            getController().pop<segue>();
        }
    }
#endif

  // Grab the mob info object from this index
  auto& mobinfo = getController().MobPackagePartitioner().GetPartition(Game::LocalPartition).FindPackageByID(mobSelectionId);

  mobLabel.SetString(mobinfo.GetName());
  hpLabel.SetString(mobinfo.GetHPString());
  speedLabel.SetString(mobinfo.GetSpeedString());
  attackLabel.SetString(mobinfo.GetAttackString());

#ifdef __ANDROID__
  if(canSwipe) {
      if (sf::Touch::isDown(0)) {
          sf::Vector2i touchPosition = sf::Touch::getPosition(0, *ENGINE.GetWindow());
          sf::Vector2f coords = ENGINE.GetWindow()->mapPixelToCoords(touchPosition,
                                                                     ENGINE.GetDefaultView());
          sf::Vector2i iCoords = sf::Vector2i((int) coords.x, (int) coords.y);
          touchPosition = iCoords;

          canSwipe = false;

          if(touchPosition.y < 400 && touchPosition.x < 290) {
            if (!touchStart) {
              touchStart = true;
              touchPosStartX = touchPosition.x;
            }

            touchPosX = touchPosition.x;
          }

          if(touchPosition.x <= 320) {
            mobSpr.setPosition(110.0f - (touchPosStartX - touchPosX), 130.f);
            canSwipe = true;
          }
      } else {
          canSwipe = false;
          touchStart = false;
      }
  } else {
      if(prevSelect == mobSelectionIndex) {
          auto x = swoosh::ease::interpolate(0.5f, mobSpr.getPosition().x, 110.f);
          auto y = swoosh::ease::interpolate(0.5f, mobSpr.getPosition().y, 130.f);
          mobSpr.setPosition(x, y);

          if (int(x) == 110 && int(y) == 130) {
              mobSpr.setPosition(110.f, 130.f);
              touchPosX = 50; // somewhere in the middle that wont trigger a swipe
              touchPosStartX = 50;
              touchStart = false;
              canSwipe = true;
          }
      }
  }
#else
  mobSpr.setPosition(110.0f, 130.f);
#endif

  if (prevSelectId != mobSelectionId || doOnce) {
    doOnce = false;
    factor = PIXEL_MAX;

    // Current mob graphic
    mobSpr.setTexture(mobinfo.GetPlaceholderTexture());
    mobSpr.setScale(2.f, 2.f);
    mobSpr.setOrigin(mobSpr.getLocalBounds().width / 2.f, mobSpr.getLocalBounds().height / 2.f);

    textbox.SetText(mobinfo.GetDescriptionString());
    textbox.Stop();
  }

  /**
   * The following code just scrambles the name, health, speed, and attack data
   * Each line does the same thing
   * If it's a number, choose random numbers
   * If it's alphabetical, choose random capital letters
   * The scramble index moves towards the end of the string over time
   * Eventually the original data is unmodified and the effect ends
   */
  if (numberCooldown > 0) {
    numberCooldown -= (float)elapsed;
    std::string newstr;

    Poco::UTF8Encoding utf8Encoding;
    Poco::TextIterator it(mobLabel.GetString(), utf8Encoding);
    Poco::TextIterator end(mobLabel.GetString());
    size_t i = 0;
    size_t length = Poco::UnicodeConverter::UTFStrlen(mobLabel.GetString().c_str());
    for (; it != end; ++it) {
      double progress = (maxNumberCooldown - numberCooldown) / maxNumberCooldown;
      double index = progress * length;

      if (i < (int)index) {
        std::string utf8string;
        Poco::UnicodeConverter::convert(Poco::UTF32String(1, *it), utf8string);
        newstr += utf8string;
      }
      else {
        if (*it != U' ') {
          newstr += (char)(((rand() % (90 - 65)) + 65) + 1);
        }
        else {
          newstr += ' ';
        }
      }
      ++i;
    }

    int randAttack = 0;
    int randSpeed = 0;
    int randHP = 0;

    int count = (int)mobinfo.GetHPString().size() - 1;
    while (count >= 0) {
      int index = (int)std::pow(10.0, (double)count);
      index *= (rand() % 9) + 1;

      randHP += index;

      count--;
    }

    count = (int)mobinfo.GetAttackString().size() - 1;

    while (count >= 0) {
      int index = (int)std::pow(10.0, (double)count);
      index *= (rand() % 9) + 1;

      randAttack += index;

      count--;
    }

    count = (int)mobinfo.GetSpeedString().size() - 1;

    while (count >= 0) {
      int index = (int)std::pow(10.0, (double)count);
      index *= (rand() % 9) + 1;

      randSpeed += index;

      count--;
    }

    attackLabel.SetString(std::to_string(randAttack));
    speedLabel.SetString(std::to_string(randSpeed));
    hpLabel.SetString(std::to_string(randHP));
    mobLabel.SetString(sf::String(newstr));
  }


  /**
   * End scramble effect
   */

  factor -= (float)elapsed * PIXEL_SPEED;

  if (factor <= 0.f) {
    factor = 0.f;
  }

  // Progress for data scramble effect
  float progress = (maxNumberCooldown - numberCooldown) / maxNumberCooldown;

  if (progress > 1.f) {
    progress = 1.f;

  if(!gotoNextScene) {
      textbox.Play();
    }
  }

  // Make a selection
  if (Input().Has(InputEvents::pressed_confirm) && !gotoNextScene) {
    Mob* mob = nullptr;

    MobPackageManager& packageManager = getController().MobPackagePartitioner().GetPartition(Game::LocalPartition);
    if (packageManager.Size() != 0) {
      try {
        auto mobFactory = packageManager.FindPackageByID(mobSelectionId).GetData();
        mob = mobFactory->Build(std::make_shared<Field>(6,3));
        delete mobFactory;
      }
      catch (...) {
        mob = nullptr;
      }
    }

    if (!mob) {
      // Play error buzzer if the mob index is out of range or
      // data is invalid
      gotoNextScene = false;

      Audio().Play(AudioType::CHIP_ERROR, AudioPriority::lowest);
    }
    else {
      gotoNextScene = true;

      // Play the pre battle rumble sound
      Audio().Play(AudioType::PRE_BATTLE, AudioPriority::high);

      // Stop music and go to battle screen
      Audio().StopStream();


      // Get the navi we selected
      auto& meta = getController().PlayerPackagePartitioner().GetPartition(Game::LocalPartition).FindPackageByID(selectedNaviId);
      const std::filesystem::path& image = meta.GetMugshotTexturePath();
      const std::filesystem::path& mugshotAnim = meta.GetMugshotAnimationPath();
      const std::filesystem::path& emotionsTexture = meta.GetEmotionsTexturePath();
      auto mugshot = Textures().LoadFromFile(image);
      auto emotions = Textures().LoadFromFile(emotionsTexture);
      auto player = std::shared_ptr<Player>(meta.GetData());

      BlockPackageManager& blockPackages = getController().BlockPackagePartitioner().GetPartition(Game::LocalPartition);
      GameSession& session = getController().Session();

      // Get the package ID from the address since we know we're only using local packages
      std::vector<PackageAddress> localNaviBlocksAddr = PlayerCustScene::GetInstalledBlocks(selectedNaviId, session);
      std::vector<std::string> localNaviBlocks;

      for (const PackageAddress& addr : localNaviBlocksAddr) {
        localNaviBlocks.push_back(addr.packageId);
      }

      // Shuffle our new folder
      auto newFolder = selectedFolder->Clone();
      newFolder->Shuffle();

      if (!mob->GetBackground()) {
        mob->SetBackground(defaultBackground);
      }

      using effect = segue<WhiteWashFade>;

      // Queue screen transition to Battle Scene with a white fade effect
      // just like the game
      if (mob->IsFreedomMission()) {
        FreedomMissionProps props{
          { player, programAdvance, std::move(newFolder), mob->GetField(), mob->GetBackground() },
          { mob },
          mob->GetTurnLimit(),
          sf::Sprite(*mugshot),
          mugshotAnim,
          emotions,
          localNaviBlocks
        };

        getController().push<effect::to<FreedomMissionMobScene>>(std::move(props));
      }
      else {
        MobBattleProperties props{
          { player, programAdvance, std::move(newFolder), mob->GetField(), mob->GetBackground() },
          MobBattleProperties::RewardBehavior::take,
          { mob },
          sf::Sprite(*mugshot),
          mugshotAnim,
          emotions,
          localNaviBlocks
        };

        getController().push<effect::to<MobBattleScene>>(std::move(props));
      }
    }
  }

  // Close mouth when there's a space.
  // Overload boolean logic to close mouth whenever the text box is also paused
  bool isDone = !textbox.IsPlaying() || textbox.GetCurrentCharacter() == '\0';
  isDone = isDone || textbox.IsEndOfMessage();

  if (isDone && navigatorAnimator.GetAnimationString() != "IDLE") {
    navigatorAnimator.SetAnimation("IDLE");
    navigatorAnimator << Animator::Mode::Loop;
  }
  else if(!isDone && navigatorAnimator.GetAnimationString() != "TALK") {
    navigatorAnimator.SetAnimation("TALK");
    navigatorAnimator << Animator::Mode::Loop;
  }
}

void SelectMobScene::onDraw(IRenderer& renderer) {
  renderer.submit(&bg);
  renderer.submit(&menuLabel);

  // Draw mob name with shadow
  mobLabel.setPosition(sf::Vector2f(30.f, 38.0f));
  mobLabel.SetColor(sf::Color(138, 138, 138));
  renderer.submit(UI{ &mobLabel });
  mobLabel.setPosition(sf::Vector2f(30.f, 36.0f));
  mobLabel.SetColor(sf::Color::White);
  renderer.submit(UI{ &mobLabel });

  // Draw attack rating with shadow
  attackLabel.setPosition(382.f, 88.f);
  attackLabel.SetColor(sf::Color(88, 88, 88));
  renderer.submit(UI{ &attackLabel });
  attackLabel.setPosition(380.f, 86.f);
  attackLabel.SetColor(sf::Color::White);
  renderer.submit(UI{ &attackLabel });

  // Draw speed rating with shadow
  speedLabel.setPosition(382.f, 120.f);
  speedLabel.SetColor(sf::Color(88, 88, 88));
  renderer.submit(UI{ &speedLabel });
  speedLabel.setPosition(380.f, 118.f);
  speedLabel.SetColor(sf::Color::White);
  renderer.submit(UI{ &speedLabel });

  // Draw hp
  hpLabel.setPosition(382.f, 152.f);
  hpLabel.SetColor(sf::Color(88, 88, 88));
  renderer.submit(UI{ &hpLabel });
  hpLabel.setPosition(380.f, 150.f);
  hpLabel.SetColor(sf::Color::White);
  renderer.submit(UI{ &hpLabel });

  // Pixelate the mob texture
  if (mobSpr.getTexture()) {
    sf::IntRect t = mobSpr.getTextureRect();
    sf::Vector2u size = mobSpr.getTexture()->getSize();
    shader.SetUniform("x", (float)t.left / (float)size.x);
    shader.SetUniform("y", (float)t.top / (float)size.y);
    shader.SetUniform("w", (float)t.width / (float)size.x);
    shader.SetUniform("h", (float)t.height / (float)size.y);
    shader.SetUniform("pixel_threshold", (float)(factor / PIXEL_MAX));

    // Refresh mob graphic origin every frame as it may change
    float scale = (PIXEL_MAX-factor) / PIXEL_MAX;
    scale *= 2.f;

    mobSpr.setOrigin(mobSpr.getTextureRect().width / 2.f, mobSpr.getTextureRect().height / 2.f);
    mobSpr.setScale(scale, scale);
    mobSpr.SetShader(shader);
    renderer.submit(UI{ &mobSpr });
  }

  renderer.submit(UI{ &textbox });
  renderer.submit(UI{ &navigator });

  // Draw the LEFT cursor
  // if (mobSelectionIndex > 0) {
    cursor.setColor(sf::Color::White);
  // }
  // else {
    // If there are no more items to the left, fade the cursor
  //  cursor.setColor(sf::Color(255, 255, 255, 100));
  //}

  // Add sine offset to create a bob effect
  auto offset = std::sin(elapsed*10.0) * 5;

  // Put the left cursor on the left of the mob
  cursor.setPosition(23.0f + (float)offset, 130.0f);

  // Flip the x axis
  cursor.setScale(-2.f, 2.f);

  // Draw left cursor
  renderer.submit(UI{ &cursor });

  // Draw the RIGHT cursor
  // if (mobSelectionIndex < (int)(getController().MobPackageManager().Size() - 1)) {
    cursor.setColor(sf::Color::White);
  // } else {
    // If there are no more items to the right, fade the cursor
    // cursor.setColor(sf::Color(255, 255, 255, 100));
  // }

  // Add sine offset to create a bob effect
  offset = -std::sin(elapsed*10.0) * 5;

  // Put the right cursor on the right of the mob
  cursor.setPosition(200.0f + (float)offset, 130.0f);

  // Flip the x axis
  cursor.setScale(2.f, 2.f);

  // Draw the right cursor
  renderer.submit(UI{ &cursor });
}

void SelectMobScene::onResume() {
  // Re-play music
  Audio().Stream("resources/loops/loop_navi_customizer.ogg", true);

  gotoNextScene = false;
  Logger::Log(LogLevel::info, "SelectMobScene::onResume()");

#ifdef __ANDROID__
  StartupTouchControls();
#endif
}

void SelectMobScene::onStart() {
  textbox.Play();
  doOnce = true;
  showMob = true;
  gotoNextScene = false;
  factor = PIXEL_MAX;

#ifdef __ANDROID__
  StartupTouchControls();
#endif

  Logger::Log(LogLevel::info, "SelectMobScene::onStart()");
}

void SelectMobScene::onLeave() {
  textbox.Stop();

#ifdef __ANDROID__
  ShutdownTouchControls();
#endif

  Logger::Log(LogLevel::info, "SelectMobScene::onLeave()");

}

void SelectMobScene::onExit() {
  Logger::Log(LogLevel::info, "SelectMobScene::onExit()");
}

void SelectMobScene::onEnter() {
  Logger::Log(LogLevel::info, "SelectMobScene::onEnter()");
}

void SelectMobScene::onEnd() {
#ifdef __ANDROID__
  ShutdownTouchControls();
#endif
  Logger::Log(LogLevel::info, "SelectMobScene::onEnd()");

}

#ifdef __ANDROID__
void SelectMobScene::StartupTouchControls() {
  /* Android touch areas*/
  TouchArea& rightSide = TouchArea::create(sf::IntRect(240, 0, 240, 320));

  releasedB = false;

  rightSide.enableExtendedRelease(true);

  rightSide.onTouch([]() {
      INPUTx.VirtualKeyEvent(InputEvent::RELEASED_A);
  });

  rightSide.onRelease([this](sf::Vector2i delta) {
      if(!releasedB) {
        INPUTx.VirtualKeyEvent(InputEvent::PRESSED_A);
      }
  });

  rightSide.onDrag([this](sf::Vector2i delta){
      if(delta.x < -25 && !releasedB) {
        INPUTx.VirtualKeyEvent(InputEvent::PRESSED_B);
        INPUTx.VirtualKeyEvent(InputEvent::RELEASED_B);
        releasedB = true;
      }
  });
}

void SelectMobScene::ShutdownTouchControls() {
  TouchArea::free();
}
#endif
