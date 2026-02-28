/******************************************************************************/
/*!
\file           MemoryDebugPanel.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          ImGui Memory Debug Panel declaration.

                Provides a single static draw() function that renders a
                real-time memory usage panel sourced from MemoryDebugRegistry.

                Call once per frame between ImGui::NewFrame() and
                ImGui::Render() — e.g. inside GameStateManager::render()
                after imgui.beginFrame() and before imgui.endFrame().

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

namespace Memory {

/******************************************************************************/
/*!
\class  MemoryDebugPanel
\brief  Lightweight ImGui panel for visualising custom allocator stats.

        Features:
          - Aggregate used / capacity / peak bytes.
          - Real-time usage bar.
          - 128-sample history graph (PlotLines).
          - Scrollable per-pool table (name, block size, used, free, peak).
          - Red warning banner when a leak has been detected at shutdown.

        No heap allocations — history buffer is a static array in the .cpp.
*/
/******************************************************************************/
class MemoryDebugPanel {
public:
    /*!
    \brief  Draw the Memory Debug panel.

            Must be called between ImGui::NewFrame() and ImGui::Render()
            each frame. Safe to call even if no pools are registered.
    */
    static void draw();
};

} // namespace Memory
