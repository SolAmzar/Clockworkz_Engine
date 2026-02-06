/*!
\file           Renderer2D.cpp
\project        Lights Out
\author(s)      [Lebon] 90%, [Wei An] 10%
\brief          Implements a batched 2D renderer for efficient sprite drawing
                using OpenGL.

                Responsibilities:
                - Initializes and manages GPU buffers (VAO, VBO, EBO)
                - Supports dynamic batching of quads (sprites) to minimize draw
calls
                - Handles texture slot management and automatic batching flush
                - Provides submitQuad() for drawing textured or colored quads
                - Offers a simple white fallback texture for untextured
rendering
                - Integrates with Shader and ResourceManager systems

                Key methods:
                - init(): Setup OpenGL buffers and fallback texture
                - beginBatch(): Start a new draw batch
                - submitQuad(): Queue a quad for rendering
                - flush(): Upload vertex data and issue draw calls
                - shutdown(): Clean up GPU resources

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Graphics/Renderer2D.hpp"
#include "Graphics/Shader.hpp"
#include <Assets/ResourceManager.hpp>
#include <GL/glew.h>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace Graphics;

bool Renderer2D::init() {
  // Create VAO/VBO/EBO for a big dynamic batch
  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_vbo);
  glGenBuffers(1, &m_ebo);

  glBindVertexArray(m_vao);

  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, MAX_VERTS * sizeof(Vertex), nullptr,
               GL_DYNAMIC_DRAW);

  // Prebuild indices for MAX_SPRITES
  std::vector<unsigned> indices;
  indices.reserve(MAX_INDS);
  for (int i = 0; i < MAX_SPRITES; ++i) {
    unsigned base = i * 4;
    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
    indices.push_back(base + 0);
  }
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned),
               indices.data(), GL_STATIC_DRAW);

  // Layout: pos(3), uv(2), color(4), texIndex(1)
  glEnableVertexAttribArray(0); // pos
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, pos));
  glEnableVertexAttribArray(1); // uv
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, uv));
  glEnableVertexAttribArray(2); // color
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, color));
  glEnableVertexAttribArray(3); // texIndex
  glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, texIndex));

  glBindVertexArray(0);

  m_vertices.reserve(MAX_VERTS);
  // === Create 1x1 white fallback texture ===
  unsigned char whitePixel[4] = {255, 255, 255, 255};
  glGenTextures(1, &m_whiteTex);
  glBindTexture(GL_TEXTURE_2D, m_whiteTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               whitePixel);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
  return true;
}

void Renderer2D::shutdown() {
  if (m_ebo)
    glDeleteBuffers(1, &m_ebo), m_ebo = 0;
  if (m_vbo)
    glDeleteBuffers(1, &m_vbo), m_vbo = 0;
  if (m_vao)
    glDeleteVertexArrays(1, &m_vao), m_vao = 0;
  if (m_whiteTex) {
    glDeleteTextures(1, &m_whiteTex);
    m_whiteTex = 0;
  }
}

// void Renderer2D::begin(const glm::mat4& vp) {
//     std::cout << "Renderer2D begin" << std::endl;
//     m_vp = vp;
//
//     // Bind sprite shader once per frame and set VP + sampler array
//     if (auto shader =
//     Assets::ResourceManager<Graphics::Shader>::Get("texture")) {
//         shader->use();
//         shader->setMat4("uVP", m_vp);
//
//         // Set sampler array bindings [0..MAX_TEXTURE_SLOTS-1]
//         int samplers[MAX_TEXTURE_SLOTS];
//         for (int i = 0; i < MAX_TEXTURE_SLOTS; ++i) samplers[i] = i;
//         glUniform1iv(glGetUniformLocation(shader->ID, "uTextures"),
//         MAX_TEXTURE_SLOTS, samplers);
//     }
// }

// void Renderer2D::beginBatch() {
//     m_vertices.clear();
//     m_spriteCount = 0;
//     // reset texture slots
//     for (int i = 0; i < MAX_TEXTURE_SLOTS; ++i) m_textures[i] = 0;
//     m_numTextures = 0;
// }

void Renderer2D::beginBatch() {
  // std::cout << "Renderer2D beginBatch" << std::endl;
  m_vertices.clear();
  m_spriteCount = 0;
  for (int i = 0; i < MAX_TEXTURE_SLOTS; ++i)
    m_textures[i] = 0;
  m_numTextures = 0;
  m_textures[0] = m_whiteTex;
  m_numTextures = 1;
}

// returns texture slot index (0..N-1) for given GL texture ID
// int Renderer2D::findOrBindTexture(unsigned texId) {
//    if (texId == 0) return 0; // slot 0 = white fallback
//
//    for (int i = 0; i < m_numTextures; ++i)
//        if (m_textures[i] == texId) return i;
//
//    if (m_numTextures >= MAX_TEXTURE_SLOTS) {
//        flush();
//        beginBatch();
//    }
//
//    int slot = m_numTextures++;
//    if (slot < MAX_TEXTURE_SLOTS) {
//        m_textures[slot] = texId;
//    }
//    return slot;
//}

// void Renderer2D::submitQuad(
//     const glm::vec2& pos, const glm::vec2& size, unsigned texId,
//     const glm::vec4& color, float angleRad, const glm::vec4& uvRect, Anchor
//     anchor)
//{
//     // --- Compute quad corners like before ---
//     glm::vec2 half = (anchor == Anchor::Center) ? size * 0.5f :
//     glm::vec2(0.0f); glm::vec2 p0 = { -half.x, -half.y }; glm::vec2 p1 = {
//     size.x - half.x, -half.y }; glm::vec2 p2 = { size.x - half.x,  size.y -
//     half.y }; glm::vec2 p3 = { -half.x,  size.y - half.y };
//
//     auto rot = [angleRad](glm::vec2 v) {
//         if (angleRad == 0.0f) return v;
//         float c = std::cos(angleRad), s = std::sin(angleRad);
//         return glm::vec2(c * v.x - s * v.y, s * v.x + c * v.y);
//         };
//     p0 = rot(p0) + pos;
//     p1 = rot(p1) + pos;
//     p2 = rot(p2) + pos;
//     p3 = rot(p3) + pos;
//
//     float u0 = uvRect.x, v0 = uvRect.y, du = uvRect.z, dv = uvRect.w;
//
//     Vertex verts[4] = {
//         { {p0.x, p0.y, 0.f}, {u0,      v0     }, color, 0.f },
//         { {p1.x, p1.y, 0.f}, {u0 + du, v0     }, color, 0.f },
//         { {p2.x, p2.y, 0.f}, {u0 + du, v0 + dv}, color, 0.f },
//         { {p3.x, p3.y, 0.f}, {u0,      v0 + dv}, color, 0.f },
//     };
//
//     unsigned idx[6] = { 0, 1, 2, 2, 3, 0 };
//
//     // --- Draw immediately ---
//     glBindVertexArray(m_vao);
//     glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
//     glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
//
//     glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
//     glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_DYNAMIC_DRAW);
//
//     glActiveTexture(GL_TEXTURE0);
//     glBindTexture(GL_TEXTURE_2D, texId ? texId : m_whiteTex);
//
//     glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
//     glBindVertexArray(0);
// }

void Renderer2D::submitQuad(const glm::vec2 &pos, const glm::vec2 &size,
                            unsigned texId, const glm::vec4 &color,
                            float angleRad, const glm::vec4 &uvRect,
                            Anchor anchor, const glm::vec2 &pivot) {
  glm::vec2 p0, p1, p2, p3;

  // -----------------------------
  //     UI MODE (Bottom-left)
  // -----------------------------
  if (anchor == Anchor::BottomLeft) {
    // UI quad directly in pixel space (Pivot ignored for UI for now, or use if
    // needed) Usually UI pivots are anchor points, but sticking to legacy
    // behavior:
    p0 = {pos.x, pos.y};
    p1 = {pos.x + size.x, pos.y};
    p2 = {pos.x + size.x, pos.y + size.y};
    p3 = {pos.x, pos.y + size.y};
  } else {
    // -----------------------------
    //   WORLD MODE (Center anchor)
    // -----------------------------
    glm::vec2 half = size * 0.5f;

    // Apply pivot offset (pivot is relative to center)
    // If pivot is (0,0), we are centered.
    // If pivot is (w/2, 0), we want the right edge to be at (0,0) local.
    // Right edge is at +half.x. So +half.x - pivot.x = 0.
    glm::vec2 local0 = {-half.x - pivot.x, -half.y - pivot.y};
    glm::vec2 local1 = {half.x - pivot.x, -half.y - pivot.y};
    glm::vec2 local2 = {half.x - pivot.x, half.y - pivot.y};
    glm::vec2 local3 = {-half.x - pivot.x, half.y - pivot.y};

    // Optional rotation
    auto rot = [angleRad](glm::vec2 v) {
      if (angleRad == 0.0f)
        return v;
      float c = std::cos(angleRad), s = std::sin(angleRad);
      return glm::vec2(c * v.x - s * v.y, s * v.x + c * v.y);
    };

    // Add world position
    p0 = rot(local0) + pos;
    p1 = rot(local1) + pos;
    p2 = rot(local2) + pos;
    p3 = rot(local3) + pos;
  }

  // ----------------------------------
  // UV + Vertex buffer
  // ----------------------------------
  float u0 = uvRect.x, v0 = uvRect.y, du = uvRect.z, dv = uvRect.w;

  Vertex verts[4] = {
      {{p0.x, p0.y, 0.f}, {u0, v0}, color, 0.f},
      {{p1.x, p1.y, 0.f}, {u0 + du, v0}, color, 0.f},
      {{p2.x, p2.y, 0.f}, {u0 + du, v0 + dv}, color, 0.f},
      {{p3.x, p3.y, 0.f}, {u0, v0 + dv}, color, 0.f},
  };

  unsigned idx[6] = {0, 1, 2, 2, 3, 0};

  // ----------------------------------
  // Send to GPU (immediate mode)
  // ----------------------------------
  glBindVertexArray(m_vao);

  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_DYNAMIC_DRAW);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texId ? texId : m_whiteTex);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

  glBindVertexArray(0);
}

// Do not remove
// void Renderer2D::submitQuad(
//    const glm::vec2& pos, const glm::vec2& size, unsigned texId,
//    const glm::vec4& color, float angleRad, const glm::vec4& uvRect, Anchor
//    anchor)
//{
//    //std::cout << "Renderer2D submitQuad" << std::endl;
//    if (m_spriteCount >= MAX_SPRITES) {
//        flush();
//        beginBatch();
//    }
//
//    // figure anchor
//    glm::vec2 half = (anchor == Anchor::Center) ? size * 0.5f :
//    glm::vec2(0.0f);
//    // local quad corners (centered)
//    glm::vec2 p0 = { -half.x, -half.y };
//    glm::vec2 p1 = { size.x - half.x, -half.y };
//    glm::vec2 p2 = { size.x - half.x,  size.y - half.y };
//    glm::vec2 p3 = { -half.x,  size.y - half.y };
//
//    // rotate if needed
//    auto rot = [angleRad](glm::vec2 v) {
//        if (angleRad == 0.0f) return v;
//        float c = std::cos(angleRad), s = std::sin(angleRad);
//        return glm::vec2(c * v.x - s * v.y, s * v.x + c * v.y);
//        };
//
//    p0 = rot(p0) + pos;
//    p1 = rot(p1) + pos;
//    p2 = rot(p2) + pos;
//    p3 = rot(p3) + pos;
//
//    int texIndex = findOrBindTexture(texId);
//
//    // uvRect = {u0, v0, du, dv} (as you used)
//    float u0 = uvRect.x, v0 = uvRect.y, du = uvRect.z, dv = uvRect.w;
//
//    // push 4 vertices
//    m_vertices.push_back({ {p0.x, p0.y, 0.0f}, {u0,      v0     }, color,
//    (float)texIndex }); m_vertices.push_back({ {p1.x, p1.y, 0.0f}, {u0 + du,
//    v0     }, color, (float)texIndex }); m_vertices.push_back({ {p2.x, p2.y,
//    0.0f}, {u0 + du, v0 + dv}, color, (float)texIndex });
//    m_vertices.push_back({ {p3.x, p3.y, 0.0f}, {u0,      v0 + dv}, color,
//    (float)texIndex });
//
//    m_spriteCount++;
//}

void Renderer2D::flush() {
  if (m_spriteCount == 0)
    return;
  // std::cout << "Renderer2D flush" << std::endl;
  //  Upload vertex buffer for current batch
  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, m_vertices.size() * sizeof(Vertex),
                  m_vertices.data());

  // Bind all textures used in this batch
  for (int i = 0; i < m_numTextures; ++i) {
    // std::cout << "Slot " << i << " bound GL ID: " << m_textures[i] << "\n";
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, m_textures[i]);
  }

  // Draw
  glDrawElements(GL_TRIANGLES, m_spriteCount * IND_PER_SPR, GL_UNSIGNED_INT,
                 (void *)0);

  // Reset “current draw” (not strictly needed)
  glBindVertexArray(0);
}

void Renderer2D::end() {}

// #include "Graphics/Renderer2D.hpp"
// #include "Graphics/Shader.hpp"
// #include <GL/glew.h>
// #include <glm/gtc/type_ptr.hpp>
// #include <glm/gtc/matrix_transform.hpp>
// #include <Assets/ResourceManager.hpp>
// #include <iostream>
//
//// You’ll need a simple sprite shader with MVP + color + sampler2D
//// For now assume you have a Shader that can be constructed/loaded elsewhere.
//
// using namespace Graphics;
//
// bool Renderer2D::init() {
//	// unit quad (0..1), we’ll scale in shader with model or via vertex
// attribs; here keep it simple
//		// ========== VAO 1: Unit Quad (0..1) ==========
//	float vertsUnit[] = {
//		// pos      // uv
//		 0.f, 0.f,  0.f, 0.f,
//		 1.f, 0.f,  1.f, 0.f,
//		 1.f, 1.f,  1.f, 1.f,
//		 0.f, 1.f,  0.f, 1.f
//	};
//
//	// ========== VAO 2: Centered Quad (-0.5..+0.5) ==========
//	float vertsCentered[] = {
//		// pos        // uv
//		-0.5f, -0.5f,  0.f, 0.f,
//		 0.5f, -0.5f,  1.f, 0.f,
//		 0.5f,  0.5f,  1.f, 1.f,
//		-0.5f,  0.5f,  0.f, 1.f
//	};
//
//	unsigned idx[] = { 0, 1, 2, 2, 3, 0 };
//
//	// ========== Setup function (helper) ==========
//	auto makeVAO = [](float* verts, size_t vertSize,
//		unsigned* idx, size_t idxSize,
//		unsigned& vao, unsigned& vbo, unsigned& ebo)
//		{
//			glGenVertexArrays(1, &vao);
//			glGenBuffers(1, &vbo);
//			glGenBuffers(1, &ebo);
//
//			glBindVertexArray(vao);
//			glBindBuffer(GL_ARRAY_BUFFER, vbo);
//			glBufferData(GL_ARRAY_BUFFER, vertSize, verts,
// GL_STATIC_DRAW);
//
//			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
//			glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxSize, idx,
// GL_STATIC_DRAW);
//
//			glEnableVertexAttribArray(0); // pos
//			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 *
// sizeof(float), (void*)0); 			glEnableVertexAttribArray(1); //
// uv 			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
// (void*)(2 * sizeof(float)));
//
//			glBindVertexArray(0);
//		};
//
//	// ========== Create both VAOs ==========
//	makeVAO(vertsUnit, sizeof(vertsUnit), idx, sizeof(idx), m_vaoBottomLeft,
// m_vboBottomLeft, m_eboBottomLeft); 	makeVAO(vertsCentered,
// sizeof(vertsCentered), idx, sizeof(idx), m_vaoCenter, m_vboCenter,
// m_eboCenter);
//
//	return true;
//
//}
//
// void Renderer2D::shutdown() {
//	if (m_eboBottomLeft) glDeleteBuffers(1, &m_eboBottomLeft),
// m_eboBottomLeft = 0; 	if (m_vboBottomLeft) glDeleteBuffers(1,
//&m_vboBottomLeft), m_vboBottomLeft = 0; 	if (m_vaoBottomLeft)
// glDeleteVertexArrays(1, &m_vaoBottomLeft), m_vaoBottomLeft = 0;
//
//	if (m_eboCenter) glDeleteBuffers(1, &m_eboCenter), m_eboCenter = 0;
//	if (m_vboCenter) glDeleteBuffers(1, &m_vboCenter), m_vboCenter = 0;
//	if (m_vaoCenter) glDeleteVertexArrays(1, &m_vaoCenter), m_vaoCenter = 0;
//}
//
// void Renderer2D::begin(const glm::mat4& vp) { m_vp = vp; }
//
// void Renderer2D::drawQuad(
//	float x, float y, float w, float h,
//	int texW, int texH, unsigned texId,
//	float uvx, float uvy, float uvw, float uvh,
//	float r, float g, float b, float a, float angleRad,
//	Anchor anchor
//) {
//	if (!texId) return;
//
//	auto shader = Assets::ResourceManager<Graphics::Shader>::Get("texture");
//	if (!shader) return;
//
//	shader->use();
//	shader->setMat4("uVP", m_vp);
//
//
//	glm::mat4 model(1.0f);
//	model = glm::translate(model, glm::vec3(x, y, 0.0f));
//	if (angleRad != 0.0f) {
//		model = glm::rotate(model, angleRad, glm::vec3(0.0f,
// 0.0f, 1.0f)); // radians!
//	}
//	model = glm::scale(model, glm::vec3(w, h, 1.0f));
//	shader->setMat4("model", model);
//	shader->setVec3("spriteColor", glm::vec3(r, g, b));
//
//	// Pass UV info
//	shader->setVec4("uvRect", glm::vec4(
//		uvx / texW,          // u start
//		uvy / texH,          // v start
//		uvw / texW,          // u size
//		uvh / texH           // v size
//	));
//
//	// Sampler
//	glActiveTexture(GL_TEXTURE0);
//	glBindTexture(GL_TEXTURE_2D, texId);
//
//	if (anchor == Anchor::BottomLeft) {
//		glBindVertexArray(m_vaoBottomLeft);
//	}
//	else {
//		glBindVertexArray(m_vaoCenter);
//	}
//
//	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
//	glBindVertexArray(0);
//}
//
//
//
// void Renderer2D::end() {}
