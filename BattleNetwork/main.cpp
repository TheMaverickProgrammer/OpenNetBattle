#include "bnGame.h"
#include "bnGameOverScene.h"
#include "bnTitleScene.h"
#include "cxxopts/cxxopts.hpp"
#include "netplay/bnNetPlayConfig.h"

int main(int argc, char** argv) {
  DrawWindow win;
  win.Initialize(DrawWindow::WindowMode::window);
  Game game{ win };

  if (game.GetEndianness() == Endianness::big) {
    Logger::Log("System arch is Big Endian");
  }
  else {
    Logger::Log("System arch is Little Endian");
  }

  cxxopts::Options options("ONB", "Open Net Battle Engine");
  
  options.add_options()
    ("l,locale", "set flair and language to desired target", cxxopts::value<std::string>()->default_value("en"))
    ("d,debug", "Enable debugging")
    ("p,port", "port for PVP", cxxopts::value<int>()->default_value(std::to_string(NetPlayConfig::OBN_PORT)))
    ("r,remotePort", "remote port for PVP", cxxopts::value<int>()->default_value(std::to_string(NetPlayConfig::OBN_PORT)))
    ("w,cyberworld", "ip address of main hub", cxxopts::value<std::string>()->default_value("0.0.0.0"));

  // The last screen the player will ever see is the game over screen so it goes 
  // to the bottom of the stack
  // TODO: game.push<GameOverScene>();

  // Go the the title screen to kick off the rest of the app
  game.push<TitleScene>(game.Boot(options.parse(argc, argv)));

  // blocking
  game.Run();

  // finished
  return EXIT_SUCCESS;
}