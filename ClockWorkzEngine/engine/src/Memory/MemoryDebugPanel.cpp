/******************************************************************************/
/*!
\file           MemoryDebugPanel.cpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          ImGui Memory Debug Panel implementation.

                Queries MemoryDebugRegistry every frame and renders pool
                stats using ImGui tables, a progress bar, and PlotLines for
                the history graph.

                Static variables:
                  s_history[]     - circular float buffer (used KB per frame)
                  s_historyOffset - current write position in the buffer

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Memory/MemoryDebugPanel.hpp"
#include "Memory/MemoryDebugRegistry.hpp"
#include "IMGUI/imgui.h"
#include <cstdio> // snprintf

namespace Memory {

// ---------------------------------------------------------------------------
// History graph — static so it persists across frames without heap allocation.
// ---------------------------------------------------------------------------
static constexpr int   HISTORY_SIZE   = 128;
static float           s_history[HISTORY_SIZE] = {};
static int             s_historyOffset = 0;

// ---------------------------------------------------------------------------

void MemoryDebugPanel::draw() {
    // Allow the panel to be collapsed / closed without breaking the registry.
    if (!ImGui::Begin("Memory Debug Panel")) {
        ImGui::End();
        return;
    }

    auto& reg = MemoryDebugRegistry::Get();
    auto  agg = reg.aggregate();

    // -----------------------------------------------------------------------
    // Aggregate header
    // -----------------------------------------------------------------------
    ImGui::SeparatorText("Aggregate");

    ImGui::Text("Used     : %zu B  (%.2f KB / %.3f MB)",
        agg.usedBytes,
        agg.usedBytes  / 1024.0f,
        agg.usedBytes  / 1048576.0f);

    ImGui::Text("Capacity : %zu B  (%.2f KB / %.3f MB)",
        agg.totalBytes,
        agg.totalBytes / 1024.0f,
        agg.totalBytes / 1048576.0f);

    ImGui::Text("Peak     : %zu B  (%.2f KB / %.3f MB)",
        agg.peakBytes,
        agg.peakBytes  / 1024.0f,
        agg.peakBytes  / 1048576.0f);

    ImGui::Text("Blocks   : Used %zu  |  Free %zu  |  Total %zu",
        agg.usedBlocks, agg.freeBlocks, agg.usedBlocks + agg.freeBlocks);

    ImGui::Text("Pools registered: %zu", reg.poolCount());

    // -----------------------------------------------------------------------
    // Leak warning (shown in red whenever one has been detected at shutdown)
    // -----------------------------------------------------------------------
    if (reg.leakDetected()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
        ImGui::TextWrapped(
            "!! LEAK DETECTED  -  at least one pool was destroyed with "
            "unfreed blocks. Check console for details.");
        ImGui::PopStyleColor();
    }

    // -----------------------------------------------------------------------
    // Usage progress bar
    // -----------------------------------------------------------------------
    ImGui::Spacing();
    {
        float ratio = (agg.totalBytes > 0)
            ? static_cast<float>(agg.usedBytes) / static_cast<float>(agg.totalBytes)
            : 0.0f;

        char overlay[32];
        snprintf(overlay, sizeof(overlay), "%.1f%%", ratio * 100.0f);
        ImGui::ProgressBar(ratio, ImVec2(-1.0f, 0.0f), overlay);
    }

    // -----------------------------------------------------------------------
    // History graph  —  updated every frame draw() is called.
    // Plots used memory in KB over the last HISTORY_SIZE frames.
    // -----------------------------------------------------------------------
    s_history[s_historyOffset] = static_cast<float>(agg.usedBytes) / 1024.0f;
    s_historyOffset = (s_historyOffset + 1) % HISTORY_SIZE;

    ImGui::Spacing();
    ImGui::Text("Usage History (KB, last %d frames):", HISTORY_SIZE);

    float graphMax = (agg.totalBytes > 0)
        ? static_cast<float>(agg.totalBytes) / 1024.0f
        : 1.0f;

    ImGui::PlotLines(
        "##memhist",
        s_history,
        HISTORY_SIZE,
        s_historyOffset,         // offset so the graph scrolls correctly
        nullptr,                 // no overlay text on the graph itself
        0.0f,                    // y min
        graphMax,                // y max = capacity
        ImVec2(-1.0f, 70.0f));  // fill width, fixed height

    // -----------------------------------------------------------------------
    // Per-pool table
    // -----------------------------------------------------------------------
    ImGui::Spacing();
    ImGui::SeparatorText("Per-Pool Breakdown");

    constexpr ImGuiTableFlags TABLE_FLAGS =
        ImGuiTableFlags_Borders     |
        ImGuiTableFlags_RowBg       |
        ImGuiTableFlags_ScrollY     |
        ImGuiTableFlags_SizingFixedFit;

    if (ImGui::BeginTable("##pools", 6, TABLE_FLAGS, ImVec2(0.0f, 160.0f))) {
        // Freeze header row so it stays visible when scrolling.
        ImGui::TableSetupScrollFreeze(0, 1);

        ImGui::TableSetupColumn("Pool Name",  ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Blk Sz",     ImGuiTableColumnFlags_WidthFixed, 58.0f);
        ImGui::TableSetupColumn("Used",       ImGuiTableColumnFlags_WidthFixed, 48.0f);
        ImGui::TableSetupColumn("Free",       ImGuiTableColumnFlags_WidthFixed, 48.0f);
        ImGui::TableSetupColumn("Total",      ImGuiTableColumnFlags_WidthFixed, 48.0f);
        ImGui::TableSetupColumn("Peak",       ImGuiTableColumnFlags_WidthFixed, 48.0f);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < reg.poolCount(); ++i) {
            const IPoolStats* p = reg.getPool(i);
            if (!p) continue;

            ImGui::TableNextRow();

            // Pool name — highlight in red if it has live blocks and a leak
            // was detected (conservative: any pool with blocks at this moment
            // during shutdown would have triggered the warning).
            ImGui::TableSetColumnIndex(0);
            if (reg.leakDetected() && p->usedBlocks() > 0) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                    "%s", p->poolName());
            } else {
                ImGui::TextUnformatted(p->poolName());
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%zu B", p->blockSize());

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%zu", p->usedBlocks());

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%zu", p->freeBlocks());

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%zu", p->totalBlocks());

            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%zu", p->peakUsedBlocks());
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace Memory
