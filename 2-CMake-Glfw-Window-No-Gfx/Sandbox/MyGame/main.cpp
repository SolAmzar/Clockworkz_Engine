#include <iostream>
#include <cstdlib>

#include "Game.hpp"

int main() {
    std::cout << "=== Minimal C++ Console Application ===" << std::endl;
    std::cout << "Starting application..." << std::endl;
    
    try {
        // Create a Game object
        Game myGame("My Engine", 800, 600);
        
        // Simulate some game operations
        myGame.onRun();
        
        // Clean shutdown
        myGame.onExit();
        
        std::cout << "Application completed successfully!" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}