#include "bnIntroScene.h"
#include "bnInputManager.h"

IntroScene::IntroScene(ActivityController& controller) 
  : 
  startFont(Font::Style::thick),
  message(startFont),
  Scene(controller)
{
  inMessageState = true;
  messageCooldown = 0.0f;

  std::shared_ptr<sf::Texture> alert = Textures().LoadFromFile("resources/ui/alert.png");
  alertSprite.setTexture(alert);
  alertSprite.setScale(2.f, 2.f);
  alertSprite.setOrigin(alertSprite.getLocalBounds().width / 2, alertSprite.getLocalBounds().height / 2);
  sf::Vector2f alertPos = (sf::Vector2f)((sf::Vector2i)getView().getSize() / 2);
  alertSprite.setPosition(sf::Vector2f(100.f, alertPos.y));

  message.SetString("This software is non profit\nIP rights belong to Capcom");
  message.setOrigin(message.GetLocalBounds().width / 2.f, message.GetLocalBounds().height * 2);
  message.setPosition(sf::Vector2f(300.f, 200.f));
  message.setScale(2.f, 2.f);
}

void IntroScene::onStart()
{
}

void IntroScene::onUpdate(double elapsed)
{
  // If 3 seconds is over, exit this loop
  if (messageCooldown <= 0) {
    inMessageState = false;
    messageCooldown = 0;
  }

  // Fade out 
  float alpha = std::min((messageCooldown)*255.f, 255.f);
  alertSprite.setColor(sf::Color((sf::Uint8)255.f, (sf::Uint8)255.f, (sf::Uint8)255.f, (sf::Uint8)alpha));
  message.SetColor(sf::Color((sf::Uint8)255.f, (sf::Uint8)255.f, (sf::Uint8)255.f, (sf::Uint8)alpha));
  messageCooldown -= static_cast<float>(elapsed) * speed;

  bool startPressed = (Input().IsConfigFileValid() ? Input().Has(InputEvents::pressed_confirm) : false) || Input().GetAnyKey() == sf::Keyboard::Return;

  if (startPressed) {
    speed = 5.f;
  }
}

void IntroScene::onLeave()
{
}

void IntroScene::onExit()
{
}

void IntroScene::onEnter()
{
}

void IntroScene::onResume()
{
}

void IntroScene::onDraw(IRenderer& renderer)
{
  // Draw the message
  renderer.submit(&alertSprite);
  renderer.submit(&message);
}

void IntroScene::onEnd()
{
}
