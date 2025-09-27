#pragma once

#include <vector>
#include <cstdint>

struct PBMPMSimulationConfig {
    uint32_t particleCount = 10000;
    float    simulationScale = 10.0f;
    float    particleRadius = 0.2f;
    float    gravity = -9.81f;
    float    damping = 0.1f;
    float    domainHeight = 10.0f;
    float    domainRadius = 5.0f;
};

struct PBMPMParticleState {
    float position[4];
    float velocity[4];
};

class PBMPMSimulation {
public:
    PBMPMSimulation();

    void initialize(const PBMPMSimulationConfig& config);
    void reset();
    void step(float deltaTime);

    const std::vector<PBMPMParticleState>& getParticleStates() const { return mParticles; }
    uint32_t getParticleCount() const { return static_cast<uint32_t>(mParticles.size()); }
    float getParticleRadius() const { return mConfig.particleRadius; }

private:
    void applyGravity(float deltaTime);
    void integrate(float deltaTime);
    void resolveBounds();

    PBMPMSimulationConfig mConfig;
    std::vector<PBMPMParticleState> mParticles;
};
