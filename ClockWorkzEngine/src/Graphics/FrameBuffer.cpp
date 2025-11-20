/******************************************************************************/
/*!
\file           Framebuffer.cpp
\project        Lights Out
\author(s)      [Syafiq] 50%, [Wei An] 50%
\brief          Implementation of Framebuffer for rendering game view to texture

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#include "Graphics/Framebuffer.hpp"
#include <GL/glew.h>
#include <iostream>

namespace Graphics {

    FrameBuffer::FrameBuffer()
        : m_fbo(0)
        , m_colorTexture(0)
        , m_rbo(0)
        , m_width(0)
        , m_height(0)
        , m_initialized(false)
    {
    }

    FrameBuffer::~FrameBuffer() {
        destroy();
    }

    bool FrameBuffer::create(int width, int height) {
        if (m_initialized) {
            destroy();
        }

        m_width = width;
        m_height = height;

        //Create FrameBuffer
        glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);


        glGenTextures(1, &m_colorTexture);
        glBindTexture(GL_TEXTURE_2D, m_colorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexture, 0);

        // Create renderbuffer for depth and stencil
        glGenRenderbuffers(1, &m_rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rbo);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "[Framebuffer] Framebuffer is not complete!\n";
            destroy();
            return false;
        }

        // Unbind
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        m_initialized = true;
        return true;
    }

    void FrameBuffer::resize(int width, int height) {
        if (width <= 0 || height <= 0) return;

        m_width = width;
        m_height = height;

        if (!m_initialized) {
            create(width, height);
            return;
        }

        // Resize color texture
        glBindTexture(GL_TEXTURE_2D, m_colorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        // Resize renderbuffer
        glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    void FrameBuffer::bind() {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, m_width, m_height);
    }

    void FrameBuffer::unbind() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void FrameBuffer::destroy() {
        if (m_colorTexture) {
            glDeleteTextures(1, &m_colorTexture);
            m_colorTexture = 0;
        }
        if (m_rbo) {
            glDeleteRenderbuffers(1, &m_rbo);
            m_rbo = 0;
        }
        if (m_fbo) {
            glDeleteFramebuffers(1, &m_fbo);
            m_fbo = 0;
        }
        m_initialized = false;
    }

} // namespace Graphics
