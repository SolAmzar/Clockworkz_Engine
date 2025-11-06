// Lebon
#include "Graphics/Renderer2D.hpp"
#include "Graphics/Shader.hpp"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <Assets/ResourceManager.hpp>
#include <iostream>

// You’ll need a simple sprite shader with MVP + color + sampler2D
// For now assume you have a Shader that can be constructed/loaded elsewhere.

using namespace Graphics;

bool Renderer2D::init() {
	// unit quad (0..1), we’ll scale in shader with model or via vertex attribs; here keep it simple
		// ========== VAO 1: Unit Quad (0..1) ==========
	float vertsUnit[] = {
		// pos      // uv
		 0.f, 0.f,  0.f, 0.f,
		 1.f, 0.f,  1.f, 0.f,
		 1.f, 1.f,  1.f, 1.f,
		 0.f, 1.f,  0.f, 1.f
	};

	// ========== VAO 2: Centered Quad (-0.5..+0.5) ==========
	float vertsCentered[] = {
		// pos        // uv
		-0.5f, -0.5f,  0.f, 0.f,
		 0.5f, -0.5f,  1.f, 0.f,
		 0.5f,  0.5f,  1.f, 1.f,
		-0.5f,  0.5f,  0.f, 1.f
	};

	unsigned idx[] = { 0, 1, 2, 2, 3, 0 };

	// ========== Setup function (helper) ==========
	auto makeVAO = [](float* verts, size_t vertSize,
		unsigned* idx, size_t idxSize,
		unsigned& vao, unsigned& vbo, unsigned& ebo)
		{
			glGenVertexArrays(1, &vao);
			glGenBuffers(1, &vbo);
			glGenBuffers(1, &ebo);

			glBindVertexArray(vao);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER, vertSize, verts, GL_STATIC_DRAW);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxSize, idx, GL_STATIC_DRAW);

			glEnableVertexAttribArray(0); // pos
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(1); // uv
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

			glBindVertexArray(0);
		};

	// ========== Create both VAOs ==========
	makeVAO(vertsUnit, sizeof(vertsUnit), idx, sizeof(idx), m_vaoBottomLeft, m_vboBottomLeft, m_eboBottomLeft);
	makeVAO(vertsCentered, sizeof(vertsCentered), idx, sizeof(idx), m_vaoCenter, m_vboCenter, m_eboCenter);

	return true;

}

void Renderer2D::shutdown() {
	if (m_eboBottomLeft) glDeleteBuffers(1, &m_eboBottomLeft), m_eboBottomLeft = 0;
	if (m_vboBottomLeft) glDeleteBuffers(1, &m_vboBottomLeft), m_vboBottomLeft = 0;
	if (m_vaoBottomLeft) glDeleteVertexArrays(1, &m_vaoBottomLeft), m_vaoBottomLeft = 0;

	if (m_eboCenter) glDeleteBuffers(1, &m_eboCenter), m_eboCenter = 0;
	if (m_vboCenter) glDeleteBuffers(1, &m_vboCenter), m_vboCenter = 0;
	if (m_vaoCenter) glDeleteVertexArrays(1, &m_vaoCenter), m_vaoCenter = 0;
}

void Renderer2D::begin(const glm::mat4& vp) { m_vp = vp; }

void Renderer2D::drawQuad(
	float x, float y, float w, float h,
	int texW, int texH, unsigned texId,
	float uvx, float uvy, float uvw, float uvh,
	float r, float g, float b, float a, float angleRad,
	Anchor anchor
) {
	if (!texId) return;

	auto shader = Assets::ResourceManager<Graphics::Shader>::Get("texture");
	if (!shader) return;

	shader->use();
	shader->setMat4("uVP", m_vp);


	glm::mat4 model(1.0f);
	model = glm::translate(model, glm::vec3(x, y, 0.0f));
	if (angleRad != 0.0f) {
		model = glm::rotate(model, angleRad, glm::vec3(0.0f, 0.0f, 1.0f)); // radians!
	}
	model = glm::scale(model, glm::vec3(w, h, 1.0f));
	shader->setMat4("model", model);
	shader->setVec3("spriteColor", glm::vec3(r, g, b));

	// Pass UV info
	shader->setVec4("uvRect", glm::vec4(
		uvx / texW,          // u start
		uvy / texH,          // v start
		uvw / texW,          // u size
		uvh / texH           // v size
	));

	// Sampler
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texId);

	if (anchor == Anchor::BottomLeft) {
		glBindVertexArray(m_vaoBottomLeft);
	}
	else {
		glBindVertexArray(m_vaoCenter);
	}

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}



void Renderer2D::end() {}
