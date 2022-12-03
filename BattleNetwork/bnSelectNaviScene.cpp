
#include <Swoosh/ActivityController.h>
#include <Swoosh/Game.h>

#include "bnPlayerCustScene.h"
#include "bnGameSession.h"
#include "bnSelectNaviScene.h"
#include "Segues/Checkerboard.h"
#include "bnGameUtils.h"
#include <Poco/TextIterator.h>
#include <Poco/UTF8Encoding.h>
#include <Poco/UnicodeConverter.h>

using namespace swoosh::types;

bool SelectNaviScene::IsNaviAllowed()
{
  PlayerPackagePartitioner& partitioner = getController().PlayerPackagePartitioner();
  PackageAddress addr = { Game::LocalPartition, naviSelectionId };
  PackageHash hash = { addr.packageId, partitioner.FindPackageByAddress(addr).GetPackageFingerprint() };

  return getController().Session().IsPackageAllowed(hash);
}

SelectNaviScene::SelectNaviScene(swoosh::ActivityController& controller, std::string& currentNaviId) :
  naviSelectionId(currentNaviId),
  currentChosenId(currentNaviId),
  font(Font::Style::small),
  textbox(70, 10, font),
  naviFont(Font::Style::thick),
  naviLabel("No Data", naviFont),
  hpLabel("1", font),
  attackLabel("1", font),
  speedLabel("1", font),
  menuLabel("1", font),
  owTextbox({4, 255}),
  Scene(controller) {

  greyScaleShader = Shaders().GetShader(ShaderType::GREYSCALE);

  // Menu name font
  menuLabel.setPosition(sf::Vector2f(20.f, 5.0f));
  menuLabel.setScale(2.f, 2.f);

  // Selection input delays
  maxSelectInputCooldown = 0.5; // half of a second
  selectInputCooldown = maxSelectInputCooldown;

  // NAVI UI font
  naviLabel.setPosition(30.f, 22.f);
  naviLabel.setScale(2.0f, 2.0f);

  attackLabel.setPosition(335.f, 24.f);
  attackLabel.setScale(2.0f, 2.0f);

  speedLabel.setPosition(335.f, 78.f);
  speedLabel.setScale(2.0f, 2.0f);

  hpLabel.setPosition(335.f, 132.0f);
  hpLabel.setScale(2.0f, 2.0f);

  maxNumberCooldown = 0.5;
  numberCooldown = maxNumberCooldown; // half a second

  prevChosenId = currentNaviId;
  // select menu graphic
  bg = new GridBackground();

  // UI Sprites
  UI_RIGHT_POS = UI_RIGHT_POS_START;
  UI_LEFT_POS = UI_LEFT_POS_START;
  UI_TOP_POS = UI_TOP_POS_START;

  charName.setTexture(Textures().LoadFromFile(TexturePaths::CHAR_NAME));
  charName.setScale(2.f, 2.f);
  charName.setPosition(UI_LEFT_POS, 10);

  charElement.setTexture(Textures().LoadFromFile(TexturePaths::CHAR_ELEMENT));
  charElement.setScale(2.f, 2.f);
  charElement.setPosition(UI_LEFT_POS, 80);

  charStat.setTexture(Textures().LoadFromFile(TexturePaths::CHAR_STAT));
  charStat.setScale(2.f, 2.f);
  charStat.setPosition(UI_RIGHT_POS, UI_TOP_POS);

  charInfo.setTexture(Textures().LoadFromFile(TexturePaths::CHAR_INFO_BOX));
  charInfo.setScale(2.f, 2.f);
  charInfo.setPosition(UI_RIGHT_POS, 170);

  element.setTexture(Textures().LoadFromFile(TexturePaths::ELEMENT_ICON));
  element.setScale(2.f, 2.f);
  element.setPosition(UI_LEFT_POS_MAX + 15.f, 90);

  // Current navi graphic
  loadNavi = false;
  navi.setScale(2.f, 2.f);
  navi.setOrigin(navi.getLocalBounds().width / 2.f, navi.getLocalBounds().height / 2.f);
  navi.setPosition(100.f, 150.f);

  auto& playerPkg = getController().PlayerPackagePartitioner().GetPartition(Game::LocalPartition).FindPackageByID(currentChosenId);
  if (auto tex = playerPkg.GetPreviewTexture()) {
    navi.setTexture(tex);
  }

  naviLabel.SetString(sf::String(playerPkg.GetName().c_str()));
  speedLabel.SetString(sf::String(playerPkg.GetSpeedString().c_str()));
  attackLabel.SetString(sf::String(playerPkg.GetAttackString().c_str()));
  hpLabel.SetString(sf::String(playerPkg.GetHPString().c_str()));

  // Distortion effect
  factor = MAX_PIXEL_FACTOR;

  gotoNextScene = true;

  // Text box
  textbox.SetCharactersPerSecond(15);
  textbox.setPosition(UI_RIGHT_POS_MAX + 10, 205);
  textbox.setScale(2.f, 2.f);
  textbox.Stop();
  // textbox.
  textbox.Mute(); // no tick sound

  elapsed = 0;

  setView(sf::Vector2u(480, 320));

  if (!IsNaviAllowed()) {
    owTextbox.EnqueueMessage("This navi is not allowed on this server.");
  }
}

SelectNaviScene::~SelectNaviScene()
{
  delete bg;
}

void SelectNaviScene::onDraw(IRenderer& renderer) {
  renderer.submit(bg);

  // Navi preview shadow
  const sf::Vector2f originalPosition = navi.getPosition();
  const sf::Color originalColor = navi.getColor();

  // Make the shadow begin on the other side of the window by an arbitrary offset
  navi.setPosition(-20.0f + getController().getVirtualWindowSize().x - navi.getPosition().x, navi.getPosition().y);
  navi.setColor(sf::Color::Black);
  // TODO: remove Clone()
  renderer.submit(&navi);

  // End 'hack' by restoring original position and color values
  navi.setPosition(originalPosition);
  navi.setColor(originalColor);

  charName.setPosition(UI_LEFT_POS, charName.getPosition().y);
  renderer.submit(&charName);

  charElement.setPosition(UI_LEFT_POS, charElement.getPosition().y);
  renderer.submit(&charElement);

  // Draw stat box three times for three diff. properties
  float charStat1Max = 10;

  if (UI_TOP_POS < charStat1Max) {
    charStat.setPosition(UI_RIGHT_POS, charStat1Max);
  }
  else {
    charStat.setPosition(UI_RIGHT_POS, UI_TOP_POS);
  }
  // TODO: remove Clone()
  renderer.submit(&charStat);

  // 2nd stat box
  float charStat2Max = 10 + UI_SPACING;

  if (UI_TOP_POS < charStat2Max) {
    charStat.setPosition(UI_RIGHT_POS, charStat2Max);
  }
  else {
    charStat.setPosition(UI_RIGHT_POS, UI_TOP_POS);
  }
  // TODO: remove Clone()
  renderer.submit(&charStat);

  // 3rd stat box
  float charStat3Max = 10 + (UI_SPACING * 2);

  if (UI_TOP_POS < charStat3Max) {
    charStat.setPosition(UI_RIGHT_POS, charStat3Max);
  }
  else {
    charStat.setPosition(UI_RIGHT_POS, UI_TOP_POS);
  }
  // TODO: remove Clone()
  renderer.submit(&charStat);

  // SP. Info box
  charInfo.setPosition(UI_RIGHT_POS, charInfo.getPosition().y);
  renderer.submit(&charInfo);

  // Update UI slide in
  if (!gotoNextScene) {
    factor -= elapsed * 280.0;

    if (factor <= 0.f) {
      factor = 0.f;
    }

    if (UI_RIGHT_POS > UI_RIGHT_POS_MAX) {
      UI_RIGHT_POS -= (float)elapsed * 1000;
    }
    else {
      UI_RIGHT_POS = UI_RIGHT_POS_MAX;
      UI_TOP_POS -= (float)elapsed * 1000;

      if (UI_TOP_POS < UI_TOP_POS_MAX) {
        UI_TOP_POS = UI_TOP_POS_MAX;

        // Draw labels
        renderer.submit(&naviLabel);
        renderer.submit(&hpLabel);
        renderer.submit(&speedLabel);
        renderer.submit(&attackLabel);
        renderer.submit(&textbox);
        renderer.submit(&element);

        textbox.Play();
      }
    }

    if (UI_LEFT_POS < UI_LEFT_POS_MAX) {
      UI_LEFT_POS += (float)elapsed * 1000;
    }
    else {
      UI_LEFT_POS = UI_LEFT_POS_MAX;
    }
  }
  else {
    factor += (float)elapsed * 320.f;

    if (factor >= MAX_PIXEL_FACTOR) {
      factor = MAX_PIXEL_FACTOR;
    }

    if (UI_TOP_POS < UI_TOP_POS_START) {
      UI_TOP_POS += (float)elapsed * 1000;
    }
    else {
      UI_RIGHT_POS += (float)elapsed * 1000;

      if (UI_RIGHT_POS > UI_RIGHT_POS_START / 2) // Be quicker at leave than startup
        UI_LEFT_POS -= (float)elapsed * 1000;
    }
  }

  sf::RenderStates states;

  if (!IsNaviAllowed()) {
    states.shader = greyScaleShader;
  }

  renderer.submit(&navi, states);
  renderer.submit(&owTextbox);
}

void SelectNaviScene::onStart()
{
  gotoNextScene = false;
}

void SelectNaviScene::onLeave()
{
}

void SelectNaviScene::onExit()
{
}

void SelectNaviScene::onEnter()
{
}

void SelectNaviScene::onResume()
{
  gotoNextScene = false;
}

void SelectNaviScene::onEnd()
{
}

void SelectNaviScene::GotoPlayerCust()
{
  gotoNextScene = true;
  Audio().Play(AudioType::CHIP_DESC);

  GameUtils(getController()).LaunchPlayerCust(this->currentChosenId);
}

void SelectNaviScene::onUpdate(double elapsed) {
  SelectNaviScene::elapsed = elapsed;

  textbox.Update((float)elapsed);
  owTextbox.Update((float)elapsed);
  bg->Update((float)elapsed);

  std::string prevSelectId = currentChosenId;
  PlayerPackageManager& packageManager = getController().PlayerPackagePartitioner().GetPartition(Game::LocalPartition);

  bool openTextbox = owTextbox.IsOpen();

  // Scene keyboard controls
  if (!gotoNextScene) {
    if (openTextbox) {
      owTextbox.HandleInput(Input(), {});
    }
    else if (Input().Has(InputEvents::pressed_ui_left)) {
      selectInputCooldown -= elapsed;

      if (selectInputCooldown <= 0) {
        // Go to previous mob
        selectInputCooldown = maxSelectInputCooldown;
        currentChosenId = packageManager.GetPackageBefore(currentChosenId);

        // Number scramble effect
        numberCooldown = maxNumberCooldown;
      }
    }
    else if (Input().Has(InputEvents::pressed_ui_right)) {
      selectInputCooldown -= elapsed;

      if (selectInputCooldown <= 0) {
        // Go to next mob
        selectInputCooldown = maxSelectInputCooldown;
        currentChosenId = packageManager.GetPackageAfter(currentChosenId);

        // Number scramble effect
        numberCooldown = maxNumberCooldown;
      }
    }
    else {
      selectInputCooldown = 0;
    }

    if (Input().Has(InputEvents::pressed_cancel) && !openTextbox) {
      if (IsNaviAllowed()) {
        gotoNextScene = true;
        Audio().Play(AudioType::CHIP_DESC_CLOSE);
        textbox.Mute();

        getController().pop<segue<Checkerboard, milliseconds<500>>>();
        return;
      }
      // else explain why
      owTextbox.EnqueueMessage("Please select a different navi.");
    }
  }

  auto& playerPkg = packageManager.FindPackageByID(currentChosenId);

  // Reset the factor/slide in effects if a new selection was made
  if (currentChosenId != prevSelectId || !loadNavi) {
    factor = 125;

    int offset = (int)(playerPkg.GetElement());
    auto iconRect = sf::IntRect(14 * offset, 0, 14, 14);
    element.setTextureRect(iconRect);

    if (auto tex = playerPkg.GetPreviewTexture()) {
      navi.setTexture(tex, true);
    }

    textbox.SetText(playerPkg.GetSpecialDescriptionString());
    loadNavi = true;
  }

  // This goes here because the jumbling effect may finish and we need to see proper values
  naviLabel.SetString(sf::String(playerPkg.GetName()));
  speedLabel.SetString(sf::String(playerPkg.GetSpeedString()));
  attackLabel.SetString(sf::String(playerPkg.GetAttackString()));
  hpLabel.SetString(sf::String(playerPkg.GetHPString()));

  // This just scrambles the letters
  if (numberCooldown > 0) {
    numberCooldown -= (float)elapsed;
    std::string newstr;

    Poco::UTF8Encoding utf8Encoding;
    Poco::TextIterator it(naviLabel.GetString(), utf8Encoding);
    Poco::TextIterator end(naviLabel.GetString());
    size_t i = 0;
    size_t length = Poco::UnicodeConverter::UTFStrlen(naviLabel.GetString().c_str());
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

    int randAttack = rand() % 10;
    int randSpeed = rand() % 10;

    //attackLabel.SetString(std::to_string(randAttack));
    //speedLabel.SetString(std::to_string(randSpeed));
    naviLabel.SetString(sf::String(newstr));
  }

  float progress = (maxNumberCooldown - numberCooldown) / maxNumberCooldown;

  if (progress > 1.f) progress = 1.f;

  // Darken the unselected navis
  if (prevChosenId != currentChosenId) {
    navi.setColor(sf::Color(200, 200, 200, 188));
  }
  else {

    // Make selected navis full color
    navi.setColor(sf::Color(255, 255, 255, 255));
  }

  // transform this value into a 0 -> 1 range
  float range = (125.f - (float)factor) / 125.f;

  if (factor != 0.f) {
    navi.setColor(sf::Color(255, 255, 255, (sf::Uint8)(navi.getColor().a * range)));
  }

  // Refresh mob graphic origin every frame as it may change
  auto size = getController().getVirtualWindowSize();

  navi.setPosition(range*float(size.x)*0.425f, float(size.y));
  navi.setOrigin(float(navi.getTextureRect().width)*0.5f, float(navi.getTextureRect().height));

  // Make a selection
  if (Input().Has(InputEvents::pressed_confirm) && !openTextbox) {
    if (currentChosenId != naviSelectionId) {
      Audio().Play(AudioType::CHIP_CONFIRM, AudioPriority::low);
      prevChosenId = currentChosenId;
      naviSelectionId = currentChosenId;
      getController().Session().SetKeyValue("SelectedNavi", naviSelectionId);
    }
    else if(owTextbox.IsClosed()) {
      owTextbox.EnqueueQuestion("Open Navi Cust?", [this](bool result) {
        if (result) {
          this->GotoPlayerCust();
        }
      });
    }
  }
}
