#include "bnBattleResults.h"
#include "bnAudioResourceManager.h"
#include "bnTextureResourceManager.h"
#include "bnShaderResourceManager.h"
#include "bnGame.h"
#include "bnMob.h"
#include "bnBattleItem.h"
#include <numeric>
#include <algorithm>
#include <random>

BattleResultsWidget::BattleResultsWidget(const BattleResults& results, Mob* mob, CardPackageManager& packageManager) :
  cardMatrixIndex(0), 
  isRevealed(false),
  playSoundOnce(false),
  rewardIsCard(false), 
  finalHealth(results.playerHealth),
  item(nullptr), 
  score(results.score), 
  counterCount(results.counterCount),
  totalElapsed(),
  time(Font::Style::gradient_tall), 
  rank(Font::Style::gradient_tall), 
  reward(Font::Style::thick), 
  cardCode(Font::Style::thick)
{
  this->counterCount = std::min(3, counterCount);
  this->counterCount = std::max(0, this->counterCount);

  std::random_device rd;
  std::mt19937 g(rd());

  // begin counting index at 0
  std::iota(hideCardMatrix.begin(), hideCardMatrix.end(), 0);

  // shuffle twice - one shuffle looks too linear
  std::shuffle(hideCardMatrix.begin(), hideCardMatrix.end(), g);
  std::shuffle(hideCardMatrix.begin(), hideCardMatrix.end(), g);

  cardMatrixIndex = 0;

  // Get reward based on score
  item = mob->GetRankedReward(score);
  resultsSprite = sf::Sprite(*Textures().LoadFromFile(TexturePaths::BATTLE_RESULTS_FRAME));
  resultsSprite.setScale(2.f, 2.f);
  resultsSprite.setPosition(-resultsSprite.getTextureRect().width*2.f, 20.f);

  pressA = sf::Sprite(*Textures().LoadFromFile(TexturePaths::BATTLE_RESULTS_PRESS_A));
  pressA.setScale(2.f, 2.f);
  pressA.setPosition(2.f*42.f, 249.f);

  star = sf::Sprite(*Textures().LoadFromFile(TexturePaths::BATTLE_RESULTS_STAR));
  star.setScale(2.f, 2.f);


  if (item) {
    rewardCard = sf::Sprite(*packageManager.FindPackageByID(item->GetUUID()).GetPreviewTexture());

    rewardCard.setTextureRect(sf::IntRect(0,0,56,48));

    if (item->IsCard()) {
      rewardIsCard = true;

      cardCode.setPosition(2.f*114.f, 216.f);
      cardCode.SetString(std::string() + item->GetCardCode());
      cardCode.setScale(2.f, 2.f);
    }
  }
  else {
    rewardCard = sf::Sprite(*Textures().LoadFromFile(TexturePaths::BATTLE_RESULTS_NODATA));
  }

  rewardCard.setScale(2.f, 2.f);
  rewardCard.setPosition(274.0f, 180.f);

  time.setPosition(2.f*192.f, 87.f);
  time.SetString(FormatString(results.battleLength));
  time.setOrigin(time.GetLocalBounds().width, -1);
  time.setScale(2.f, 2.f);

  rank.setPosition(2.f*192.f, 119.f);
  rank.setScale(2.f, 2.f);

  reward.setPosition(2.f*42.f, 220.f);
  reward.setScale(2.f, 2.f);

  if (item) {
    reward.SetString(item->GetName());
  }
  else {
    reward.SetString("No Data");
  }

  if (score > 10) {
    rank.SetString("S");
  }
  else {
    rank.SetString(std::to_string(score));
  }

  rank.setOrigin(rank.GetLocalBounds().width, -1);

  playSoundOnce = false;
}

BattleResultsWidget::~BattleResultsWidget() {

}

std::string BattleResultsWidget::FormatString(sf::Time time)
{
  double totalMS = time.asMilliseconds();
  
  int minutes = (int)totalMS / 1000 / 60;
  int seconds = (int)(totalMS / 1000.0) % 60;
  int ms = (int)(totalMS - (minutes*60*1000) - (seconds*1000)) % 99;

  if (minutes > 59) {
    minutes = 59;
  }

  std::string O = "0";
  std::string builder;

  if (minutes < 10) builder += O;
  builder += std::to_string(minutes) + ":";

  if (seconds < 10) builder += O;
  builder += std::to_string(seconds) + ":";

  if (ms < 10) builder += O;
  builder += std::to_string(ms);

  return builder;
}

// GUI ops
bool BattleResultsWidget::CursorAction() {
  bool prevStatus = isRevealed;

  if (!isRevealed) {
    isRevealed = true;
    totalElapsed = 0;
  } /*
    else if(extraItems.size() > 0) {
        item = extraItems.top(); extraItems.pop();
        totalElapsed =  0;
    }
    */

  return prevStatus;
}

bool BattleResultsWidget::IsOutOfView() {
  float bounds = -resultsSprite.getTextureRect().width*2.f;

  if (resultsSprite.getPosition().x <= bounds) {
    resultsSprite.setPosition(bounds, resultsSprite.getPosition().y);
  }

  return (resultsSprite.getPosition().x == bounds);
}

bool BattleResultsWidget::IsInView() {
  float bounds = 50.f;

  if (resultsSprite.getPosition().x >= bounds) {
    resultsSprite.setPosition(bounds, resultsSprite.getPosition().y);
  }

  return (resultsSprite.getPosition().x == bounds);
}

void BattleResultsWidget::Move(sf::Vector2f delta) {
  resultsSprite.setPosition(resultsSprite.getPosition() + delta);
  IsInView();
}

void BattleResultsWidget::Update(double elapsed)
{
  totalElapsed += elapsed;

  if ((int)totalElapsed % 2 == 0) {
    pressA.setScale(0, 0);
  }
  else {
    pressA.setScale(2.f, 2.f);
  }

  if (isRevealed) {
    if (cardMatrixIndex == hideCardMatrix.size() && !playSoundOnce) {
      playSoundOnce = true;
      Audio().Play(AudioType::ITEM_GET);
    }
    else {
      if (cardMatrixIndex < hideCardMatrix.size()) {
        hideCardMatrix[cardMatrixIndex++] = 0;
      }
    }

    if (!playSoundOnce) {
      Audio().Play(AudioType::TEXT, AudioPriority::lowest);
    }
  }
}

void BattleResultsWidget::Draw(IRenderer& renderer) {
  renderer.submit(&resultsSprite);

  // moves over when there's counter stars
  auto starSpacing = [](int index) -> float { return (19.f*index); };
  auto rankPos = sf::Vector2f((2.f*191.f) - starSpacing(counterCount), 116.f);

  if (IsInView()) {
    if (!isRevealed)
      renderer.submit(&pressA);

    renderer.submit(&rank);

    // Draw overlay
    rank.setPosition(rankPos);
    renderer.submit(&rank);

    // Draw overlay
    time.setPosition(2.f*191.f, 84.f);
    renderer.submit(&time);

    if (isRevealed) {
      renderer.submit(&rewardCard);

      sf::RectangleShape c(sf::Vector2f(8 * 2, 8 * 2));
      c.setFillColor(sf::Color::Black);
      c.setOutlineColor(sf::Color::Black);

      // obscure the card with a matrix
      for (auto cell : hideCardMatrix) {
        if (cell >= cardMatrixIndex) {
          // position based on cell's index from a 7x6 matrix
          sf::Vector2f offset = sf::Vector2f(float(cell % 7) * 8.0f, float(std::floor(cell / 7) * 8.0f));
          offset = 2.0f * offset;

          c.setPosition(rewardCard.getPosition() + offset);

          // TODO: remove Clone()
          renderer.submit(Clone(c));
        }
      }

      if (IsFinished()) {
        // shadow first
        auto rewardPos = reward.getPosition();
        reward.setPosition(rewardPos.x + 2.f, rewardPos.y + 2.f);
        reward.SetColor(sf::Color(80, 72, 88));

        // TODO: remove Clone()
        renderer.submit(Clone(reward));

        // then overlay
        reward.setPosition(rewardPos);
        reward.SetColor(sf::Color::White);
        // TODO: remove Clone()
        renderer.submit(Clone(reward));

        if (rewardIsCard) {
          auto codePos = cardCode.getPosition();
          cardCode.setPosition(codePos.x + 2.f, codePos.y + 2.f);
          cardCode.SetColor(sf::Color(80, 72, 88));
          // TODO: remove Clone()
          renderer.submit(Clone(cardCode));

          cardCode.setPosition(codePos);
          cardCode.SetColor(sf::Color::White);
          // TODO: remove Clone()
          renderer.submit(Clone(cardCode));
        }
      }
    }

    for (int i = 0; i < counterCount; i++) {
      star.setPosition(rankPos.x + starSpacing(i) + (starSpacing(1) / 2.0f), rankPos.y + 14.0f);
      // TODO: remove Clone()
      renderer.submit(Clone(star));
    }
  }
}

// Card ops
bool BattleResultsWidget::IsFinished() {
  return isRevealed && hideCardMatrix.size() == cardMatrixIndex;
}

BattleItem* BattleResultsWidget::GetReward()
{
  // pass ownership off
  BattleItem* reward = item;
  item = nullptr;
  return reward;
}

/*
  Calculate score and rank
  Calculations are based off http ://megaman.wikia.com/wiki/Virus_Busting

  Delete Time
    0.00~5.00 = 7
    5.01~12.00 = 6
    12.01~36.00 = 5
    36.01 and up = 4

    Boss Delete Time
    0.00~30.00 = 10
    30.01~40.00 = 8
    40.01~50.00 = 6
    50.01 and up = 4

    Number of Hits(received by MegaMan)
    0 = +1
    1 = 0
    2 = -1
    3 = -2
    4 or more = -3

    Movement Number of Steps
    0~2 = +1
    3 or more = +0

    Multiple Deletion
    Double Delete = +2
    Triple Delete = +4

    Counter Hits
    0 = +0
    1 = +1
    2 = +2
    3 = +3
    */

BattleResults& BattleResults::CalculateScore(BattleResults& results, Mob* mob)
{
  int score = 0;

  if (!mob->IsBoss()) {
    if (results.battleLength.asSeconds() > 36.1) score += 4;
    else if (results.battleLength.asSeconds() > 12.01) score += 5;
    else if (results.battleLength.asSeconds() > 5.01) score += 6;
    else score += 7;
  }
  else {
    if (results.battleLength.asSeconds() > 50.01) score += 4;
    else if (results.battleLength.asSeconds() > 40.01) score += 6;
    else if (results.battleLength.asSeconds() > 30.01) score += 8;
    else score += 10;
  }

  switch (results.hitCount) {
  case 0: score += 1; break;
  case 1: score += 0; break;
  case 2: score -= 1; break;
  case 3: score -= 2; break;
  default: score -= 3; break;
  }

  if (results.moveCount >= 0 && results.moveCount <= 2) {
    score += 1;
  }

  if (results.doubleDelete) score += 2;
  if (results.tripleDelete) score += 4;

  score += results.counterCount;

  // No score of zero or below. Min score of 1
  score = std::max(1, score);

  results.score = score;

  return results;
}
