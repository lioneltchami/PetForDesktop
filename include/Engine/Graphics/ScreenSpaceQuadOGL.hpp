#pragma once

#include "Engine/Graphics/WindowOGL.hpp"
#include <GLFW/glfw3.h>
#include <glad/glad.h>

// TODO full screen Triangle
class ScreenSpaceQuad
{
protected:
    unsigned int VBO = 0;
    unsigned int VAO = 0;
    unsigned int EBO = 0;

public:
    ScreenSpaceQuad() = default;

    ScreenSpaceQuad(const ScreenSpaceQuad&) = delete;
    ScreenSpaceQuad& operator=(const ScreenSpaceQuad&) = delete;

    ScreenSpaceQuad(ScreenSpaceQuad&& other) noexcept : VBO(other.VBO), VAO(other.VAO), EBO(other.EBO)
    {
        other.VBO = 0;
        other.VAO = 0;
        other.EBO = 0;
    }

    ScreenSpaceQuad& operator=(ScreenSpaceQuad&& other) noexcept
    {
        if (this != &other)
        {
            this->~ScreenSpaceQuad();
            VBO = other.VBO;
            VAO = other.VAO;
            EBO = other.EBO;

            other.VBO = 0;
            other.VAO = 0;
            other.EBO = 0;
        }

        return *this;
    }

    ScreenSpaceQuad(Window& win, float minPos = -1.f, float maxPos = 1.f)
    {
        // TODO: Pos vec2 ?
        float vertices[] = {
            // positions        // texture coords
            maxPos, maxPos, 0.0f, 1.0f, 1.0f, // top right
            maxPos, minPos, 0.0f, 1.0f, 0.0f, // bottom right
            minPos, minPos, 0.0f, 0.0f, 0.0f, // bottom left
            minPos, maxPos, 0.0f, 0.0f, 1.0f  // top left
        };

        unsigned int indices[] = {
            0, 1, 3, // first triangle
            1, 2, 3  // second triangle
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        // position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        // texture coord attribute
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    ~ScreenSpaceQuad()
    {
        if (VAO != 0 && glfwGetCurrentContext() != nullptr)
            glDeleteVertexArrays(1, &VAO);
        if (VBO != 0 && glfwGetCurrentContext() != nullptr)
            glDeleteBuffers(1, &VBO);
        if (EBO != 0 && glfwGetCurrentContext() != nullptr)
            glDeleteBuffers(1, &EBO);

        VBO = 0;
        VAO = 0;
        EBO = 0;
    }

    void use()
    {
        glBindVertexArray(VAO);
    }

    void draw()
    {
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
};
