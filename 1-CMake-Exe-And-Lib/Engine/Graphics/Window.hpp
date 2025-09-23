#pragma once

#include <string>
namespace giraphics {

class Window {
 public:
     Window(const std::string& windowTitle, int width = 800, int height = 600);
     ~Window();

     Window(const Window&) = delete;
     Window&operator=(const Window&) = delete;
};

}  // namespace giraphics
