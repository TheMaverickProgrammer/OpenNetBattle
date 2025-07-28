#include "bnPlayerEmotionUI.h"
#include "bnPlayer.h"
#include "battlescene/bnBattleSceneBase.h"

/** @todo This should probably be replace with a .animation file
 * A pair of Frame-count to Whether-or-not-to-show-the-new-or-old-emotion.
 */
static const struct {
  frame_time_t frames{ ::frames(0) };
  bool showNewEmotion{};
} flickerAnimation[] = {
  { { ::frames(3)}, true  },  // For frames [0-3), show new emotion
  { { ::frames(6)}, false },  // For frames [3-6), show old emotion
  { { ::frames(9)}, true  },  // etc.
  { { ::frames(12)}, false },
  { { ::frames(15)}, true  },  // End on new emotion so it's shown when there is no more animation
};

PlayerEmotionUI::PlayerEmotionUI(std::weak_ptr<Player> player)
  : UIComponent(player)
  , texture{ nullptr }
  , emotionWindow()
  , emotion{ Emotion::normal }
  , previousEmotion{ Emotion::COUNT }
  , emotionChangeFrame{ 0 }
{
  SetDrawOnUIPass(false);
}

PlayerEmotionUI::~PlayerEmotionUI()
{
  this->Eject();
}

void PlayerEmotionUI::SetTexture(std::shared_ptr<sf::Texture> texture)
{
  this->texture = texture;
  emotionWindow.setTexture(texture);

  OnUpdate(0);
}

void PlayerEmotionUI::Inject(BattleSceneBase& scene)
{
  scene.Inject(shared_from_base<PlayerEmotionUI>());
}

void PlayerEmotionUI::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  if (this->IsHidden()) return;

  auto this_states = states;
  this_states.transform *= getTransform();

  auto player = GetOwnerAs<Player>();

  if (player && !player->IsInForm())
  {
    target.draw(emotionWindow, this_states);
  }

  UIComponent::draw(target, this_states);
}

void PlayerEmotionUI::OnUpdate(double elapsed)
{
  auto player = GetOwnerAs<Player>();

  if (!player) return;
  if (!texture) return;

  // previousEmotion diff is used to determine if the animation needs to play
  if (previousEmotion == player->GetEmotion() && !isChangingEmotion) return;

  // If the player's emotion has changed, prepare the flicker animation
  if (emotion != player->GetEmotion())
  {
    isChangingEmotion = true;
    previousEmotion = emotion;
    emotion = player->GetEmotion();
    emotionChangeFrame = {0};
  }
  else
  {
    emotionChangeFrame += from_seconds(elapsed);
  }

  // Flicker between the previous emotion and current emotion
  auto it = std::find_if(std::begin(flickerAnimation), std::end(flickerAnimation),
                         [this](auto& animationData){ return emotionChangeFrame < animationData.frames; });
  if (it != std::end(flickerAnimation))
  {
    SetDisplayedEmotion(it->showNewEmotion ? emotion : previousEmotion);
  }
  else
  {
    // Once we have completed the animation, set previousEmotion to signal the animation is complete
    previousEmotion = emotion;
    isChangingEmotion = false;
  }
}

void PlayerEmotionUI::OnFormSelected(int index)
{
  if (index == -1)
  {
    Reveal();
  }
  else
  {
    Hide();
  }
}

void PlayerEmotionUI::SetDisplayedEmotion(Emotion emotion)
{
  if (emotion >= Emotion::COUNT) return;
  sf::Vector2i atlasSize = {(int)texture->getSize().x, (int)texture->getSize().y};

  if (atlasSize.y < ((int)Emotion::COUNT * AtlasCellHeight))
  {
    // Texture too small to be an atlas, must be a single emotion
    // Force use of full texture in case texture was hot swapped
    emotionWindow.setTextureRect({{0, 0}, {atlasSize.x, atlasSize.y}});
  }
  else
  {
    // Texture must be an atlas, select cell based on emotion offset
    int cellHeight = atlasSize.y / (int)Emotion::COUNT;
    int cellOffsetY = cellHeight * (int)emotion;
    emotionWindow.setTextureRect({
      {0, cellOffsetY},
      {atlasSize.x, cellHeight}
    });
  }
}
