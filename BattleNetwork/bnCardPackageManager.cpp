#include "bnCardPackageManager.h"
#include "bnPlayer.h"
#include "bnLogger.h"
#include "stx/zip_utils.h"
#include <exception>
#include <atomic>
#include <thread>

#ifdef BN_MOD_SUPPORT
#include "bindings/bnScriptedCard.h"
#endif

CardMeta::CardMeta() :
  iconTexture(), previewTexture(),
  PackageManager<CardMeta>::Meta<CardImpl>()
{ 
}

CardMeta::~CardMeta() {
}

CardMeta& CardMeta::SetIconTexture(const std::shared_ptr<sf::Texture> icon)
{
  CardMeta::iconTexture = icon;

  return *this;
}

CardMeta& CardMeta::SetCodes(const std::vector<char> codes)
{
  this->codes = codes;

  // codes can only be upper case
  std::transform(this->codes.begin(), this->codes.end(), this->codes.begin(), ::toupper);

  return *this;
}

void CardMeta::OnMetaParsed()
{
  this->properties.uuid = this->GetPackageID();
}

CardMeta& CardMeta::SetPreviewTexture(const std::shared_ptr<Texture> texture)
{
  previewTexture = texture;
  return *this;
}

const std::shared_ptr<Texture> CardMeta::GetIconTexture() const
{
  return iconTexture;
}

const std::shared_ptr<Texture> CardMeta::GetPreviewTexture() const
{
  return previewTexture;
}

Battle::Card::Properties& CardMeta::GetCardProperties()
{
  return properties;
}

const std::vector<char> CardMeta::GetCodes() const
{
  return codes;
}
