/******************************************************************************/
/*!
\file           GameStateManager.cpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares GameStateManager, a stack-based controller for
                                managing active GameStates.
                                Responsibilities include:
                                - Pushing and popping states dynamically
                                - Updating and rendering the top-most state
                                - Supporting smooth transitions between
different states
                                - Ensuring proper cleanup when states exit

Reference: GIT

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#include "Runtime/GameStateManager.hpp"
#include "Runtime/GameState.hpp"
#include <Core/Engine.hpp>
#include <Core/Profiler.hpp>
#include <Core/Time.hpp>
#include <iostream>


using namespace Runtime;

void GameStateManager::push(std::shared_ptr<GameState> s) {
  if (!s)
    return;
  /*if (!m_states.empty()) {
          m_states.top()->onExit();
  }*/
  m_states.push(std::move(s));
  m_states.top()->onEnter();
}

void GameStateManager::pop() {

  m_pendingOp = Operation::Pop;
  m_popCount++;
  /*if (m_states.empty())
  {
          return;
  }
  m_states.top()->onExit();
  m_states.pop();*/
}

void GameStateManager::switchTo(std::shared_ptr<GameState> s) {
  if (!s)
    return;

  m_pendingOp = Operation::Switch;
  m_pendingState = std::move(s);
  /*if (!m_states.empty())
  {
          m_states.top()->onExit();
          m_states.pop();
  }
  m_states.push(std::move(s));
  m_states.top()->onEnter();*/
}

void GameStateManager::processPendingOps() {
  if (m_pendingOp == Operation::None)
    return;

  if (m_pendingOp == Operation::Pop) {
    for (int i = 0; i < m_popCount && !m_states.empty(); ++i) {
      std::cout << "[GSM] Popping state: " << m_states.top()->name() << "\n";
      m_states.top()->onExit();
      m_states.pop();
    }
    m_popCount = 0;
  } else if (m_pendingOp == Operation::Switch) {
    if (!m_states.empty()) {
      std::cout << "[GSM] Switching from: " << m_states.top()->name() << "\n";
      m_states.top()->onExit();
      m_states.pop();
    }

    if (m_pendingState) {
      std::cout << "[GSM] Switching to: " << m_pendingState->name() << "\n";
      m_states.push(m_pendingState);
      m_states.top()->onEnter();
      m_pendingState.reset();
    }
  }
  m_pendingOp = Operation::None;
}

void GameStateManager::update(double dt) {
  if (!m_states.empty())
    m_states.top()->update(dt);

  processPendingOps();
}

void GameStateManager::render(Core::Time *timer) {
  if (!m_states.empty()) {
    // Start ImGui FIRST
    Core::Engine &engine = Core::Engine::Get();
    Graphics::ImGuiManager &imgui = engine.imguiManager();
    imgui.beginFrame();

    {
      PROFILE_SCOPE("Render Game State");
      m_states.top()->render(timer);
    }

    {
      PROFILE_SCOPE("Render IMGUI");
      m_states.top()->renderImGui(timer);
    }

    imgui.endFrame();
  }
}
