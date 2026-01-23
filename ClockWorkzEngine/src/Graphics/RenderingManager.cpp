/******************************************************************************/
/*!
\file           RenderingManager.cpp
\project        Lights Out
\author(s)      [Wei An] 50% [Lebon] 50%
\brief          Implements RenderingManager, the graphics subsystem responsible
                                for all debug and primitive rendering in the
engine. Responsibilities include:
                                - Initializing OpenGL resources (VAO, VBO,
shaders) for rendering
                                - Implementing methods to draw points, lines,
rectangles, circles, and other debug primitives
                                - Managing frame lifecycle (beginFrame,
endFrame) and camera
                                - Providing visualization tools for game objects
and debugging
                                - Used by Engine and game states to render
gameplay and debug info
                                - Works with GraphicsManager for rendering state
and configuration

All content � 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#include "Graphics/RenderingManager.hpp"
#include "Gameplay/GameObject.hpp"
#include "Gameplay/Physics2D.hpp"
#include "Graphics/Camera2D.hpp"
#include <Assets/ResourceManager.hpp>
#include <GL/glew.h>
#include <Graphics/Shader.hpp>
#include <Math/VecCompatGLM.hpp>
#include <cmath>
#include <vector>


namespace Graphics {
// Call Stack: engine run -> engine init -> rendering manager init
bool RenderingManager::init() {
  Assets::ResourceManager<Shader>::Load("debug", "assets/shaders/debug.vert",
                                        "assets/shaders/debug.frag");

  // Create buffers
  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_vbo);

  m_renderer2D.init();

  // allocate memoru
  // glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  // glBufferData(GL_ARRAY_BUFFER, 1000, nullptr, GL_STATIC_DRAW);
  return true;
}

void RenderingManager::drawPoint(float x, float y, float r, float g, float b,
                                 float size) {
  auto shader = Assets::ResourceManager<Graphics::Shader>::Get("debug");
  if (!shader)
    return;
  shader->use();

  struct Vertex {
    float x, y;
    float r, g, b;
  };
  Vertex v = {x, y, r, g, b};

  glPointSize(size);
  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(v), &v, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)(2 * sizeof(float)));

  glDrawArrays(GL_POINTS, 0, 1);
}

void RenderingManager::drawLine(float x1, float y1, float x2, float y2, float r,
                                float g, float b) {
  auto shader = Assets::ResourceManager<Graphics::Shader>::Get("debug");
  if (!shader)
    return;
  shader->use();

  struct Vertex {
    float x, y;
    float r, g, b;
  };
  Vertex vertices[2] = {{x1, y1, r, g, b}, {x2, y2, r, g, b}};

  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)(2 * sizeof(float)));

  glDrawArrays(GL_LINES, 0, 2);
}

void RenderingManager::drawRect(float x, float y, float w, float h, float r,
                                float g, float b) {
  // top left
  /*drawLine(x, y, x + w, y, r, g, b);
  drawLine(x + w, y, x + w, y + h, r, g, b);
  drawLine(x + w, y + h, x, y + h, r, g, b);
  drawLine(x, y + h, x, y, r, g, b);*/

  // center
  // Compute half extents
  float halfW = w * 0.5f;
  float halfH = h * 0.5f;

  // Compute corner coordinates based on center (x, y)
  float left = x - halfW;
  float right = x + halfW;
  float bottom = y - halfH;
  float top = y + halfH;

  // Draw rectangle edges (clockwise)
  drawLine(left, bottom, right, bottom, r, g, b); // bottom
  drawLine(right, bottom, right, top, r, g, b);   // right
  drawLine(right, top, left, top, r, g, b);       // top
  drawLine(left, top, left, bottom, r, g, b);     // left
}

void RenderingManager::drawCircle(float x, float y, float radius, float r,
                                  float g, float b, int segments) {
  const float step = 2.0f * 3.14159f / static_cast<float>(segments);
  for (int i = 0; i < segments; ++i) {
    float angle1 = step * i;
    float angle2 = step * (i + 1);

    float x1 = x + radius * cosf(angle1);
    float y1 = y + radius * sinf(angle1);
    float x2 = x + radius * cosf(angle2);
    float y2 = y + radius * sinf(angle2);

    drawLine(x1, y1, x2, y2, r, g, b);
  }
}

void RenderingManager::drawAABB(const glm::vec2 &min, const glm::vec2 &max,
                                const glm::vec3 &color) {
  // drawRect(min.x, min.y, max.x - min.x, max.y - min.y, color.r, color.g,
  // color.b);

  glm::vec2 size = max - min;
  glm::vec2 center = min + size * 0.5f; // middle of the AABB

  drawRect(center.x, center.y, size.x, size.y, color.r, color.g, color.b);
}

void RenderingManager::drawVelocity(const glm::vec2 &pos, const glm::vec2 &vel,
                                    const glm::vec3 &color) {
  drawLine(pos.x, pos.y, pos.x + vel.x, pos.y + vel.y, color.r, color.g,
           color.b);
}

void RenderingManager::drawSprite(unsigned texId, const glm::vec2 &pos,
                                  const glm::vec2 &size, float angleRad,
                                  const glm::vec4 &uv, const glm::vec4 &color,
                                  bool isUI) {
  /*std::cout << "draw Sprite: " << texId << std::endl;*/
  m_renderer2D.submitQuad(
      pos, size, texId, color, angleRad, uv,
      ((isUI) ? Graphics::Anchor::BottomLeft : Graphics::Anchor::Center));
}

std::vector<vec2> RenderingManager::computeVisibilityPolygon(
    const vec2 &origin, float angleCenter, float fov, float radius,
    Gameplay::Physics2D &physics,
    const std::vector<std::unique_ptr<Gameplay::GameObject>> &objs) {
  std::vector<vec2> points;

  points.push_back(origin);

  const int numRays = 200;
  float halfFov = fov * 0.5f;

  for (int i = 0; i < numRays; ++i) {
    float angle = angleCenter - halfFov + fov * (i / (float)(numRays - 1));
    vec2 dir = {cos(angle), sin(angle)};
    auto hit = physics.raycastLight(origin, dir, radius, objs);
    points.push_back(hit.value_or(origin + dir * radius));
  }

  return points;
}

void RenderingManager::drawLightCone(const glm::vec2 &origin,
                                     const std::vector<glm::vec2> &polygon,
                                     const glm::vec3 &color) const {
  std::vector<float> verts;

  // Center (brightest)
  verts.push_back(origin.x);
  verts.push_back(origin.y);
  verts.push_back(color.r);
  verts.push_back(color.g);
  verts.push_back(color.b);

  // unused variable
  // float radius = 300.0f;
  for (auto &p : polygon) {
    // float dist = glm::length(p - origin);
    // float alpha = glm::clamp(1.0f - dist / radius, 0.0f, 1.0f) * 0.8f;
    verts.push_back(p.x);
    verts.push_back(p.y);
    verts.push_back(color.r);
    verts.push_back(color.g);
    verts.push_back(color.b);
  }

  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(),
               GL_DYNAMIC_DRAW);

  // Position
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  // Color
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);
  // Alpha
  // glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
  // (void*)(5 * sizeof(float))); glEnableVertexAttribArray(2);

  glDrawArrays(GL_TRIANGLE_FAN, 0, static_cast<GLsizei>(polygon.size() + 1));
}

void RenderingManager::beginFrame() {
  glClearColor(0.05f, 0.05f, 0.15f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Normal alpha blending
  glDepthMask(GL_TRUE);
  glDisable(GL_DEPTH_TEST);
}

void RenderingManager::endFrame() {
  // Ensure any unflushed sprites are drawn
  m_renderer2D.flush();

  glBindVertexArray(0);
  glUseProgram(0);
}

void RenderingManager::beginDebugPass(const glm::mat4 &vp) {
  auto shader = Assets::ResourceManager<Graphics::Shader>::Get("debug");
  if (!shader)
    return;
  shader->use();
  shader->setMat4("uVP", vp);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
}

void RenderingManager::endDebugPass() {
  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
  glUseProgram(0);
}

void RenderingManager::beginFontPass(const glm::mat4 &vp) {
  // auto shader = Assets::ResourceManager<Shader>::Get("text");
  // shader->use();
  // glEnable(GL_BLEND);
  // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  // shader->setVec3("textColor", glm::vec3(1.0f, 1.0f, 1.0f));
  // shader->setMat4("projection", vp);

  auto shader = Assets::ResourceManager<Shader>::Get("text");
  if (!shader)
    return;
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  shader->use();
  shader->setMat4("projection", vp);
}
void RenderingManager::endFontPass() {
  //// Unbind everything font touched
  // glBindVertexArray(0);
  // glBindTexture(GL_TEXTURE_2D, 0);
  // glUseProgram(0);

  //// Reset color write and blending to sprite default
  // glEnable(GL_BLEND);
  // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  // glDisable(GL_DEPTH_TEST);
  // glDepthMask(GL_FALSE);

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glUseProgram(0);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
}

void RenderingManager::beginScenePass(const glm::mat4 &vp) {
  // auto shader = Assets::ResourceManager<Graphics::Shader>::Get("texture");
  // if (!shader) return;
  // glEnable(GL_BLEND);
  // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // shader->use();
  // shader->setMat4("uVP", vp);

  // int samplers[MAX_TEXTURE_SLOTS];
  // for (int i = 0; i < MAX_TEXTURE_SLOTS; ++i) samplers[i] = i;
  // glUniform1iv(glGetUniformLocation(shader->ID, "uTextures"),
  // MAX_TEXTURE_SLOTS, samplers);

  // m_renderer2D.beginBatch();
  ////m_renderer2D.flush();
  auto shader = Assets::ResourceManager<Graphics::Shader>::Get("texture");
  if (!shader)
    return;

  // --- Make sure we're in a known state ---
  glUseProgram(0);
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  // --- Now activate sprite shader ---
  shader->use();
  shader->setMat4("uVP", vp);

  int samplers[MAX_TEXTURE_SLOTS];
  for (int i = 0; i < MAX_TEXTURE_SLOTS; ++i)
    samplers[i] = i;
  glUniform1iv(glGetUniformLocation(shader->ID, "uTextures"), MAX_TEXTURE_SLOTS,
               samplers);

  // --- Start fresh batch ---
  // m_renderer2D.beginBatch();
}

void RenderingManager::endScenePass() {
  /*m_renderer2D.flush();
  glUseProgram(0);*/

  m_renderer2D.flush(); // upload + draw
  glBindVertexArray(0); // clear VAO
  glBindTexture(GL_TEXTURE_2D, 0);
  glUseProgram(0); // unbind shader
}

void RenderingManager::beginLightPass(
    const glm::mat4 &vp, const glm::vec2 &lightPos, const glm::vec3 &color,
    const float &radius, const float &playerRotationAngle,
    const float &lightAngle, const float &batteryFactor,
    const float &intensity) {
  auto shader = Assets::ResourceManager<Graphics::Shader>::Get("light");
  if (!shader)
    return;
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_ONE, GL_ONE);
  shader->use();

  shader->setMat4("uProjection", vp);
  shader->setVec2("uLightPos", lightPos);
  shader->setVec2("uLightDir",
                  glm::normalize(glm::vec2(cos(playerRotationAngle),
                                           sin(playerRotationAngle))));
  shader->setFloat("uAngle", glm::radians(lightAngle)); // cone width ~60° total
  shader->setFloat("uRadius", 300.0f);
  shader->setVec3("uLightColor", glm::vec3(1.0f, 0.9f, 0.6f));

  shader->setFloat("uBattery", batteryFactor);
  shader->setFloat("uIntensity", intensity);

  float flicker = 1.0f;

  if (batteryFactor <= 0.125f) {
    float low = 0.3f;
    float high = 1.4f;
    flicker = low + static_cast<float>(rand()) / RAND_MAX * (high - low);
  }

  shader->setFloat("uFlicker", flicker);
  (void)radius;
  (void)color;
}

void RenderingManager::endLightPass() {
  // nothing to flush unless batching light meshes
  glDepthMask(GL_TRUE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

} // namespace Graphics
