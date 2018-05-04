#include "bnChipSelectionCust.h"
#include "bnTextureResourceManager.h"

#define SHADER_FRAG_PATH "resources/shaders/greyscale.frag.txt"

ChipSelectionCust::ChipSelectionCust(int cap) {
  chipCap = cap;
  queue = new Bucket[chipCap];
  selectQueue = new Bucket*[chipCap];

  chipCount = selectCount = cursorPos = 0;

  custSprite = sf::Sprite(*TextureResourceManager::GetInstance().GetTexture(TextureType::CHIP_SELECT_MENU));
  custSprite.setScale(2.f, 2.f);
  custSprite.setPosition(-custSprite.getTextureRect().width*2.f, 0);

  icon.setTexture(*TextureResourceManager::GetInstance().GetTexture(CHIP_ICONS));
  icon.setScale(sf::Vector2f(2.f, 2.f));

  cursorSmall = sf::Sprite(*TextureResourceManager::GetInstance().GetTexture(TextureType::CHIP_CURSOR_SMALL));
  cursorSmall.setScale(sf::Vector2f(2.f, 2.f));

  cursorBig = sf::Sprite(*TextureResourceManager::GetInstance().GetTexture(TextureType::CHIP_CURSOR_BIG));
  cursorBig.setScale(sf::Vector2f(2.f, 2.f));
  cursorBig.setPosition(sf::Vector2f(2.f*92.f, 2.f*111.f));

  sf::Texture* card = TextureResourceManager::GetInstance().GetTexture(TextureType::CHIP_CARDS);
  chipCard.setTexture(*card);
  chipCard.setScale(2.f, 2.f);
  chipCard.setPosition(2.f*16.f, 48.f);

  if (!greyscale.loadFromFile(SHADER_FRAG_PATH, sf::Shader::Fragment)) {
    Logger::Logf("Error loading shader: %s\n", SHADER_FRAG_PATH);
  }

  sf::Font* font = TextureResourceManager::GetInstance().LoadFontFromFile("resources/fonts/mmbnthick_regular.ttf");
  label.setFont(*font);

  ChipLibrary::GetInstance().LoadLibrary();
}


ChipSelectionCust::~ChipSelectionCust() {
  ClearChips();

  for (int i = 0; i < chipCount; i++) {
    selectQueue[i] = nullptr;
    delete queue[i].data;
  }

  if (chipCount > 0) {
    delete[] queue;
    delete[] selectQueue;
  }

  chipCount = 0;
}

void ChipSelectionCust::CursorRight() {
  if (++cursorPos > 5) {
    cursorPos = 5;
  }
}

void ChipSelectionCust::CursorLeft() {
  if (--cursorPos < 0) {
    cursorPos = 0;
  }
}

void ChipSelectionCust::CursorAction() {
  if (cursorPos == 5) {
    // End chip select
    areChipsReady = true;
  } else {
    // Does chip exist 
    if (cursorPos < chipCount) {
      // Queue this chip if not selected
      if (queue[cursorPos].state == 1) {
        selectQueue[selectCount++] = &queue[cursorPos];
        queue[cursorPos].state = 2;

        // Check chip code. If other chips are not compatible, set their bucket state flag to 0
        char code = queue[cursorPos].data->GetCode();

        for (int i = 0; i < chipCount; i++) {
          if (i == cursorPos || queue[i].state == 0 || queue[i].state == 2) continue;
          char otherCode = queue[i].data->GetCode();

          if (code == '=' || otherCode == '=' || otherCode == code || otherCode == code - 1 || otherCode == code + 1) { queue[i].state = 1; }
          else { queue[i].state = 0; }
        }
      }
    }
  }
}

void ChipSelectionCust::CursorCancel() {
  // Unqueue all chips buckets
  if (selectCount <= 0) {
    selectCount = 0;
    return;
  }

  selectQueue[selectCount-1]->state = 1;

  selectCount--;

  if (selectCount == 0) {
    // Everything is selectable again
    for (int i = 0; i < chipCount; i++) {
      queue[i].state = 1;
    }

    return;
  }

  /*
    Compatible chip states are built upon adding chips from the last available chip states. 
    The only way to "revert" to the last compatible chip states is to step through the already selected 
    chip queue and build up the state again. 
  */


  for(int i = 0; i < selectCount; i++) {
    char code = selectQueue[i]->data->GetCode();

    for (int j = 0; j < chipCount; j++) {
      if (i > 0) {
        if (queue[j].state == 0) continue; // already checked
      }

      if (queue[j].state == 2) continue; // skip  

      char otherCode = queue[j].data->GetCode();

      if (code == '=' || otherCode == '=' || otherCode == code || otherCode == code - 1 || otherCode == code + 1) { queue[j].state = 1; }
      else { queue[j].state = 0; }
    }
  }
}

bool ChipSelectionCust::IsOutOfView() {
  float bounds = -custSprite.getTextureRect().width*2.f;

  if (custSprite.getPosition().x <= bounds) {
    custSprite.setPosition(bounds, custSprite.getPosition().y);
  }

  return (custSprite.getPosition().x == bounds);
}

bool ChipSelectionCust::IsInView() {
  float bounds = 0;

  if (custSprite.getPosition().x >= bounds) {
    custSprite.setPosition(bounds, custSprite.getPosition().y);
  }

  return (custSprite.getPosition().x == bounds);
}

void ChipSelectionCust::Move(sf::Vector2f delta) {
  custSprite.setPosition(custSprite.getPosition() + delta);
}

void ChipSelectionCust::GetNextChips() {
  ClearChips();

  for (int i = chipCount; i < chipCap; i++) {

    queue[i].data = ChipLibrary::GetInstance().Next();
    queue[i].state = 1;

    if (!queue[i].data) {
      // nullptr is end of list
      return;
    }
    chipCount++;
  }
}

void ChipSelectionCust::Draw() {
  Engine::GetInstance().Draw(custSprite, false);

  if (IsInView()) {
    cursorSmall.setPosition(2.f*(7.0f + (cursorPos*16.0f)), 2.f*103.f); // TODO: Make this relative to cust instead of screen

    for (int i = 0; i < chipCount; i++) {
      icon.setPosition(2.f*(9.0f + (i*16.0f)), 2.f*105.f);
      sf::IntRect iconSubFrame = TextureResourceManager::GetInstance().GetIconRectFromChipID(queue[i].data->GetID());
      icon.setTextureRect(iconSubFrame);
      icon.SetShader(nullptr);

      if (queue[i].state == 0) {
        icon.SetShader(&greyscale);
        Engine::GetInstance().Draw(&icon);
      } else if (queue[i].state == 1) {
        Engine::GetInstance().Draw(icon, false);
      }
    }


    icon.SetShader(nullptr);

    for (int i = 0; i < selectCount; i++) {
      icon.setPosition(2 * 97.f, 2.f*(25.0f + (i*16.0f)));
      sf::IntRect iconSubFrame = TextureResourceManager::GetInstance().GetIconRectFromChipID((*selectQueue[i]).data->GetID());
      icon.setTextureRect(iconSubFrame);
      Engine::GetInstance().Draw(icon, false);
    }


    if (cursorPos < 5) {
      // Draw the selected chip info
      label.setFillColor(sf::Color::White);

      if (cursorPos < chipCount) {
        // Draw the selected chip card
        sf::IntRect cardSubFrame = TextureResourceManager::GetInstance().GetCardRectFromChipID(queue[cursorPos].data->GetID());
        chipCard.setTextureRect(cardSubFrame);

        chipCard.SetShader(nullptr);

        if (!queue[cursorPos].state) {
          chipCard.SetShader(&greyscale);
          Engine::GetInstance().Draw((LayeredDrawable*)&chipCard);
        } else {
          Engine::GetInstance().Draw(chipCard, false);
        }

        label.setPosition(2.f*16.f, 16.f);
        label.setString(queue[cursorPos].data->GetShortName());
        Engine::GetInstance().Draw(label, false);

        // the order here is very important:
        label.setString(std::to_string(queue[cursorPos].data->GetDamage()));
        label.setOrigin(label.getLocalBounds().width*2.f, 0);
        label.setPosition(2.f*(label.getLocalBounds().width + 60.f), 143.f);
        Engine::GetInstance().Draw(label, false);

        label.setPosition(2.f*16.f, 143.f);
        label.setOrigin(0, 0);
        label.setString(std::string() + queue[cursorPos].data->GetCode());
        label.setFillColor(sf::Color(225, 180, 0));
        Engine::GetInstance().Draw(label, false);
      }

      // Draw the small cursor
      Engine::GetInstance().Draw(cursorSmall, false);
    } else
      Engine::GetInstance().Draw(cursorBig, false);
  }
}

Chip** ChipSelectionCust::GetChips() {
  selectedChips = (Chip**)malloc(sizeof(Chip*)*selectCount);

  for (int i = 0; i < selectCount; i++) {
    selectedChips[i] = (*(selectQueue[i])).data;
  }

  return selectedChips;
}

void ChipSelectionCust::ClearChips() {
  for (int i = 0; i < selectCount; i++) {
    if (*(selectedChips+i) != nullptr) {
      delete selectedChips[i];
    }

    (*(selectQueue[i])).data = nullptr;
    selectedChips[i] = nullptr;
  }

  if (selectCount > 0)
    delete[] selectedChips;

  // Restructure queue
  for (int i = 0; i < chipCount; i++) {
    queue[i].state = 1;
    int next = i;
    while (!queue[i].data && next + 1 < chipCount) {
      queue[i].data = queue[next + 1].data;
      queue[next + 1].data = nullptr;
      next++;
    }
  }

  chipCount = chipCount - selectCount;
  selectCount = 0;
}

const int ChipSelectionCust::GetChipCount() {
  return selectCount;
}

void ChipSelectionCust::ResetState() {
  cursorPos = 0;
  areChipsReady = false;
}

bool ChipSelectionCust::AreChipsReady() {
  return areChipsReady;
}
