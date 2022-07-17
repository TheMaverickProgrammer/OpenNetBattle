#pragma once
#include <string>
#include <vector>
#include <functional>
#include "../bnMenu.h"
#include "../bnText.h"
class TerminalConsole : public Menu {
public:
  enum class MessageType {
    Command,
    Response
  };

private:
  struct Message {
    MessageType type;
    std::string text;
  };

  bool capturing{ false };
  std::string pendingCommand;
  std::vector<Message> messages;
  sf::RectangleShape background;
  mutable Text text{ Font::Style::thin };
  std::function<void(const std::string&)> callbackFn;
public:
  TerminalConsole(const std::function<void(const std::string&)>& callbackFn);
  ~TerminalConsole();

  void SetDimensions(int width, int height);
  void AddResponse(const std::string& message);
  void Update(double elapsed) override;
  void Open() override;
  void Close() override;
  void HandleInput(InputManager& input, sf::Vector2f mousePos) override;
  void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
};