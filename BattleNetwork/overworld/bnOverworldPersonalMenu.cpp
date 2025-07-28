#include "bnOverworldPersonalMenu.h"
#include <Swoosh/Ease.h>
#include "../bnDrawWindow.h"
#include "../bnTextureResourceManager.h"
#include "../bnAudioResourceManager.h"

namespace Overworld {
  PersonalMenu::PersonalMenu(const std::shared_ptr<PlayerSession>& session, const std::string& area, const PersonalMenu::OptionsList& options) :
    session(session),
    optionsList(options),
    infoText(Font::Style::thin),
    areaLabel(Font::Style::thin),
    areaLabelThick(Font::Style::thick),
    time(Font::Style::thick)
  {
    // Load resources
    areaLabel.setPosition(127, 119);
    infoText = areaLabel;

    // clock
    time.setPosition(480 - 4.f, 6.f);
    time.setScale(2.f, 2.f);

    widgetTexture = Textures().LoadFromFile("resources/ui/main_menu_ui.png");

    banner = std::make_shared<SpriteProxyNode>();
    symbol = std::make_shared<SpriteProxyNode>();
    icon = std::make_shared<SpriteProxyNode>();
    exit = std::make_shared<SpriteProxyNode>();
    infoBox = std::make_shared<SpriteProxyNode>();
    selectTextSpr = std::make_shared<SpriteProxyNode>();
    placeTextSpr = std::make_shared<SpriteProxyNode>();

    banner->setTexture(Textures().LoadFromFile("resources/ui/menu_overlay.png"));
    symbol->setTexture(widgetTexture);
    icon->setTexture(widgetTexture);
    exit->setTexture(widgetTexture);
    infoBox->setTexture(widgetTexture);
    selectTextSpr->setTexture(widgetTexture);
    placeTextSpr->setTexture(widgetTexture);

    AddNode(banner);

    infoBoxAnim = Animation("resources/ui/main_menu_ui.animation");
    infoBoxAnim << "INFO";
    infoBoxAnim.SetFrame(1, infoBox->getSprite());
    AddNode(infoBox);
    infoBox->setPosition(180, 52);

    optionAnim = Animation("resources/ui/main_menu_ui.animation");
    optionAnim << "SYMBOL";
    optionAnim.SetFrame(1, symbol->getSprite());
    AddNode(symbol);
    symbol->setPosition(20, 1);

    optionAnim << "SELECT";
    optionAnim.SetFrame(1, selectTextSpr->getSprite());
    AddNode(selectTextSpr);
    selectTextSpr->setPosition(4, 18);

    optionAnim << "PLACE";
    optionAnim.SetFrame(1, placeTextSpr->getSprite());
    AddNode(placeTextSpr);
    placeTextSpr->setPosition(120, 111);

    optionAnim << "EXIT";
    optionAnim.SetFrame(1, exit->getSprite());
    exit->setPosition(240, 144);
    exit->Hide();
    AddNode(exit);

    // Device / HP top-left position
    optionAnim << "PET";
    optionAnim.SetFrame(1, icon->getSprite());
    icon->setPosition(2, 3);
    healthUI.setPosition(2, 3);
    healthUI.setScale(0.5, 0.5);

    SetPlayerDisplay(PlayerDisplay::PlayerHealth);

    exitAnim = Animation("resources/ui/main_menu_ui.animation") << Animator::Mode::Loop;

    //
    // Load options
    //

    CreateOptions();

    // Set name
    SetArea(area);

    maxSelectInputCooldown = 0.5; // half of a second
    selectInputCooldown = maxSelectInputCooldown;
  }

  PersonalMenu::~PersonalMenu()
  {
  }

  using namespace swoosh;

  /**
  8 frames to slide in
  - frame 8 first folder appears
  - "PLACE" name starts to appear one letter at a time
  - Exit begins to slide in

  @ 10 frames
  "place" name scrolls out

  @ 14 frames
  - exit is in place
  - the screen is fully dark
  - right-hand window with HP and zenny begins to open up

  @ 16 frames
  - right-hand info window is fully open

  @ 20 frames
  - all the folder options have expanded
  - ease in animation is complete
  */
  void PersonalMenu::QueueAnimTasks(const PersonalMenu::state& state)
  {
    easeInTimer.clear();

    if (state == PersonalMenu::state::opening) {
      easeInTimer.reverse(false);
      easeInTimer.set(frames(0));
    }
    else {
      easeInTimer.reverse(true);
      easeInTimer.set(frames(21));
    }

    //
    // Start these task at the beginning
    //

    auto& t0f = easeInTimer.at(frames(0));

    t0f.doTask([=](sf::Time elapsed) {
      this->opacity = ease::linear(elapsed.asSeconds(), seconds_cast<float>(frames(14)), 1.0f);
    }).withDuration(frames(14));

    t0f.doTask([=](sf::Time elapsed) {
      float x = ease::linear(elapsed.asSeconds(), seconds_cast<float>(frames(8)), 1.0f);
      this->banner->setPosition((1.0f - x) * -this->banner->getSprite().getLocalBounds().width, 0);
    }).withDuration(frames(8));

    if (state == PersonalMenu::state::closing) {
      t0f.doTask([=](sf::Time elapsed) {
        currState = state::closed;
        Menu::Close();
      });

      t0f.doTask([=](sf::Time elapsed) {
        for (size_t i = 0; i < options.size(); i++) {

          //
          // labels (menu options)
          //

          float x = ease::linear(elapsed.asSeconds(), seconds_cast<float>(frames(20)), 1.0f);
          float start = 36;
          float dest = -(options[i]->getLocalBounds().width + start); // our destination

          // lerp to our hiding spot
          options[i]->setPosition(dest * (1.0f - x) + (x * start), options[i]->getPosition().y);

          //
          // icons
          //
          start = 16;
          dest = dest - start; // our destination is calculated from the previous label's pos

          // lerp to our hiding spot
          optionIcons[i]->setPosition(dest * (1.0f - x) + (x * start), optionIcons[i]->getPosition().y);
        }
      }).withDuration(frames(20));
    }

    //
    // These tasks begin at the 8th frame
    //

    auto& t8f = easeInTimer.at(frames(8));

    if (state == PersonalMenu::state::opening) {
      t8f.doTask([=](sf::Time elapsed) {
        placeTextSpr->Reveal();
        selectTextSpr->Reveal();
        exit->Reveal();

        for (auto&& opts : options) {
          opts->Reveal();
        }

        for (auto&& opts : optionIcons) {
          opts->Reveal();
        }
        });

      t8f.doTask([=](sf::Time elapsed) {
        for (size_t i = 0; i < options.size(); i++) {
          float y = static_cast<float>(ease::linear(elapsed.asSeconds(), seconds_cast<float>(frames(12)), 1.0f));
          options[i]->setPosition(36, 26 + (y * (i * 16)));
          optionIcons[i]->setPosition(16, 26 + (y * (i * 16)));
        }
        }).withDuration(frames(12));
    }
    else {
      t8f.doTask([=](sf::Time elapsed) {
        placeTextSpr->Hide();
        selectTextSpr->Hide();
        exit->Hide();

        //infobox task handles showing, but we need to hide if closing
        infoBox->Hide();

        for (auto&& opts : options) {
          opts->Hide();
        }


        for (auto&& opts : optionIcons) {
          opts->Hide();
        }
      });
    }

    t8f.doTask([=](sf::Time elapsed) {
      float x = 1.0f - ease::linear(elapsed.asSeconds(), seconds_cast<float>(frames(6)), 1.0f);
      exit->setPosition(130 + (x * 200), exit->getPosition().y);
    }).withDuration(frames(6));

    t8f.doTask([=](sf::Time elapsed) {
      std::string printName = areaName;

      while (printName.size() < 12) {
        printName = "_" + printName; // add underscore brackets to output text
      }

      size_t offset = static_cast<size_t>(13 * ease::linear(elapsed.asSeconds(), seconds_cast<float>(frames(2)), 1.0f));
      std::string substr = printName.substr(0, offset);
      areaLabel.SetString(substr);
    }).withDuration(frames(2));

    //
    // These tasks begin on the 14th frame
    //

    easeInTimer
      .at(time_cast<sf::Time>(frames(14)))
      .doTask([=](sf::Time elapsed) {
        infoBox->Reveal();
        infoBoxAnim.SyncTime(from_seconds(elapsed.asSeconds()));
        infoBoxAnim.Refresh(infoBox->getSprite());
      }).withDuration(frames(4));

    //
    // on frame 20 change state flag
    //
    if (state == PersonalMenu::state::opening) {
      easeInTimer
        .at(frames(20))
        .doTask([=](sf::Time elapsed) {
        currState = state::opened;
          });
    }
  }

  void PersonalMenu::CreateOptions()
  {
    options.reserve(optionsList.size() * 2);
    optionIcons.reserve(optionsList.size() * 2);

    for (auto&& L : optionsList) {
      // label
      auto sprite = std::make_shared<SpriteProxyNode>();
      sprite->setTexture(Textures().LoadFromFile("resources/ui/main_menu_ui.png"));
      sprite->setPosition(36, 26);
      optionAnim << (L.name + "_LABEL");
      optionAnim.SetFrame(1, sprite->getSprite());
      options.push_back(sprite);
      sprite->Hide();
      AddNode(sprite);

      // icon
      auto iconSpr = std::make_shared<SpriteProxyNode>();
      iconSpr->setTexture(Textures().LoadFromFile("resources/ui/main_menu_ui.png"));
      iconSpr->setPosition(36, 26);
      optionAnim << L.name;
      optionAnim.SetFrame(1, iconSpr->getSprite());
      optionIcons.push_back(iconSpr);
      iconSpr->Hide();
      AddNode(iconSpr);
    }
  }


  void PersonalMenu::Update(double elapsed)
  {
    frameTick += from_seconds(elapsed);
    if (frameTick.count() >= 60) {
      frameTick = frames(0);
    }

    easeInTimer.update(sf::seconds(static_cast<float>(elapsed)));
    elapsedThisFrame = elapsed;
    healthUI.SetHP(session->health);
    healthUI.Update(elapsed);

    if (!IsOpen()) return;

    // loop over options
    for (size_t i = 0; i < optionsList.size(); i++) {
      if (i == row && selectExit == false) {
        optionAnim << (optionsList[i].name + "_LABEL");
        optionAnim.SetFrame(2, options[i]->getSprite());

        // move the icon inwards to the label
        optionAnim << optionsList[i].name;
        optionAnim.SetFrame(2, optionIcons[i]->getSprite());

        const auto& pos = optionIcons[i]->getPosition();
        float delta = ease::interpolate(0.5f, pos.x, 20.0f + 5.0f);
        optionIcons[i]->setPosition(delta, pos.y);
      }
      else {
        optionAnim << (optionsList[i].name + "_LABEL");
        optionAnim.SetFrame(1, options[i]->getSprite());

        // move the icon away from the label
        optionAnim << optionsList[i].name;
        optionAnim.SetFrame(1, optionIcons[i]->getSprite());

        const auto& pos = optionIcons[i]->getPosition();
        float delta = ease::interpolate(0.5f, pos.x, 16.0f);
        optionIcons[i]->setPosition(delta, pos.y);
      }
    }

    if (selectExit) {
      exitAnim.Update(elapsed, exit->getSprite());
    }
  }

  void PersonalMenu::HandleInput(InputManager& input, sf::Vector2f mousePos) {
    if (currState != state::opened) {
      // ignore input unless the menu is fully open
      return;
    }

    if (input.Has(InputEvents::pressed_pause) && !input.Has(InputEvents::pressed_cancel)) {
      Close();
      Audio().Play(AudioType::CHIP_DESC_CLOSE);
      return;
    }

    selectInputCooldown -= this->elapsedThisFrame;

    if (input.Has(InputEvents::pressed_ui_up) || input.Has(InputEvents::held_ui_up)) {
      if (selectInputCooldown <= 0) {
        if (!extendedHold) {
          selectInputCooldown = maxSelectInputCooldown;
          extendedHold = true;
        }
        else {
          selectInputCooldown = maxSelectInputCooldown / 4.0;
        }

        CursorMoveUp() ? Audio().Play(AudioType::CHIP_SELECT) : 0;
      }
    }
    else if (input.Has(InputEvents::pressed_ui_down) || input.Has(InputEvents::held_ui_down)) {
      if (selectInputCooldown <= 0) {
        if (!extendedHold) {
          selectInputCooldown = maxSelectInputCooldown;
          extendedHold = true;
        }
        else {
          selectInputCooldown = maxSelectInputCooldown / 4.0;
        }

        CursorMoveDown() ? Audio().Play(AudioType::CHIP_SELECT) : 0;
      }
    }
    else if (input.Has(InputEvents::pressed_confirm)) {
      ExecuteSelection();
    }
    else if (input.Has(InputEvents::pressed_ui_right) || input.Has(InputEvents::pressed_cancel)) {
      extendedHold = false;

      bool exitSelected = !SelectExit();

      if (exitSelected) {
        if (input.Has(InputEvents::pressed_cancel)) {
          ExecuteSelection();
        }
        else {
          // already selected, switch to options
          if (SelectOptions()) {
            Audio().Play(AudioType::CHIP_SELECT);
          }
        }
      }
    }
    else if (input.Has(InputEvents::pressed_ui_left)) {
      SelectOptions() ? Audio().Play(AudioType::CHIP_SELECT) : 0;
      extendedHold = false;
    }
    else {
      extendedHold = false;
      selectInputCooldown = 0;
    }
  }


  void PersonalMenu::draw(sf::RenderTarget& target, sf::RenderStates states) const
  {
    if (IsHidden()) return;

    states.transform *= getTransform();

    if (IsClosed()) {
      target.draw(*icon, states);
      target.draw(healthUI, states);

      const sf::Vector2f pos = areaLabelThick.getPosition();
      areaLabelThick.SetColor(sf::Color(105, 105, 105));
      areaLabelThick.setPosition(pos.x + 1.f, pos.y + 1.f);
      target.draw(areaLabelThick, states);

      areaLabelThick.setPosition(pos);
      areaLabelThick.SetColor(sf::Color::White);
      target.draw(areaLabelThick, states);
    }
    else {
      // draw black square to darken bg
      const sf::View view = target.getView();
      sf::RectangleShape screen(view.getSize());
      screen.setFillColor(sf::Color(0, 0, 0, int(opacity * 255.f * 0.5f)));
      target.draw(screen, sf::RenderStates::Default);

      // draw all child nodes
      SceneNode::draw(target, states);

      auto shadowColor = sf::Color(16, 82, 107, 255);

      // area text
      const sf::Vector2f pos = areaLabel.getPosition();
      Text copyAreaLabel = areaLabel;
      copyAreaLabel.setPosition(pos.x + 1, pos.y + 1);
      copyAreaLabel.SetColor(shadowColor);
      target.draw(copyAreaLabel, states);
      target.draw(areaLabel, states);

      if (currState == state::opened) {
        // hp shadow
        infoText.SetString(std::to_string(session->health));
        infoText.setOrigin(infoText.GetLocalBounds().width, 0);
        infoText.SetColor(shadowColor);
        infoText.setPosition(174 + 1, 33 + 1);
        target.draw(infoText, states);

        // hp text
        infoText.setPosition(174, 33);
        infoText.SetColor(sf::Color::White);
        target.draw(infoText, states);

        // "/" shadow
        infoText.SetString("/");
        infoText.setOrigin(infoText.GetLocalBounds().width, 0);
        infoText.SetColor(shadowColor);
        infoText.setPosition(182 + 1, 33 + 1);
        target.draw(infoText, states);

        // "/"
        infoText.setPosition(182, 33);
        infoText.SetColor(sf::Color::White);
        target.draw(infoText, states);

        // max hp shadow
        infoText.SetString(std::to_string(session->maxHealth));
        infoText.setOrigin(infoText.GetLocalBounds().width, 0);
        infoText.SetColor(shadowColor);
        infoText.setOrigin(infoText.GetLocalBounds().width, 0);
        infoText.setPosition(214 + 1, 33 + 1);
        target.draw(infoText, states);

        // max hp 
        infoText.setPosition(214, 33);
        infoText.SetColor(sf::Color::White);
        target.draw(infoText, states);

        // coins shadow
        infoText.SetColor(shadowColor);
        infoText.SetString(std::to_string(session->money) + "$");
        infoText.setOrigin(infoText.GetLocalBounds().width, 0);
        infoText.setPosition(214 + 1, 57 + 1);
        target.draw(infoText, states);

        // coins
        infoText.setPosition(214, 57);
        infoText.SetColor(sf::Color::White);
        target.draw(infoText, states);
      }
    }

    DrawTime(target);
  }

  void PersonalMenu::SetPlayerDisplay(PlayerDisplay mode)
  {
    switch (mode) {
    case PlayerDisplay::PlayerHealth:
    {
      healthUI.Reveal();
      icon->Hide();
    }
    break;
    case PlayerDisplay::PlayerIcon: 
    {
      healthUI.Hide();
      icon->Reveal();
    }
    break;
    }
  }

  void PersonalMenu::DrawTime(sf::RenderTarget& target) const
  {
    auto shadowColor = sf::Color(105, 105, 105);
    std::string format = (frameTick.count() < 30) ? "%OI:%OM %p" : "%OI %OM %p";
    std::string timeStr = CurrentTime::AsFormattedString(format);
    time.SetString(timeStr);
    time.setOrigin(time.GetLocalBounds().width, 0.f);
    auto origin = time.GetLocalBounds().width;

    auto pos = time.getPosition();
    time.SetColor(shadowColor);
    time.setPosition(pos.x + 2.f, pos.y + 2.f);
    target.draw(time);

    time.SetString(timeStr.substr(0, 5));
    time.setPosition(pos);
    time.SetColor(sf::Color::White);
    target.draw(time);

    auto pColor = sf::Color::Red;
    time.SetString("AM");

    if (timeStr[6] != 'A') {
      pColor = sf::Color::Green;
      time.SetString("PM");
    }

    time.setOrigin(time.GetLocalBounds().width, 0.f);
    time.SetColor(pColor);
    target.draw(time);
  }

  void PersonalMenu::SetArea(const std::string& name)
  {
    auto bounds = areaLabelThick.GetLocalBounds();
    areaName = name;

    std::string printName = name;
    while (printName.size() < 12) {
      printName = "_" + printName; // add underscore brackets to output text
    }

    areaLabelThick.SetString(printName);
    areaLabelThick.setOrigin(bounds.width, bounds.height);
    areaLabelThick.setPosition(240 - 1.f, 160 - 2.f);
  }

  void PersonalMenu::UseIconTexture(const std::shared_ptr<sf::Texture> iconTexture)
  {
    this->iconTexture = iconTexture;
    this->icon->setTexture(iconTexture, true);
  }

  void PersonalMenu::ResetIconTexture()
  {
    iconTexture.reset();

    optionAnim << "PET";
    icon->setTexture(widgetTexture);
    optionAnim.SetFrame(1, icon->getSprite());
  }

  bool PersonalMenu::ExecuteSelection()
  {
    if (selectExit) {
      if (currState == state::opened) {
        Close();
        Audio().Play(AudioType::CHIP_DESC_CLOSE);
        return true;
      }
    }
    else {
      auto& func = optionsList[row].onSelectFunc;

      if (func) {
        func();
        return true;
      }
    }

    return false;
  }

  bool PersonalMenu::SelectExit()
  {
    if (!selectExit) {
      Audio().Play(AudioType::CHIP_SELECT);

      selectExit = true;
      exitAnim << "EXIT_SELECTED" << Animator::Mode::Loop;
      exitAnim.SetFrame(1, exit->getSprite());

      for (size_t i = 0; i < optionsList.size(); i++) {
        optionAnim << optionsList[i].name;
        optionAnim.SetFrame(1, optionIcons[i]->getSprite());

        optionAnim << (optionsList[i].name + "_LABEL");
        optionAnim.SetFrame(1, options[i]->getSprite());
      }
      return true;
    }

    return false;
  }

  bool PersonalMenu::SelectOptions()
  {
    if (selectExit) {
      selectExit = false;
      row = 0;
      exitAnim << "EXIT";
      exitAnim.SetFrame(1, exit->getSprite());
      return true;
    }

    return false;
  }

  bool PersonalMenu::CursorMoveUp()
  {
    if (!selectExit) {
      if (--row < 0) {
        row = static_cast<int>(optionsList.size() - 1);
      }

      return true;
    }

    row = std::max(row, 0);

    return false;
  }

  bool PersonalMenu::CursorMoveDown()
  {
    if (!selectExit) {
      row = (row + 1u) % (int)optionsList.size();

      return true;
    }

    return false;
  }

  void PersonalMenu::Open()
  {
    if (currState == state::closed) {
      Audio().Play(AudioType::CHIP_DESC);
      currState = state::opening;
      QueueAnimTasks(currState);
      easeInTimer.start();
      Menu::Open();
    }
  }

  void PersonalMenu::Close()
  {
    if (currState == state::opened) {
      currState = state::closing;
      QueueAnimTasks(currState);
      easeInTimer.start();
    }
  }
}