/******************************************************************************/
/*!
\file           Framebuffer.hpp
\project        Lights Out
\author(s)      Syafiq [100%]
\brief          Helper class for creating framebuffers to render the game
                view into an ImGui window (for docked level editor)

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#pragma once

namespace Graphics {

    class FrameBuffer {
    public:
        FrameBuffer();
        ~FrameBuffer();

        bool create(int width, int height);

        void resize(int width, int height);

        void bind();

        void unbind();

        unsigned int getTextureID() const { return m_colorTexture; }

        int getWidth() const { return m_width; }
        int getHeight() const { return m_height; }

        void destroy();

    private:
        unsigned int m_fbo;
        unsigned int m_colorTexture;
        unsigned int m_rbo;
        int m_width;
        int m_height;
        bool m_initialized;
    };

} // namespace Graphics
