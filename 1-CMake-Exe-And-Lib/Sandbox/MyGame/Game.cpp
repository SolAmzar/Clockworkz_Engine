#include "Game.hpp"
#include <iostream>

Game::Game(std::string windowTitle, int width, int height)
{
    std::cout << "Initializing Game: " << windowTitle << " (" << width << "x" << height << ")" << std::endl;
    m_Window = std::make_unique<giraphics::Window>(windowTitle, width, height);
    onInit();
}

Game::~Game()
{
}

void Game::onInit()
{
    std::cout << "Game initialization complete!" << std::endl;
}

void Game::onRun()
{
    std::cout << "Game is running..." << std::endl;
    // Implement game loop
}

void Game::onExit()
{
    std::cout << "Game is shutting down..." << std::endl;
}