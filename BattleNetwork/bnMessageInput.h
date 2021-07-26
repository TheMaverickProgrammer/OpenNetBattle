#pragma once

#include "bnResourceHandle.h"
#include "bnInputHandle.h"
#include "bnInputManager.h"
#include "bnAnimatedTextBox.h"
#include <functional>

class MessageInput : public MessageInterface, ResourceHandle, InputHandle {
public:
  MessageInput(const std::string& initialText, size_t characterLimit);
  ~MessageInput();

  void ProtectPassword(bool isPassword);
  void SetHint(const std::string& hint);
  void SetCaptureText(const std::string& capture);

  bool IsDone();
  void HandleClick(sf::Vector2f mousePos);
  const std::string& Submit();

  void OnUpdate(double elapsed) override;
  void OnDraw(sf::RenderTarget& target, sf::RenderStates states) override;
private:
  double time{};
  bool password{};
  bool done{};
  bool enteredView{};
  bool capturingText{};
  bool blockSubmission{};
  size_t characterLimit{};
  size_t prevCaretPosition{};
  std::string latestCapture;
  std::string hint;
};

