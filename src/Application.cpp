#include "Application.h"

#include "Graphics/RenderSystems/SimpleRenderSystem.h"
#include "Graphics/RenderSystems/PointLightRenderSystem.h"
#include "Graphics/Camera.h"
#include "Graphics/Buffer.h"
#include "Graphics/CameraSystem.h"
#include "Instrumentation.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/constants.hpp>

#include<array>
#include<stdexcept>
#include <cassert>
#include <chrono>
#include <iostream>

Coordinator m_Coord;

glm::vec3 red =    { 1.0f, 0.1f, 0.1f };
glm::vec3 green =  { 0.1f, 1.0f, 0.1f };
glm::vec3 blue =   { 0.1f, 0.1f, 1.0f };
glm::vec3 white =  { 1.0f, 1.0f, 1.0f };
glm::vec3 yellow = { 1.0f, 1.0f, 0.1f };
glm::vec3 purple = { 1.0f, 0.1f, 1.0f };
glm::vec3 cyan =   { 0.1f, 1.0f, 1.0f };

std::vector<std::pair<glm::vec3, float>> posAngle
{
    {{0.0f, -10.0f, -15.0f}, -0.4f},
    {{11.0f, -10.0f, -15.0f}, -0.4f},
    {{22.0f, -10.0f, -15.0f}, -0.4f},
    {{33.0f, -10.0f, -15.0f}, -0.4f},
    {{44.0f, -10.0f, -15.0f}, -0.4f},

    {{0.0f, -10.0f, -3.0f}, -0.95f},
    {{11.0f, -10.0f, -3.0f}, -0.95f},
    {{22.0f, -10.0f, -3.0f}, -0.95f},
    {{33.0f, -10.0f, -3.0f}, -0.95f},
    {{44.0f, -10.0f, -3.0f}, -0.95f},
};
int camCount = 0;
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

Application::Application()
{
    m_GlobalPool = DescriptorPool::Builder(m_Device)
        .setMaxSets(1000)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000)
        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000)
        .build();
}

Application::~Application()
{

}

void Application::Run()
{
    std::vector<std::unique_ptr<Buffer>> uboBuffers(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < uboBuffers.size(); i++) {
        uboBuffers[i] = std::make_unique<Buffer>(
            m_Device,
            sizeof(GlobalUBO),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        uboBuffers[i]->map();
    }

    std::shared_ptr<Texture> texture = std::make_shared<Texture>(m_Device, "Assets/Textures/Ground.png");

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.sampler = texture->GetSampler();
    imageInfo.imageView = texture->GetImageView();
    imageInfo.imageLayout = texture->GetImageLayout();

    auto globalSetLayout = DescriptorSetLayout::Builder(m_Device)
        .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
        .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build();

    auto materialSetLayout = DescriptorSetLayout::Builder(m_Device)
        .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build();

    std::vector<VkDescriptorSet> globalDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < globalDescriptorSets.size(); i++)
    {
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
        DescriptorWriter(*globalSetLayout, *m_GlobalPool)
            .writeBuffer(0, &bufferInfo)
            .writeImage(1, &imageInfo)
            .build(globalDescriptorSets[i]);
    }



     m_Models.push_back(std::make_shared<Model>(m_Device, "Assets/Models/Cube/Cube.gltf", *materialSetLayout, *m_GlobalPool));
     m_Models.push_back(std::make_shared<Model>(m_Device, "Assets/Models/Plane/Plane.gltf", *materialSetLayout, *m_GlobalPool));
     m_Models.push_back(std::make_shared<Model>(m_Device, "Assets/Models/Sponza/Sponza.gltf", *materialSetLayout, *m_GlobalPool));
     //m_Models.push_back(std::make_shared<Model>(m_Device, "Assets/Models/MetalRoughSpheres/MetalRoughSpheres.gltf", *materialSetLayout, *m_GlobalPool));


    m_Coord.RegisterComponent<ModelComponent>();
    m_Coord.RegisterComponent<ECSTransformComponent>();
    m_Coord.RegisterComponent<LightObjectComponent>();

    m_SetLayouts.push_back(globalSetLayout->getDescriptorSetLayout());
    m_SetLayouts.push_back(materialSetLayout->getDescriptorSetLayout());
    std::shared_ptr<SimpleRenderSystem> simpleRenderSystem = m_Coord.RegisterSystem<SimpleRenderSystem>(m_Device, m_Renderer.GetSwapChainRenderPass(), m_SetLayouts, *m_GlobalPool);
    Signature simple;
    simple.set(m_Coord.GetComponentID<ModelComponent>());
    simple.set(m_Coord.GetComponentID<ECSTransformComponent>());
    m_Coord.SetSystemSignature<SimpleRenderSystem>(simple);

    std::shared_ptr<PointLightRenderSystem> pointLightRenderSystem = m_Coord.RegisterSystem<PointLightRenderSystem>(m_Device, m_Renderer.GetSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout());
    Signature point;
    point.set(m_Coord.GetComponentID<LightObjectComponent>());
    point.set(m_Coord.GetComponentID<ECSTransformComponent>());
    m_Coord.SetSystemSignature<PointLightRenderSystem>(point);
    
    LoadGameObjects();

    CameraSystem cameraSystem{};
    cameraSystem.SetViewTarget(glm::vec3(0.0f, -8.0f, -12.0f), glm::vec3(0.0f, -0.5f, 0.0f));
    cameraSystem.UpdateEditorCameraTransform(glm::vec3(0.0f, -8.0f, -12.0f), glm::vec3(-0.40f, 0.0f, 0.0f));

    auto currenTime = std::chrono::high_resolution_clock::now();

    float limit = 300.0f;

   
    //glfwSetKeyCallback(m_AppWindow.GetWindow(), key_callback);

	while (!m_AppWindow.ShouldClose())
	{
        PROFILE_SCOPE("RunLoop");
		glfwPollEvents();

        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currenTime).count();
        currenTime = newTime;

        std::cout << "FrameTime : " << frameTime * 1000.0f << "\n";

        cameraSystem.EditorCameraInput(m_AppWindow.GetWindow(), frameTime);
        //cameraSystem.UpdateEditorCameraTransform(posAngle[camCount].first, glm::vec3(posAngle[camCount].second, 0.0f, 0.0f));
        float aspect = m_Renderer.GetAspectRatio();
        cameraSystem.SetPerspectiveProjectionEditorCam(glm::radians(60.0f), aspect, 0.1f, 100.0f);
        
       

		if (auto commandBuffer = m_Renderer.BeginFrame())
		{
            int frameIndex = m_Renderer.GetFrameIndex();
            FrameInfo frameInfo{ frameIndex, frameTime, commandBuffer, cameraSystem, globalDescriptorSets[frameIndex]};

            float lightSpd = 1.0f;
            if (glfwGetKey(m_AppWindow.GetWindow(), GLFW_KEY_L) == GLFW_PRESS)
            {
                pointLightRenderSystem->point.x -= lightSpd;
                if (pointLightRenderSystem->point.x < -limit)
                    pointLightRenderSystem->point.x = -limit;
            }

            if (glfwGetKey(m_AppWindow.GetWindow(), GLFW_KEY_J) == GLFW_PRESS)
            {
                pointLightRenderSystem->point.x += lightSpd;
                if (pointLightRenderSystem->point.x > limit)
                    pointLightRenderSystem->point.x = limit;
            }

            if (glfwGetKey(m_AppWindow.GetWindow(), GLFW_KEY_I) == GLFW_PRESS)
            {
                pointLightRenderSystem->point.z += lightSpd;
                if (pointLightRenderSystem->point.z > limit)
                    pointLightRenderSystem->point.z = limit;
            }

            if (glfwGetKey(m_AppWindow.GetWindow(), GLFW_KEY_K) == GLFW_PRESS)
            {
                pointLightRenderSystem->point.z -= lightSpd;
                if (pointLightRenderSystem->point.z < -limit)
                    pointLightRenderSystem->point.z = -limit;
            }

            if (glfwGetKey(m_AppWindow.GetWindow(), GLFW_KEY_O) == GLFW_PRESS)
            {
                pointLightRenderSystem->point.y -= lightSpd;
                if (pointLightRenderSystem->point.y < -limit)
                    pointLightRenderSystem->point.y = -limit;
            }

            if (glfwGetKey(m_AppWindow.GetWindow(), GLFW_KEY_U) == GLFW_PRESS)
            {
                pointLightRenderSystem->point.y += lightSpd;
                if (pointLightRenderSystem->point.y > limit)
                    pointLightRenderSystem->point.y = limit;
            }

            // Update
            GlobalUBO ubo {};
            ubo.cameraData.projectionMatrix = cameraSystem.GetProjection();
            ubo.cameraData.viewMatrix = cameraSystem.GetView();
            ubo.cameraData.inverseViewMatrix = cameraSystem.GetInverseView();
            pointLightRenderSystem->Update(frameInfo, ubo);
            uboBuffers[frameIndex]->writeToBuffer(&ubo);
            uboBuffers[frameIndex]->flush();
            //simpleRenderSystem->RenderShadowPass(frameInfo, ubo);
            simpleRenderSystem->RenderCascadedShadowPass(frameInfo, ubo);
            simpleRenderSystem->RenderPointShadowPass(frameInfo, ubo);
            simpleRenderSystem->RenderSpotShadowPass(frameInfo, ubo);

            // Render
			m_Renderer.BeginSwapChainRenderPass(commandBuffer);

            // order matters
            // solid objects first, then transparent
            simpleRenderSystem->RenderMainPass(frameInfo);
			pointLightRenderSystem->Render(frameInfo, ubo);

			m_Renderer.EndSwapChainRenderPass(commandBuffer);
			m_Renderer.EndFrame();
		}
	}

}

void Application::LoadGameObjects()
{

    // SPONZA SCENE ///////////////////////////////////////////////////////////////////////////////
    //Entity sponza = m_Coord.CreateEntity();
    //m_Coord.AddComponent<ECSTransformComponent>(sponza, ECSTransformComponent{ glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(glm::radians(180.0f), glm::radians(90.0f),0.0f), glm::vec3(0.01f)});
    //m_Coord.AddComponent<ModelComponent>(sponza, ModelComponent{ m_Models[2]});

    //Entity cube = m_Coord.CreateEntity();
    //m_Coord.AddComponent<ECSTransformComponent>(cube, ECSTransformComponent{ glm::vec3(-4.0f, -6.5f, 6.0f), glm::vec3(0.0f), glm::vec3(0.5f) });
    //m_Coord.AddComponent<ModelComponent>(cube, ModelComponent{ m_Models[0]});

    //Entity cubeTwo = m_Coord.CreateEntity();
    //m_Coord.AddComponent<ECSTransformComponent>(cubeTwo, ECSTransformComponent{ glm::vec3(4.5f, -6.5f, -6.0f), glm::vec3(0.0f), glm::vec3(0.5f) });
    //m_Coord.AddComponent<ModelComponent>(cubeTwo, ModelComponent{ m_Models[0] });

    //Entity pointLight = m_Coord.CreateEntity();
    //m_Coord.AddComponent<ECSTransformComponent>(pointLight, ECSTransformComponent{ glm::vec3(glm::vec4(-1.0f, -7.0f, 5.0f, 1.0f)) });
    //m_Coord.AddComponent<LightObjectComponent>(pointLight, LightObjectComponent::PointLight(cyan, 30.0f, 0.1f));

    //Entity spotLight = m_Coord.CreateEntity();
    //m_Coord.AddComponent<ECSTransformComponent>(spotLight, ECSTransformComponent{ glm::vec3(glm::vec4(5.0f, -9.0f, -6.0f, 1.0f)) });
    //m_Coord.AddComponent<LightObjectComponent>(spotLight, LightObjectComponent::SpotLight(red, 30.0f, 0.1f, glm::vec3(0.0f, 1.0f, 0.0f), 0.8978f, 0.853f));
    // SPONZA SCENE ///////////////////////////////////////////////////////////////////////////////

    float groundSize = 40.0f;
    Entity ground = m_Coord.CreateEntity();
    m_Coord.AddComponent<ECSTransformComponent>(ground, ECSTransformComponent{ glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(1.0f * groundSize, 0.05f, 1.0f * groundSize) });
    m_Coord.AddComponent<ModelComponent>(ground, ModelComponent{ m_Models[0] });


    //Room
    glm::vec3 roomDisplacement = glm::vec3(6.0f, -2.0f, 6.0f);
    float roomSize = 5.0f;

    Entity room_ground = m_Coord.CreateEntity();
    m_Coord.AddComponent<ECSTransformComponent>(room_ground, ECSTransformComponent{ glm::vec3(0.0f, 0.0f, 0.0f) + roomDisplacement, glm::vec3(0.0f), glm::vec3(1.0f * roomSize, 0.05f, 1.0f * roomSize) });
    m_Coord.AddComponent<ModelComponent>(room_ground, ModelComponent{ m_Models[0] });

    //Entity left_wall = m_Coord.CreateEntity();
    //m_Coord.AddComponent<ECSTransformComponent>(left_wall, ECSTransformComponent{ glm::vec3(-1.0f * roomSize, -1.0f * roomSize, 0.0f) + roomDisplacement, glm::vec3(0.0f), glm::vec3(0.05f, 1.0f * roomSize, 1.0f * roomSize) });
    //m_Coord.AddComponent<ModelComponent>(left_wall, ModelComponent{ m_Models[0] });

    //Entity right_wall = m_Coord.CreateEntity();
    //m_Coord.AddComponent<ECSTransformComponent>(right_wall, ECSTransformComponent{ glm::vec3(1.0f * roomSize, -1.0f * roomSize, 0.0f) + roomDisplacement, glm::vec3(0.0f), glm::vec3(0.05f, 1.0f * roomSize, 1.0f * roomSize) });
    //m_Coord.AddComponent<ModelComponent>(right_wall, ModelComponent{ m_Models[0] });

    //Entity back_wall = m_Coord.CreateEntity();
    //m_Coord.AddComponent<ECSTransformComponent>(back_wall, ECSTransformComponent{ glm::vec3(0.0f, -1.0f * roomSize, 1.0f * roomSize) + roomDisplacement, glm::vec3(0.0f), glm::vec3(1.0f * roomSize, 1.0f * roomSize, 0.05f) });
    //m_Coord.AddComponent<ModelComponent>(back_wall, ModelComponent{ m_Models[0] });

    //Entity ceiling_wall = m_Coord.CreateEntity();
    //m_Coord.AddComponent<ECSTransformComponent>(ceiling_wall, ECSTransformComponent{ glm::vec3(0.0f, -2.0f * roomSize, 0.0f) + roomDisplacement, glm::vec3(0.0f), glm::vec3(1.0f * roomSize, 0.05f, 1.0f * roomSize) });
    //m_Coord.AddComponent<ModelComponent>(ceiling_wall, ModelComponent{ m_Models[0] });



    //Room Cubes
    Entity cube = m_Coord.CreateEntity();
    m_Coord.AddComponent<ECSTransformComponent>(cube, ECSTransformComponent{ glm::vec3(-3.0f, -2.0f, 0.0f) + roomDisplacement, glm::vec3(0.0f), glm::vec3(1.0f) });
    m_Coord.AddComponent<ModelComponent>(cube, ModelComponent{ m_Models[0] });

    Entity cubeTwo = m_Coord.CreateEntity();
    m_Coord.AddComponent<ECSTransformComponent>(cubeTwo, ECSTransformComponent{ glm::vec3(2.0f, -1.0f, 0.0f) + roomDisplacement, glm::vec3(0.0f), glm::vec3(0.5f) });
    m_Coord.AddComponent<ModelComponent>(cubeTwo, ModelComponent{ m_Models[0] });


    //Room Coords
    Entity x_line = m_Coord.CreateEntity();
    m_Coord.AddComponent<ECSTransformComponent>(x_line, ECSTransformComponent{ glm::vec3(0.0f, -5.0f, 0.0f) + roomDisplacement, glm::vec3(0.0f), glm::vec3(5.0f, 0.05f, 0.05f) });
    m_Coord.AddComponent<ModelComponent>(x_line, ModelComponent{ m_Models[0] });

    Entity y_line = m_Coord.CreateEntity();
    m_Coord.AddComponent<ECSTransformComponent>(y_line, ECSTransformComponent{ glm::vec3(0.0f, -5.0f, 0.0f) + roomDisplacement, glm::vec3(0.0f, 0.0f, glm::radians(90.0f)), glm::vec3(5.0f, 0.05f, 0.05f) });
    m_Coord.AddComponent<ModelComponent>(y_line, ModelComponent{ m_Models[0] });

    Entity z_line = m_Coord.CreateEntity();
    m_Coord.AddComponent<ECSTransformComponent>(z_line, ECSTransformComponent{ glm::vec3(0.0f, -5.0f, 0.0f) + roomDisplacement, glm::vec3(0.0f, glm::radians(90.0f), 0.0f), glm::vec3(5.0f, 0.05f, 0.05f) });
    m_Coord.AddComponent<ModelComponent>(z_line, ModelComponent{ m_Models[0] });


    //Room Point Lights
    {
        Entity pointLight = m_Coord.CreateEntity();
        m_Coord.AddComponent<ECSTransformComponent>(pointLight, ECSTransformComponent{ glm::vec3(glm::vec4(-3.0f, -6.0f, -2.0f, 1.0f)) + roomDisplacement });
        m_Coord.AddComponent<LightObjectComponent>(pointLight, LightObjectComponent::PointLight(blue, 30.0f, 0.1f));

        Entity pointLightTwo = m_Coord.CreateEntity();
        m_Coord.AddComponent<ECSTransformComponent>(pointLightTwo, ECSTransformComponent{ glm::vec3(glm::vec4(3.0f, -4.0f, -2.0f, 1.0f)) + roomDisplacement });
        m_Coord.AddComponent<LightObjectComponent>(pointLightTwo, LightObjectComponent::PointLight(red, 30.0f, 0.1f));
    }

    //Room Spot Lights
    //{
    //    Entity spotLight = m_Coord.CreateEntity();
    //    m_Coord.AddComponent<ECSTransformComponent>(spotLight, ECSTransformComponent{ glm::vec3(glm::vec4(3.0f, -6.0f, 1.0f, 1.0f)) + roomDisplacement });
    //    m_Coord.AddComponent<LightObjectComponent>(spotLight, LightObjectComponent::SpotLight(red, 50.0f, 0.1f, glm::vec3(0.0f, 1.0f, 0.0f), 0.8978f, 0.853f));
    //}

}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
        camCount++;
        if (camCount >= posAngle.size())
        {
            camCount = 0;
        }
    }
}