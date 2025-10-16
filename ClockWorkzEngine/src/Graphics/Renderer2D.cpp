#include "Graphics/Renderer2D.hpp"
#include "Graphics/Shader.hpp"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <Assets/ResourceManager.hpp>
#include <iostream>

// You’ll need a simple sprite shader with MVP + color + sampler2D
// For now assume you have a Shader that can be constructed/loaded elsewhere.

using namespace Graphics;

bool Renderer2D::init() {
	// unit quad (0..1), we’ll scale in shader with model or via vertex attribs; here keep it simple
	float verts[] = {
		// pos    // uv
		 0.f, 0.f,  0.f, 0.f,
		 1.f, 0.f,  1.f, 0.f,
		 1.f, 1.f,  1.f, 1.f,
		 0.f, 1.f,  0.f, 1.f
	};
	unsigned idx[] = { 0,1,2, 2,3,0 };

	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);
	glGenBuffers(1, &m_ebo);
	glBindVertexArray(m_vao);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0); // pos
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1); // uv
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glBindVertexArray(0);

	return true;
}

void Renderer2D::shutdown() {
	if (m_ebo) glDeleteBuffers(1, &m_ebo), m_ebo = 0;
	if (m_vbo) glDeleteBuffers(1, &m_vbo), m_vbo = 0;
	if (m_vao) glDeleteVertexArrays(1, &m_vao), m_vao = 0;
}

void Renderer2D::begin(const glm::mat4& vp) { m_vp = vp; }

void Renderer2D::drawQuad(float x, float y, float w, float h,
	int texW, int texH, unsigned texId,
	float uvx, float uvy, float uvw, float uvh,
	float r, float g, float b, float a) {
	if (!texId) return;

	// Fetch and bind shader
	auto shader = Assets::ResourceManager<Graphics::Shader>::Get("texture");
	if (!shader) {
		static bool warned = false;
		if (!warned) {
			std::cerr << "[Renderer2D] Texture shader not found. Did you load it in PlayState::onEnter()?\n";
			warned = true;
		}
		return;
	}

	shader->use();

	shader->setMat4("projection", m_vp);

	// Build model transform (position + scale)
	glm::mat4 model(1.0f);
	model = glm::translate(model, glm::vec3(x, y, 0.0f));
	model = glm::scale(model, glm::vec3(w, h, 1.0f));
	shader->setMat4("model", model);
	shader->setVec3("spriteColor", glm::vec3(r, g, b));

	// Ensure sampler uses GL_TEXTURE0
	int loc = glGetUniformLocation(shader->ID, "image");
	if (loc >= 0) glUniform1i(loc, 0);


	// Bind texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texId);

	// Draw
	glBindVertexArray(m_vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void Renderer2D::end() {}
