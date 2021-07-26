#include "bnMessageInput.h"

MessageInput::MessageInput(const std::string& initialText, size_t characterLimit) :
  MessageInterface("", nullptr),
  characterLimit(characterLimit)
{
  SetCaptureText(initialText);
}

MessageInput::~MessageInput() {
  if (capturingText) {
    Input().GetInputTextBuffer().EndCapture();
  }
}

void MessageInput::ProtectPassword(bool isPassword)
{
  password = isPassword;
}

void MessageInput::SetHint(const std::string& hint)
{
  this->hint = hint;
}

void MessageInput::SetCaptureText(const std::string& capture)
{
  latestCapture = capture.substr(0, characterLimit);
  prevCaretPosition = latestCapture.size();
}

bool MessageInput::IsDone() {
  return done;
}

const std::string& MessageInput::Submit() {
  GetTextBox()->DequeMessage();
  return Input().GetInputTextBuffer().ToString();
}

void MessageInput::OnUpdate(double elapsed) {
  auto& textBuffer = Input().GetInputTextBuffer();
  auto textbox = GetTextBox();

  if (!enteredView) {
    // prevent submission if we've started updating holding enter
    blockSubmission = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter);

    textbox->ReplaceText(latestCapture + ' ');
    textBuffer.BeginCapture();
    textBuffer.Reset();
    textBuffer.SetCharacterLimit(characterLimit);
    textBuffer.SetText(latestCapture);
    textBuffer.SetCaretPosition(prevCaretPosition);
    textBuffer.SetFont(textbox->GetFont());
    textBuffer.SetLineWidth(140);
    textBuffer.SetIgnoreNewLine(true);
    textBuffer.ProtectPassword(password);
    capturingText = true;
    enteredView = true;
  }

  // handle text updates
  // need to grab IsModified before grabbing text
  if (textBuffer.IsModified()) {
    // figure out which page we're on
    auto [firstRow, _] = textbox->GetCurrentLineRange();
    auto page = firstRow / textbox->GetNumberOfFittingLines();

    latestCapture = textBuffer.ToString();
    textbox->ReplaceText(latestCapture + ' '); // + ' ' needed to create a new blank page when the current page is filled
  }

  auto caretPosition = textBuffer.GetCaretPosition();

  if (caretPosition != prevCaretPosition) {
    // reset time to prevent the caret from blinking while typing
    time = 0;
    prevCaretPosition = caretPosition;
  }

  bool holdingShift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

  // allow \n if shift is held
  textBuffer.SetIgnoreNewLine(!holdingShift);

  if (Input().HasFocus()) {
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Home)) {
      // jump to start of block
      textBuffer.SetCaretPosition(textbox->GetBlockCharacterRange().first);
    }
    else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::End)) {
      // jump to end of block
      auto [_, end] = textbox->GetBlockCharacterRange();

      if (end > 0) {
        // jumping to the exact last char brings us to the next page
        // if the last char is a ' ' or '\n' it will place us just before that
        // also accounts for the added ' '
        end -= 1;
      }

      textBuffer.SetCaretPosition(end);
    }


    if (blockSubmission) {
      // keep blocking submission until enter is released
      blockSubmission = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter);
    }
    else if (!holdingShift && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter)) {
      // submit if shift is not held while pressing enter
      done = true;
    }
  }

  auto [caretRow, _] = textBuffer.GetRowCol();

  // resolve page
  while (true) {
    auto [rangeStart, rangeEnd] = textbox->GetCurrentLineRange();

    if (caretRow < rangeStart) {
      textbox->ShowPreviousLines();
    }
    else if (caretRow > rangeEnd && !textbox->IsFinalBlock()) {
      textbox->ShowNextLines();
    }
    else {
      break;
    }
  }

  // prevent animating from updates above
  if (textbox->IsPlaying()) {
    textbox->CompleteCurrentBlock();
  }

  time += elapsed;
}

void MessageInput::HandleClick(sf::Vector2f mousePos) {
  if (!enteredView) {
    // we have not initialized yet, continuing is pointless
    return;
  }

  mousePos.y -= 8.0f;

  if (mousePos.x <= 0 || mousePos.y <= 0) {
    return;
  }

  auto textbox = GetTextBox();

  mousePos -= textbox->GetTextPosition();
  mousePos /= 2.0f;

  if (mousePos.y >= textbox->GetFont().GetLineHeight() * textbox->GetNumberOfFittingLines()) {
    return;
  }

  auto [rangeStart, rangeEnd] = textbox->GetCurrentLineRange();
  mousePos.y += rangeStart * textbox->GetFont().GetLineHeight();

  auto& textBuffer = Input().GetInputTextBuffer();
  textBuffer.MoveCaretToCursor(mousePos);
}

void MessageInput::OnDraw(sf::RenderTarget& target, sf::RenderStates states) {
  auto textbox = GetTextBox();

  if (!hint.empty() && latestCapture.empty()) {
    Text hintTxt = textbox->MakeTextObject(hint);
    hintTxt.setPosition(textbox->GetTextPosition());
    hintTxt.setScale(textbox->getScale());
    hintTxt.SetColor(sf::Color::Blue);
    target.draw(hintTxt, states);
    return;
  }

  if (password) {
    textbox->ReplaceText(std::string(this->latestCapture.size(), '*'));
    textbox->CompleteCurrentBlock();
  }

  textbox->DrawMessage(target, states);

  if (!enteredView) {
    // we have not initialized yet
    // we can not trust the data in Input().GetInputTextBuffer()
    return;
  }

  // update text and caret
  const auto BLINK_FRAME_COUNT = 30;

  auto& textBuffer = Input().GetInputTextBuffer();

  auto [firstRow, _] = textbox->GetCurrentLineRange();
  auto [caretRow, caretCol] = textBuffer.GetRowCol();

  caretRow -= firstRow;

  if ((from_seconds(time).count() / BLINK_FRAME_COUNT) % 2 == 0) {
    auto textPosition = textbox->GetTextPosition();
    auto font = textbox->GetFont();

    // possibly slow, bnInputTextBuffer also does measurement, maybe it should be moved to Font?
    Text text(font);
    auto caretPosition = textBuffer.GetCaretPosition();

    std::string caretRef;

    if (password) {
      caretRef = std::string(this->latestCapture.size(), '*');
    }
    else {
      caretRef = latestCapture;
    }

    text.SetString(caretRef.substr(caretPosition - caretCol, caretCol));

    sf::RectangleShape caret;
    caret.setFillColor(sf::Color::Black);
    caret.setPosition(
      textPosition.x + text.GetWorldBounds().width * 2,
      textPosition.y + caretRow * font.GetLineHeight() * 2 + 8
    );
    caret.setSize(sf::Vector2f(2.0f, (font.GetLineHeight() - 2) * 2));

    target.draw(caret, states);
  }
}
