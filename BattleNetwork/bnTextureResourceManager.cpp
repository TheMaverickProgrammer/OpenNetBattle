#include <stdlib.h>
#include <sstream>
using std::stringstream;
#include <fstream>
using std::ifstream;

#include "bnTextureResourceManager.h"

TextureResourceManager& TextureResourceManager::GetInstance() {
  static TextureResourceManager instance;
  return instance;
}

void TextureResourceManager::LoadAllTextures(unsigned &status) {
  TextureType textureType = static_cast<TextureType>(0);
  while (textureType != TEXTURE_TYPE_SIZE) {
    status++;

    // TODO: Catch failed resources and try again
    Texture* texture = nullptr;
    texture = LoadTextureFromFile(paths[static_cast<int>(textureType)]);
    if (texture) textures.insert(pair<TextureType, Texture*>(textureType, texture));
    textureType = (TextureType)(static_cast<int>(textureType) + 1);
  }
}

Texture* TextureResourceManager::LoadTextureFromFile(string _path) {
  Texture* texture = new Texture();
  if (!texture->loadFromFile(_path)) {
    Logger::Failf("Failed loading texture: %s\n", _path.c_str());
  } else {
    Logger::Logf("Loaded texture: %s\n", _path.c_str());
  }
  return texture;
}

Texture* TextureResourceManager::GetTexture(TextureType _ttype) {
  return textures.at(_ttype);
}

sf::IntRect TextureResourceManager::GetCardRectFromChipID(unsigned ID) {
  return sf::IntRect((ID % 11) * 56, (ID / 11) * 48, 56, 48);
}

// NOTE: This is not accurate. The cards and icons are not identical in order. Perhaps manual entry is best here? 
// Otherwise we'll need to manually swap icon placement ourselves...
sf::IntRect TextureResourceManager::GetIconRectFromChipID(unsigned ID) {
  return sf::IntRect(1+((ID % 20) * 16), 1+((ID / 20) * 16), 14, 14);
}

Font* TextureResourceManager::LoadFontFromFile(string _path) {
  Font* font = new Font();
  if (!font->loadFromFile(_path)) {
    Logger::Failf("Failed loading font: %s\n", _path.c_str());
  } else {
    Logger::Logf("Loaded font: %s\n", _path.c_str());
  }
  return font;
}

TextureResourceManager::TextureResourceManager(void) {
  //-Tiles-
  //Blue tile
  paths.push_back("resources/tiles/img_blue_normal.png");
  paths.push_back("resources/tiles/img_blue_cracked.png");
  paths.push_back("resources/tiles/img_blue_broken.png");
  paths.push_back("resources/tiles/img_blue_empty.png");
  //Red tile
  paths.push_back("resources/tiles/img_red_normal.png");
  paths.push_back("resources/tiles/img_red_cracked.png");
  paths.push_back("resources/tiles/img_red_broken.png");
  paths.push_back("resources/tiles/img_red_empty.png");

  //-Navis-
  //Megaman
  paths.push_back("resources/navis/megaman/navi_megaman_move.png");
  paths.push_back("resources/navis/megaman/navi_megaman_hit.png");
  paths.push_back("resources/navis/megaman/navi_megaman_shoot.png");

  //-Mobs-
  paths.push_back("resources/mobs/mob_move.png");
  paths.push_back("resources/mobs/mob_explosion.png");
  //Mettaur
  paths.push_back("resources/mobs/mettaur/mob_mettaur_move.png");
  paths.push_back("resources/mobs/mettaur/mob_mettaur_hit.png");
  paths.push_back("resources/mobs/mettaur/mob_mettaur_attack.png");

  // ProgsMan
  paths.push_back("resources/mobs/progsman/mob_progsman_idle.png");
  paths.push_back("resources/mobs/progsman/mob_progsman_move.png");
  paths.push_back("resources/mobs/progsman/mob_progsman_shoot.png");
  paths.push_back("resources/mobs/progsman/mob_progsman_hit.png");
  paths.push_back("resources/mobs/progsman/mob_progsman_punch.png");
  paths.push_back("resources/mobs/progsman/mob_progsman_throw.png");

  //-Spells-
  //Bullet
  paths.push_back("resources/spells/spell_bullet_hit.png");
  paths.push_back("resources/spells/spell_buster_charge.png");
  //paths.push_back("resources/spells/spell_charged_bullet_hit.png");
  paths.push_back("resources/spells/spell_wave.png");
  paths.push_back("resources/spells/spell_prog_bomb.png");

  // Chip Select 
  paths.push_back("resources/ui/chip_select.png");
  paths.push_back("resources/ui/chip_cursor_small.png");
  paths.push_back("resources/ui/chip_cursor_big.png");
  paths.push_back("resources/ui/chip_cards.png");
  paths.push_back("resources/ui/chip_icons.png");

  // Background/foreground
  paths.push_back("resources/backgrounds/title/bg_blue.png");
  paths.push_back("resources/backgrounds/title/prog-pulse.png");
}

TextureResourceManager::~TextureResourceManager(void) {
  for (auto it = textures.begin(); it != textures.end(); ++it) {
    delete it->second;
  }
}

string TextureResourceManager::LoadDataFromFile(string _path) {
  ifstream in(_path);
  stringstream buffer;
  buffer << in.rdbuf();
  string contents(buffer.str());
  return contents;
}