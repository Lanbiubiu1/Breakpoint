#include "ForgeApp/PBMPMForgeApp.h"

#include "Common_3/Application/Interfaces/IFileSystem.h"
#include "Common_3/Application/Interfaces/ILog.h"
#include "Common_3/Application/Interfaces/IMemory.h"
#include "Common_3/Application/Interfaces/IProfiler.h"
#include "Common_3/Application/Interfaces/IUI.h"
#include "Common_3/Renderer/IRenderer.h"
#include "Common_3/Renderer/ResourceLoader/ResourceLoader.h"
#include "Common_3/Math/MathTypes.h"

#include <cstring>

static const uint32_t gParticleVertexStride = sizeof(PBMPMParticleState);

PBMPMForgeApp::PBMPMForgeApp() {
    mSimulationConfig.particleCount = 16000;
    mSimulationConfig.domainHeight = 8.0f;
    mSimulationConfig.domainRadius = 4.0f;
    mSimulationConfig.particleRadius = 0.08f;
}

bool PBMPMForgeApp::Init() {
    initMemAlloc(mSettings.mTotalMemoryBudget, mSettings.mGpuMemoryBudget);

    FileSystemInitDesc fileSystemDesc = {};
    fileSystemDesc.pAppName = GetName();
    if (!initFileSystem(&fileSystemDesc)) {
        LOGF(LogLevel::eERROR, "Failed to initialize the file system");
        return false;
    }

    Log::Init();

    if (!initRendererResources()) {
        return false;
    }

    initResourceLoaderInterface(pRenderer);

    mSimulation.initialize(mSimulationConfig);

    CameraMotionParameters camMotion{ 160.0f, 600.0f, 200.0f };
    vec3 camPos{ 0.0f, 3.0f, -12.0f };
    vec3 lookAt{ 0.0f, 1.0f, 0.0f };
    pCameraController = initFpsCameraController(camPos, lookAt);
    pCameraController->setMotionParameters(camMotion);

    InputSystemInitDesc inputDesc = {};
    inputDesc.pRenderer = pRenderer;
    inputDesc.mEnableJoystick = false;
    inputDesc.mEnableMouseCapture = true;
    initInputSystem(&inputDesc);

    if (!mAppUI.Init(pRenderer)) {
        LOGF(LogLevel::eERROR, "Failed to initialize The Forge UI layer");
        return false;
    }

    GuiDesc guiDesc = {};
    guiDesc.mStartPosition = vec2(10.0f, 10.0f);
    guiDesc.mStartSize = vec2(300.0f, 450.0f);
    pGuiWindow = mAppUI.AddGuiComponent("Simulation", &guiDesc);
    pGuiWindow->AddSliderFloat("Gravity", &mSimulationConfig.gravity, -25.0f, -0.1f);
    pGuiWindow->AddSliderFloat("Damping", &mSimulationConfig.damping, 0.0f, 1.0f);
    pGuiWindow->AddSliderFloat("Particle Radius", &mSimulationConfig.particleRadius, 0.02f, 0.3f);
    pGuiWindow->AddButton("Reset", [this]() { mSimulation.initialize(mSimulationConfig); });

    return true;
}

void PBMPMForgeApp::Exit() {
    exitInputSystem();

    unloadPipelineResources();

    if (pSwapChain) {
        removeSwapChain(pRenderer, pSwapChain);
        pSwapChain = nullptr;
    }

    if (pDepthBuffer) {
        removeRenderTarget(pRenderer, pDepthBuffer);
        pDepthBuffer = nullptr;
    }

    for (uint32_t i = 0; i < kImageCount; ++i) {
        if (ppCmds[i]) {
            removeCmd(pCmdPool, ppCmds[i]);
            ppCmds[i] = nullptr;
        }
    }

    if (pCmdPool) {
        removeCmdPool(pRenderer, pCmdPool);
        pCmdPool = nullptr;
    }

    if (pGraphicsQueue) {
        removeQueue(pRenderer, pGraphicsQueue);
        pGraphicsQueue = nullptr;
    }

    if (pRenderer) {
        removeResourceLoaderInterface(pRenderer);
        removeRenderer(pRenderer);
        pRenderer = nullptr;
    }

    mAppUI.Exit();
    exitFileSystem();
    exitMemAlloc();
}

bool PBMPMForgeApp::Load(ReloadDesc* pReloadDesc) {
    if (pReloadDesc && pReloadDesc->mType & RELOAD_TYPE_SHADER) {
        return loadPipelineResources();
    }

    ResizeDesc resizeDesc{};
    resizeDesc.mWidth = mSettings.mWidth;
    resizeDesc.mHeight = mSettings.mHeight;
    resizeSwapChain(pRenderer, pSwapChain, &resizeDesc);

    return loadPipelineResources();
}

void PBMPMForgeApp::Unload(ReloadDesc* pReloadDesc) {
    unloadPipelineResources();
}

void PBMPMForgeApp::Update(float deltaTime) {
    updateInputSystem(deltaTime, &mSettings.mWidth, &mSettings.mHeight);
    updateCamera(deltaTime);
    updateSimulation(deltaTime);
    mAppUI.Update(deltaTime);
}

void PBMPMForgeApp::Draw() {
    acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, nullptr, &mFrameIndex);

    RenderTarget* pRenderTarget = ppSwapChainRenderTargets[mFrameIndex];
    Cmd* pCmd = ppCmds[mFrameIndex];

    beginCmd(pCmd);

    TextureBarrier barrier = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET };
    cmdResourceBarrier(pCmd, 0, nullptr, 0, nullptr, 1, &barrier);

    LoadActionsDesc loadActions = {};
    loadActions.mLoadAction = LOAD_ACTION_CLEAR;
    loadActions.mClearColorValues[0] = { 0.02f, 0.02f, 0.04f, 1.0f };
    loadActions.mClearDepth.mDepth = 1.0f;
    loadActions.mClearDepth.mStencil = 0;

    RenderTarget* pTargets[] = { pRenderTarget };
    cmdBindRenderTargets(pCmd, 1, pTargets, pDepthBuffer);
    cmdSetViewport(pCmd, 0.0f, 0.0f, (float)mSettings.mWidth, (float)mSettings.mHeight, 0.0f, 1.0f);
    cmdSetScissor(pCmd, 0, 0, mSettings.mWidth, mSettings.mHeight);

    updateParticleBuffers(mFrameIndex);

    cmdBindPipeline(pCmd, pParticlePipeline);
    cmdBindDescriptorSet(pCmd, 0, pDescriptorSetUniforms);

    uint64_t vertexOffset = 0;
    cmdBindVertexBuffer(pCmd, 1, &pParticleVertexBuffers[mFrameIndex], &vertexOffset);
    cmdDraw(pCmd, mSimulation.getParticleCount(), 0);

    mAppUI.Draw(pCmd);

    cmdBindRenderTargets(pCmd, 0, nullptr, nullptr);

    barrier = { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
    cmdResourceBarrier(pCmd, 0, nullptr, 0, nullptr, 1, &barrier);

    endCmd(pCmd);

    QueueSubmitDesc submitDesc = {};
    submitDesc.mCmdCount = 1;
    submitDesc.ppCmds = &pCmd;
    submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphores[mFrameIndex];
    submitDesc.mSignalSemaphoreCount = 1;
    queueSubmit(pGraphicsQueue, &submitDesc);

    QueuePresentDesc presentDesc = {};
    presentDesc.mIndex = mFrameIndex;
    presentDesc.mWaitSemaphoreCount = 1;
    presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphores[mFrameIndex];
    presentDesc.pSwapChain = pSwapChain;
    queuePresent(pGraphicsQueue, &presentDesc);
}

bool PBMPMForgeApp::initRendererResources() {
    RendererDesc rendererDesc = {};
    rendererDesc.mShaderTarget = shader_target_6_0;

    if (!initRenderer(GetName(), &rendererDesc, &pRenderer)) {
        LOGF(LogLevel::eERROR, "Failed to initialize renderer");
        return false;
    }

    QueueDesc queueDesc = {};
    queueDesc.mType = QUEUE_TYPE_GRAPHICS;
    queueDesc.mFlag = QUEUE_FLAG_NONE;
    queueDesc.mPriority = QUEUE_PRIORITY_NORMAL;
    addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

    CmdPoolDesc poolDesc = {};
    poolDesc.pQueue = pGraphicsQueue;
    addCmdPool(pRenderer, &poolDesc, &pCmdPool);

    CmdDesc cmdDesc = {};
    cmdDesc.pPool = pCmdPool;
    for (uint32_t i = 0; i < kImageCount; ++i) {
        addCmd(pRenderer, &cmdDesc, &ppCmds[i]);
        addFence(pRenderer, &pRenderCompleteFences[i]);
        addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
    }

    addSemaphore(pRenderer, &pImageAcquiredSemaphore);

    SwapChainDesc swapChainDesc = {};
    swapChainDesc.mWidth = mSettings.mWidth;
    swapChainDesc.mHeight = mSettings.mHeight;
    swapChainDesc.mImageCount = kImageCount;
    swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true, true);
    swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
    swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
    swapChainDesc.mWindowHandle = pWindow->handle;
    addSwapChain(pRenderer, pGraphicsQueue, &swapChainDesc, &pSwapChain);

    DepthStateDesc depthDesc = {};
    depthDesc.mDepthFunc = CMP_LEQUAL;
    depthDesc.mDepthTest = true;
    depthDesc.mDepthWrite = true;

    RenderTargetDesc depthRTDesc = {};
    depthRTDesc.mArraySize = 1;
    depthRTDesc.mDepth = 1;
    depthRTDesc.mWidth = mSettings.mWidth;
    depthRTDesc.mHeight = mSettings.mHeight;
    depthRTDesc.mFormat = TinyImageFormat_D32_SFLOAT;
    depthRTDesc.mSampleCount = SAMPLE_COUNT_1;
    depthRTDesc.mClearValue.depth = 1.0f;
    depthRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
    addRenderTarget(pRenderer, &depthRTDesc, &pDepthBuffer);

    for (uint32_t i = 0; i < kImageCount; ++i) {
        ppSwapChainRenderTargets[i] = pSwapChain->ppRenderTargets[i];
    }

    return true;
}

bool PBMPMForgeApp::loadPipelineResources() {
    ShaderLoadDesc shaderDesc = {};
    shaderDesc.mStages[0].mStage = SHADER_STAGE_VERT;
    shaderDesc.mStages[0].pFileName = "Forge/Particle.vert";
    shaderDesc.mStages[1].mStage = SHADER_STAGE_FRAG;
    shaderDesc.mStages[1].pFileName = "Forge/Particle.frag";
    if (!addShader(pRenderer, &shaderDesc, &pParticleShader)) {
        LOGF(LogLevel::eERROR, "Failed to load particle shader");
        return false;
    }

    SamplerDesc samplerDesc = {};
    samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerDesc.mFilter = FILTER_LINEAR;
    Sampler* pSampler = nullptr;
    addSampler(pRenderer, &samplerDesc, &pSampler);

    RootSignatureDesc rootDesc = {};
    rootDesc.mShaderCount = 1;
    rootDesc.ppShaders = &pParticleShader;
    if (!addRootSignature(pRenderer, &rootDesc, &pParticleRootSignature)) {
        LOGF(LogLevel::eERROR, "Failed to create particle root signature");
        return false;
    }

    DescriptorSetDesc setDesc = { pParticleRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, kImageCount };
    addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetUniforms);

    GraphicsPipelineDesc pipelineDesc = {};
    pipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_POINT_LIST;
    pipelineDesc.mRenderTargetCount = 1;
    pipelineDesc.pColorFormats = &pSwapChain->ppRenderTargets[0]->mDesc.mFormat;
    pipelineDesc.mSampleCount = SAMPLE_COUNT_1;
    pipelineDesc.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
    pipelineDesc.pRootSignature = pParticleRootSignature;
    pipelineDesc.pShaderProgram = pParticleShader;
    pipelineDesc.mDepthStencilDesc.mDepthTest = true;
    pipelineDesc.mDepthStencilDesc.mDepthWrite = true;
    pipelineDesc.mDepthStencilDesc.mDepthFunc = CMP_LEQUAL;
    pipelineDesc.mRasterizerState = { CULL_MODE_NONE, FILL_MODE_SOLID, 0, 0, false, false }; // needs struct
    pipelineDesc.mBlendState = { 0 };
    VertexLayout layout = {};
    layout.mBindingCount = 1;
    layout.mBindings[0].mStride = gParticleVertexStride;
    layout.mAttribCount = 1;
    layout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
    layout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
    layout.mAttribs[0].mBinding = 0;
    layout.mAttribs[0].mLocation = 0;
    layout.mAttribs[0].mOffset = 0;
    pipelineDesc.pVertexLayout = &layout;

    if (!addPipeline(pRenderer, &pipelineDesc, &pParticlePipeline)) {
        LOGF(LogLevel::eERROR, "Failed to create particle pipeline");
        return false;
    }

    BufferLoadDesc cameraBufferDesc = {};
    cameraBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    cameraBufferDesc.mDesc.mSize = sizeof(mat4) * 2;
    cameraBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;

    BufferLoadDesc simBufferDesc = cameraBufferDesc;
    simBufferDesc.mDesc.mSize = sizeof(float4);

    BufferLoadDesc particleBufferDesc = {};
    particleBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    particleBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    particleBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    particleBufferDesc.mDesc.mSize = gParticleVertexStride * mSimulation.getParticleCount();

    for (uint32_t i = 0; i < kImageCount; ++i) {
        cameraBufferDesc.ppBuffer = &pCameraUniformBuffers[i];
        addResource(&cameraBufferDesc, nullptr);

        simBufferDesc.ppBuffer = &pSimulationUniformBuffers[i];
        addResource(&simBufferDesc, nullptr);

        particleBufferDesc.ppBuffer = &pParticleVertexBuffers[i];
        addResource(&particleBufferDesc, nullptr);

        DescriptorData params[2] = {};
        params[0].pName = "cbCamera";
        params[0].ppBuffers = &pCameraUniformBuffers[i];
        params[0].mCount = 1;
        params[1].pName = "cbSimulation";
        params[1].ppBuffers = &pSimulationUniformBuffers[i];
        params[1].mCount = 1;
        updateDescriptorSet(pRenderer, i, pDescriptorSetUniforms, 2, params);
    }

    return true;
}

void PBMPMForgeApp::unloadPipelineResources() {
    for (uint32_t i = 0; i < kImageCount; ++i) {
        if (pCameraUniformBuffers[i]) {
            removeResource(pCameraUniformBuffers[i]);
            pCameraUniformBuffers[i] = nullptr;
        }
        if (pSimulationUniformBuffers[i]) {
            removeResource(pSimulationUniformBuffers[i]);
            pSimulationUniformBuffers[i] = nullptr;
        }
        if (pParticleVertexBuffers[i]) {
            removeResource(pParticleVertexBuffers[i]);
            pParticleVertexBuffers[i] = nullptr;
        }
    }

    if (pDescriptorSetUniforms) {
        removeDescriptorSet(pRenderer, pDescriptorSetUniforms);
        pDescriptorSetUniforms = nullptr;
    }

    if (pParticlePipeline) {
        removePipeline(pRenderer, pParticlePipeline);
        pParticlePipeline = nullptr;
    }

    if (pParticleRootSignature) {
        removeRootSignature(pRenderer, pParticleRootSignature);
        pParticleRootSignature = nullptr;
    }

    if (pParticleShader) {
        removeShader(pRenderer, pParticleShader);
        pParticleShader = nullptr;
    }
}

void PBMPMForgeApp::updateCamera(float deltaTime) {
    if (pCameraController) {
        pCameraController->update(deltaTime);
    }

    mat4 view = pCameraController->getViewMatrix();
    mat4 proj = mat4::perspective(PI / 4.0f, float(mSettings.mWidth) / float(mSettings.mHeight), 0.1f, 1000.0f);
    mat4 viewProj = proj * view;

    BufferUpdateDesc updateDesc = { pCameraUniformBuffers[mFrameIndex] };
    updateDesc.mSize = sizeof(mat4) * 2;
    beginUpdateResource(&updateDesc);
    mat4* matrices = static_cast<mat4*>(updateDesc.pMappedData);
    matrices[0] = view;
    matrices[1] = viewProj;
    endUpdateResource(&updateDesc, nullptr);
}

void PBMPMForgeApp::updateSimulation(float deltaTime) {
    mSimulation.step(deltaTime);

    BufferUpdateDesc updateDesc = { pSimulationUniformBuffers[mFrameIndex] };
    updateDesc.mSize = sizeof(float4);
    beginUpdateResource(&updateDesc);
    float4* data = static_cast<float4*>(updateDesc.pMappedData);
    data[0] = float4(mSimulation.getParticleRadius(), 0.0f, 0.0f, 0.0f);
    endUpdateResource(&updateDesc, nullptr);
}

void PBMPMForgeApp::updateParticleBuffers(uint32_t frameIndex) {
    const auto& particles = mSimulation.getParticleStates();
    const size_t particleCount = particles.size();

    BufferUpdateDesc updateDesc = { pParticleVertexBuffers[frameIndex] };
    updateDesc.mSize = gParticleVertexStride * particleCount;
    beginUpdateResource(&updateDesc);
    memcpy(updateDesc.pMappedData, particles.data(), updateDesc.mSize);
    endUpdateResource(&updateDesc, nullptr);
}
