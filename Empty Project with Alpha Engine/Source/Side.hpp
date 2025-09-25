#ifndef SIDE_HPP
#define SIDE_HPP


#include <AEEngine.h>

// Function to create an n-sided polygon mesh
// Parameters:
//   - n: Number of sides for the polygon (minimum 3)
//   - color: Color of the polygon in ARGB format
// Returns:
//   - A pointer to the created AEGfxVertexList, or nullptr if n is invalid
AEGfxVertexList* CreatePolygonMesh(u32 n, u32 color);

void HandlePlayerInput(AEVec2& posPlayer);

void RenderHealthBar(AEGfxVertexList* mesh, u32 pHealth, const AEVec2& position);



#endif // SIDE_HPP
