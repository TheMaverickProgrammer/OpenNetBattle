#include "bnTerminalConsole.h"

TerminalConsole::TerminalConsole(const std::function<void(const std::string&)>& callbackFn) : callbackFn(callbackFn)
{
}

TerminalConsole::~TerminalConsole()
{
}

void TerminalConsole::SetDimensions(int width, int height)
{
  background = sf::RectangleShape(sf::Vector2f(width, height));
  background.setFillColor(sf::Color(0, 0, 0, 220));
}

void TerminalConsole::AddResponse(const std::string& message)
{
  messages.emplace(messages.begin(), TerminalConsole::Message{ MessageType::Response, message });
}

void TerminalConsole::Update(double elapsed)
{

}

void TerminalConsole::Open()
{
  Menu::Open();
}

void TerminalConsole::Close()
{
  Menu::Close();
}

void TerminalConsole::HandleInput(InputManager& input, sf::Vector2f mousePos)
{
  InputTextBuffer& textbuffer = input.GetInputTextBuffer();

  if (this->IsOpen()) {
    if (!capturing) {
      capturing = true;
      pendingCommand.clear();
      textbuffer.Reset();
      textbuffer.BeginCapture();
    }

    sf::Keyboard::Key anyKey = input.GetAnyKey();

    if (anyKey == sf::Keyboard::Escape) {
      this->Close();
      return;
    }

    pendingCommand = textbuffer.ToString();

    if (anyKey == sf::Keyboard::Enter) {
      pendingCommand = pendingCommand.substr(0, pendingCommand.length() - 1); // trim the newline char

      if (pendingCommand.size() > 0) {
        messages.emplace(messages.begin(), TerminalConsole::Message{ MessageType::Command, pendingCommand });

        const size_t max_history = 20;
        if (messages.size() > max_history) {
          messages.resize(max_history);
        }
        textbuffer.Reset();

        if (callbackFn) {
          callbackFn(pendingCommand);
        }

        pendingCommand.clear();
      }
    }
  }
  else {
    if (capturing) {
      capturing = false;
      textbuffer.EndCapture();
    }
  }
}

void TerminalConsole::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
  states.transform *= getTransform();

  target.draw(background, states);
  
  // draw the user's typing at the top
  float y_offset = 0.0f;
  text.SetColor(sf::Color::White);
  text.SetString(pendingCommand);
  text.setPosition(0.0f, y_offset);
  target.draw(text, states);
  y_offset += text.GetFont().GetLetterHeight();

  for (auto& m : messages) {
    if (m.type == MessageType::Response) {
      text.SetColor(sf::Color::Yellow);
    }
    else {
      // grey
      text.SetColor(sf::Color(150, 150, 150, 255));
    }

    text.SetString(m.text);
    text.setPosition(0.0f, y_offset);
    target.draw(text, states);
    y_offset += text.GetLocalBounds().height;
  }
}
