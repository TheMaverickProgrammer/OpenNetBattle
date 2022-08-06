#include "bnFrameRecorder.h"
#include "bnLogger.h"

#include <SFML/Window.hpp>

constexpr int maxOutstandingFrames = 10;

FrameRecorder::FrameRecorder(const sf::Window &window)
  : window(window), consumerThread([this] {
    Logger::Log(LogLevel::info, "Starting recording consumer thread!");
    while (true) {
      sf::Texture tex;

      {
        std::unique_lock<std::mutex> lk(usedTexturesLock);
        usedTexturesCv.wait(lk, [this] { return usedTextures.size() > 0 || stopped; });
        if (stopped && usedTextures.empty()) {
          break;
        }
        tex = std::move(usedTextures.front());
        usedTextures.pop_front();
      }
      usedTexturesCv.notify_one();

      // This operation is unavoidably expensive, but at least we don't have to hold up writers while doing it.
      sf::Image img = tex.copyToImage();

      {
        std::unique_lock<std::mutex> lk(freeTexturesLock);
        freeTextures.push_back(std::move(tex));
      }
      freeTexturesCv.notify_one();

      // TODO: Something more useful with with img.
      img.saveToFile("recording/frame_" +
                      std::to_string(totalFramesProcessed) + ".bmp");

      ++totalFramesProcessed;
    }
      Logger::Log(LogLevel::info, "Recording consumer thread stopped.");
  }) {
  // ctor
  for (int i = 0; i < maxOutstandingFrames; ++i) {
    sf::Texture tex;
    tex.create(window.getSize().x, window.getSize().y);
    freeTextures.push_back(std::move(tex));
  }
}

void FrameRecorder::capture() {
  {
    std::unique_lock<std::mutex> lk(usedTexturesLock);
    if (stopped) {
      // Cannot add frame to stopped recorder. This should never happen!
      return;
    }
  }

  sf::Texture tex;
  {
    std::unique_lock<std::mutex> lk(freeTexturesLock);
    if (freeTextures.empty()) {
      Logger::Log(LogLevel::info, "Recording is stalling.");
    }
    freeTexturesCv.wait(lk, [this] { return !freeTextures.empty(); });
    tex = std::move(freeTextures.back());
    freeTextures.pop_back();
  }
  freeTexturesCv.notify_one();

  tex.update(window);

  {
    std::unique_lock<std::mutex> lk(usedTexturesLock);
    usedTextures.push_back(std::move(tex));
  }
  usedTexturesCv.notify_one();
}

void FrameRecorder::flush() {
  Logger::Log(LogLevel::info, "Flushing recording...");

  {
    std::unique_lock<std::mutex> lk(usedTexturesLock);
    stopped = true;
  }
  usedTexturesCv.notify_one();

  {
    std::unique_lock<std::mutex> lk(usedTexturesLock);
    usedTexturesCv.wait(lk, [this] { return usedTextures.empty(); });
  }

  consumerThread.join();
  Logger::Logf(LogLevel::info,
               "Flushed! Recording session processed %d frames.",
               totalFramesProcessed);
}

FrameRecorder::~FrameRecorder() { flush(); }
