#include "bnTile.h"
#include "bnInvalidCardAction.h"
#include "bnCardPackageManager.h"

/***
 * all of this code will be tossed out when scripting cards is complete. 
 * This is for demonstration of the engine until we have scripting done
 */

std::shared_ptr<CardAction> CardToAction(
  const Battle::Card& card, 
  std::shared_ptr<Character> character, 
  CardPackagePartitioner* partition, 
  std::optional<Battle::Card::Properties> moddedProps
) {
  if (!character) return nullptr;

  stx::result_t<PackageAddress> maybe_addr = PackageAddress::FromStr(card.GetUUID());

  if (partition && !maybe_addr.is_error()) {
    PackageAddress addr = maybe_addr.value();

    if (partition->HasPackage(addr)) {
      CardMeta& meta = partition->FindPackageByAddress(addr);
      std::unique_ptr<CardImpl> cardImpl = std::unique_ptr<CardImpl>(meta.GetData());
      const Battle::Card::Properties& props = moddedProps.has_value() ? moddedProps.value() : meta.GetCardProperties();

      std::shared_ptr<CardAction> result = cardImpl->BuildCardAction(character, props);

      if (result) {
        result->SetMetaData(card);
        return result;
      }
    }
  }

  return std::make_shared<InvalidCardAction>(character);
}