#pragma once

#include "Engine/Graphics/TextureOGL.hpp"
#include "Engine/Log.hpp"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

class Framebuffer
{
protected:
    unsigned int ID = 0;

public:
    Framebuffer()
        : ID(0)
    {
        glGenFramebuffers(1, &ID);
    }

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    Framebuffer(Framebuffer&& other) noexcept : ID(other.ID)
    {
        other.ID = 0;
    }

    Framebuffer& operator=(Framebuffer&& other) noexcept
    {
        if (this != &other)
        {
            this->~Framebuffer();
            ID       = other.ID;
            other.ID = 0;
        }
        return *this;
    }

    ~Framebuffer()
    {
        if (ID != 0 && glfwGetCurrentContext() != nullptr)
            glDeleteFramebuffers(1, &ID);
        ID = 0;
    }

    void bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, ID);
    }

    void attachTexture(const Texture& texture)
    {
        // Set "renderedTexture" as our colour attachment #0
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.getID(), 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            log("Framebuffer error");
        }
    }

    static void bindScreen()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};
