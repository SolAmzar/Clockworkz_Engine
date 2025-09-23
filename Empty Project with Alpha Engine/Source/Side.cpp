// Function to create an n-sided polygon mesh
// Input parameters: number of sides (n) and color of the polygon
#include "side.hpp"
#include <AEEngine.h>

AEGfxVertexList* CreatePolygonMesh(u32 n, u32 color) {
    // Ensure the number of sides is valid
    if (n < 3) {
        return nullptr; // A polygon must have at least 3 sides
    }

    // Start creating the mesh
    AEGfxMeshStart();

    for (u32 k = 1; k <= n; ++k) {
        // Calculate coordinates for the two vertices of the current side
        f32 xp1 = AECosDeg((180.0f * (2.0f * k - 1.0f)) / n);
        f32 yp1 = AESinDeg((180.0f * (2.0f * k - 1.0f)) / n);
        f32 xp2 = AECosDeg((180.0f * (2.0f * k + 1.0f)) / n);
        f32 yp2 = AESinDeg((180.0f * (2.0f * k + 1.0f)) / n);

        // Add vertices to the mesh
        AEGfxVertexAdd(xp1, yp1, color, 0.0f, 0.0f); // First vertex
        AEGfxVertexAdd(xp2, yp2, color, 0.0f, 0.0f); // Second vertex
        AEGfxVertexAdd(0.0f, 0.0f, color, 0.0f, 0.0f); // Origin (center of the polygon)
    }

    // End the mesh creation and return the mesh pointer
    return AEGfxMeshEnd();
}

void HandlePlayerInput(AEVec2& posPlayer) {
    if (AEInputCheckCurr(AEVK_W)) {
        posPlayer.y += 6;
    }
    if (AEInputCheckCurr(AEVK_A)) {
        posPlayer.x += -6;
    }
    if (AEInputCheckCurr(AEVK_S)) {
        posPlayer.y += -6;
    }
    if (AEInputCheckCurr(AEVK_D)) {
        posPlayer.x += 6;
    }
}

void RenderHealthBar(AEGfxVertexList* mesh, u32 pHealth, const AEVec2& position) {
    AEMtx33 scale, rotate, translate, transform;
    AEMtx33Rot(&rotate, 0);
    AEMtx33Scale(&scale, pHealth * 10.0f * AECosDeg(45), 100.0f * AESinDeg(45)); // Scaled based on health
    AEMtx33Trans(&translate, position.x, position.y);
    AEMtx33Concat(&transform, &rotate, &scale);
    AEMtx33Concat(&transform, &translate, &transform);
    AEGfxSetTransform(transform.m);
    AEGfxSetColorToAdd(0.0f, 0.0f, 1.0f, 1.0f);
    AEGfxMeshDraw(mesh, AE_GFX_MDM_TRIANGLES);
}

