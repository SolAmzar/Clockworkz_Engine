/******************************************************************************/
/*!
\file           Config.hpp
\project        Lights Out
\author(s)      [Lebon]
\brief          Declares Config, a container for engine settings.
*/
/******************************************************************************/
#pragma once
#include <string>

namespace Core {
struct Config {
  int width = 1920;
  int height = 1080;
  bool vsync = true;
  std::string title = "Lights Out";
  std::string startState = "Menu";

  bool loadFromFile(const std::string &path);
};
} // namespace Core
