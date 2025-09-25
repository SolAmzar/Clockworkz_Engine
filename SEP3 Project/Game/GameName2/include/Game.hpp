#include <Window.hpp>
#include <memory>

class Game {
public:
    Game(std::string name, int width, int height);
    ~Game();

    void onInit();
    void onRun();
    void onExit();

private:
    std::unique_ptr<giraphics::Window> m_Window;
};
