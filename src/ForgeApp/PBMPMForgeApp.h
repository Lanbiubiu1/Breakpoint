#pragma once

#include "Common_3/Application/Interfaces/IApp.h"
#include "Common_3/Application/Interfaces/IInput.h"
#include "Common_3/Application/Interfaces/IUI.h"
#include "Common_3/Application/Interfaces/ICameraController.h"
#include "ForgeApp/PBMPMSimulation.h"

static constexpr uint32_t kImageCount = 3;

class PBMPMForgeApp : public IApp {
public:
    PBMPMForgeApp();

    bool Init() override;
    void Exit() override;
    bool Load(ReloadDesc* pReloadDesc) override;
    void Unload(ReloadDesc* pReloadDesc) override;
    void Update(float deltaTime) override;
    void Draw() override;
    const char* GetName() override { return "PBMPMForgeApp"; }

private:
    bool initRendererResources();
    bool loadPipelineResources();
    void unloadPipelineResources();
    void updateCamera(float deltaTime);
    void updateSimulation(float deltaTime);
    void updateParticleBuffers(uint32_t frameIndex);

    Renderer*         pRenderer = nullptr;
    Queue*            pGraphicsQueue = nullptr;
    CmdPool*          pCmdPool = nullptr;
    Cmd*              ppCmds[kImageCount] = {};
    Fence*            pRenderCompleteFences[kImageCount] = {};
    Semaphore*        pImageAcquiredSemaphore = nullptr;
    Semaphore*        pRenderCompleteSemaphores[kImageCount] = {};
    SwapChain*        pSwapChain = nullptr;
    RenderTarget*     pDepthBuffer = nullptr;
    RenderTarget*     ppSwapChainRenderTargets[kImageCount] = {};
    uint32_t          mFrameIndex = 0;

    Shader*           pParticleShader = nullptr;
    RootSignature*    pParticleRootSignature = nullptr;
    Pipeline*         pParticlePipeline = nullptr;
    DescriptorSet*    pDescriptorSetUniforms = nullptr;

    Buffer*           pCameraUniformBuffers[kImageCount] = {};
    Buffer*           pSimulationUniformBuffers[kImageCount] = {};
    Buffer*           pParticleVertexBuffers[kImageCount] = {};

    AppUI             mAppUI;
    GuiComponent*     pGuiWindow = nullptr;
    CameraController* pCameraController = nullptr;

    PBMPMSimulation       mSimulation;
    PBMPMSimulationConfig mSimulationConfig;
};
