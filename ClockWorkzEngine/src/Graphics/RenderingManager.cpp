/******************************************************************************/
/*!
\file           RenderingManager.cpp
\project        Lights Out
\author(s)      [Wei An] 100%
\brief          Implements RenderingManager, the graphics subsystem responsible
				for all debug and primitive rendering in the engine.
				Responsibilities include:
				- Initializing OpenGL resources (VAO, VBO, shaders) for rendering
				- Implementing methods to draw points, lines, rectangles, circles,
				  and other debug primitives
				- Managing frame lifecycle (beginFrame, endFrame) and camera
				- Providing visualization tools for game objects and debugging
				- Used by Engine and game states to render gameplay and debug info
				- Works with GraphicsManager for rendering state and configuration

All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Camera2D.hpp"
#include <GL/glew.h>
#include <vector>
#include <cmath>
#include <Assets/ResourceManager.hpp>
#include <Graphics/Shader.hpp>
#include "Gameplay/Physics2D.hpp"
#include "Gameplay/GameObject.hpp"

namespace Graphics {

	namespace {
		const char* debugVertexShader = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec3 aColor;
		layout(location = 2) in float aAlpha;
        uniform mat4 uProjection;
        out vec3 Color;
        void main() {
            gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
            Color = aColor;
        }
    )";

			const char* debugFragmentShader = R"(
			#version 330 core
			in vec3 Color;
			out vec4 FragColor;
			void main() {
				FragColor = vec4(Color, 1.0);
			}
		)";
	}

	// Call Stack: engine run -> engine init -> rendering manager init
	bool RenderingManager::init() {
		// Create and compile shaders
		unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vs, 1, &debugVertexShader, nullptr);
		glCompileShader(vs);

		unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fs, 1, &debugFragmentShader, nullptr);
		glCompileShader(fs);

		m_shader = glCreateProgram();
		glAttachShader(m_shader, vs);
		glAttachShader(m_shader, fs);
		glLinkProgram(m_shader);

		glDeleteShader(vs);
		glDeleteShader(fs);

		// Create buffers
		glGenVertexArrays(1, &m_vao);
		glGenBuffers(1, &m_vbo);

		m_renderer2D.init();

		// Critical GL state for 2D sprites
		glDisable(GL_DEPTH_TEST);                     // 2D: avoid depth hiding sprites
		glEnable(GL_BLEND);                           // enable alpha blending
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		return true;
	}

	void RenderingManager::drawPoint(float x, float y, float r, float g, float b, float size) {
		struct Vertex { float x, y; float r, g, b; };
		Vertex v = { x, y, r, g, b };

		glPointSize(size);
		glUseProgram(m_shader);
		glBindVertexArray(m_vao);
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(v), &v, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(float)));

		glDrawArrays(GL_POINTS, 0, 1);
	}

	void RenderingManager::drawLine(float x1, float y1, float x2, float y2, float r, float g, float b) {
		struct Vertex { float x, y; float r, g, b; };
		Vertex vertices[2] = {
			{x1, y1, r, g, b},
			{x2, y2, r, g, b}
		};

		glUseProgram(m_shader);
		glBindVertexArray(m_vao);
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(float)));

		glDrawArrays(GL_LINES, 0, 2);
	}

	void RenderingManager::drawRect(float x, float y, float w, float h, float r, float g, float b) {
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
		drawLine(right, bottom, right, top, r, g, b); // right
		drawLine(right, top, left, top, r, g, b); // top
		drawLine(left, top, left, bottom, r, g, b); // left
	}

	void RenderingManager::drawCircle(float x, float y, float radius, float r, float g, float b, int segments) {
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

	void RenderingManager::drawAABB(const glm::vec2& min, const glm::vec2& max, const glm::vec3& color) {
		drawRect(min.x, min.y, max.x - min.x, max.y - min.y, color.r, color.g, color.b);
	}

	void RenderingManager::drawVelocity(const glm::vec2& pos, const glm::vec2& vel, const glm::vec3& color) {
		drawLine(pos.x, pos.y, pos.x + vel.x, pos.y + vel.y, color.r, color.g, color.b);
	}

	//void RenderingManager::drawSprite(unsigned texId, const glm::vec2& pos,
	//	const glm::vec2& size, float angleRad, const glm::vec4& color)
	//{
	//	// hard code angle to 0 first.
	//	m_renderer2D.drawQuad(
	//		pos.x, pos.y, size.x, size.y,
	//		1, 1, texId, // texture width/height ratio
	//		0, 0, 1, 1, // uv position, uv dimension
	//		color.r, color.g, color.b, color.a, angleRad, Graphics::Anchor::Center
	//	);
	//}

	void RenderingManager::drawSprite(
		unsigned texId, const glm::vec2& pos,
		const glm::vec2& size, float angleRad, const glm::vec4& color)
	{
		m_renderer2D.submitQuad(pos, size, texId, color, angleRad, { 0,0,1,1 }, Graphics::Anchor::Center);
	}



	std::vector<glm::vec2> RenderingManager::computeVisibilityPolygon(
		const glm::vec2& origin,
		float angleCenter,
		float fov,
		float radius,
		Gameplay::Physics2D& physics,
		const std::vector<std::unique_ptr<Gameplay::GameObject>>& objs)
	{
		std::vector<glm::vec2> points;
		const int numRays = 200;
		float halfFov = fov * 0.5f;

		for (int i = 0; i < numRays; ++i) {
			float angle = angleCenter - halfFov + fov * (i / (float)(numRays - 1));
			glm::vec2 dir = { cos(angle), sin(angle) };
			auto hit = physics.raycastLight(origin, dir, radius, objs);
			points.push_back(hit.value_or(origin + dir * radius));
		}

		return points;
	}

	void RenderingManager::drawLightCone(
		const glm::vec2& origin,
		const std::vector<glm::vec2>& polygon,
		const glm::vec3& color)
	{
		std::vector<float> verts;

		// Center (brightest)
		verts.push_back(origin.x);
		verts.push_back(origin.y);
		verts.push_back(color.r);
		verts.push_back(color.g);
		verts.push_back(color.b);

		float radius = 300.0f;
		for (auto& p : polygon) {
			float dist = glm::length(p - origin);
			float alpha = glm::clamp(1.0f - dist / radius, 0.0f, 1.0f) * 0.8f;
			verts.push_back(p.x);
			verts.push_back(p.y);
			verts.push_back(color.r);
			verts.push_back(color.g);
			verts.push_back(color.b);
		}

		glBindVertexArray(m_vao);
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
		glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);

		// Position
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		// Color
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
		glEnableVertexAttribArray(1);
		// Alpha
		//glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(5 * sizeof(float)));
		//glEnableVertexAttribArray(2);

		glDrawArrays(GL_TRIANGLE_FAN, 0, polygon.size() + 1);
	}

	void RenderingManager::beginFrame() {
		glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
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

	void RenderingManager::beginScenePass(const glm::mat4& vp) {
		auto shader = Assets::ResourceManager<Graphics::Shader>::Get("texture");
		if (!shader) return;
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		shader->use();
		shader->setMat4("uVP", vp);

		int samplers[MAX_TEXTURE_SLOTS];
		for (int i = 0; i < MAX_TEXTURE_SLOTS; ++i) samplers[i] = i;
		glUniform1iv(glGetUniformLocation(shader->ID, "uTextures"), MAX_TEXTURE_SLOTS, samplers);

		m_renderer2D.beginBatch();
	}

	void RenderingManager::endScenePass() {
		m_renderer2D.flush();
	}

	void RenderingManager::beginLightPass(const glm::mat4& vp, const glm::vec2& lightPos, const glm::vec3& color, const float& radius, const float& playerRotationAngle, const float& lightAngle) {
		auto shader = Assets::ResourceManager<Graphics::Shader>::Get("light");
		if (!shader) return;
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_ONE, GL_ONE);
		shader->use();
		//shader->setMat4("uProjection", vp);
		//shader->setVec2("uLightPos", lightPos);
		//shader->setVec3("uLightColor", color);
		//shader->setFloat("uRadius", radius);

		shader->setMat4("uProjection", vp);
		shader->setVec2("uLightPos", lightPos);
		shader->setVec2("uLightDir", glm::normalize(glm::vec2(cos(playerRotationAngle), sin(playerRotationAngle))));
		shader->setFloat("uAngle", glm::radians(lightAngle));   // cone width ~60° total
		shader->setFloat("uRadius", 300.0f);
		shader->setVec3("uLightColor", glm::vec3(1.0f, 0.9f, 0.6f));
	}

	void RenderingManager::endLightPass() {
		// nothing to flush unless batching light meshes
		glDepthMask(GL_TRUE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}


	//void RenderingManager::beginFrame(const Camera2D& cam) {
	//	// Clear buffers at the start of a new frame
	//	glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // background color
	//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//	// Activate texture shader
	//	if (auto shader = Assets::ResourceManager<Graphics::Shader>::Get("texture")) {
	//		shader->use();
	//		shader->setMat4("uVP", cam.viewProj());

	//		// Ensure sampler 'image' uses GL_TEXTURE0
	//		int loc = glGetUniformLocation(shader->ID, "image");
	//		if (loc >= 0) glUniform1i(loc, 0);
	//	}

	//	// Pass VP to the 2D renderer so it can upload 'projection' when drawing
	//	m_renderer2D.begin(cam.viewProj());

	//	// Debug shader for overlays, line draw etc
	//	glUseProgram(m_shader);
	//	glUniformMatrix4fv(glGetUniformLocation(m_shader, "uProjection"), 1, GL_FALSE, &cam.viewProj()[0][0]);
	//}

	//void RenderingManager::endFrame() {
	//	glBindVertexArray(0);
	//	glUseProgram(0);
	//}

} // namespace Graphics


