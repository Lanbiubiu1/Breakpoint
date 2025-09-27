#include "ForgeApp/PBMPMSimulation.h"

#include <cmath>
#include <random>

namespace {
struct RandomGenerator {
    std::mt19937 rng;
    std::uniform_real_distribution<float> dist{-1.0f, 1.0f};

    RandomGenerator() : rng(std::random_device{}()) {}

    float next() { return dist(rng); }
};
}

PBMPMSimulation::PBMPMSimulation() = default;

void PBMPMSimulation::initialize(const PBMPMSimulationConfig& config) {
    mConfig = config;
    mParticles.resize(config.particleCount);

    RandomGenerator generator;
    for (uint32_t i = 0; i < config.particleCount; ++i) {
        auto& particle = mParticles[i];
        const float radius = std::sqrt(generator.next() * generator.next()) * config.domainRadius;
        particle.position[0] = radius * generator.next();
        particle.position[1] = config.domainHeight * 0.5f + generator.next();
        particle.position[2] = radius * generator.next();
        particle.position[3] = 1.0f;

        particle.velocity[0] = 0.0f;
        particle.velocity[1] = 0.0f;
        particle.velocity[2] = 0.0f;
        particle.velocity[3] = 0.0f;
    }
}

void PBMPMSimulation::reset() {
    initialize(mConfig);
}

void PBMPMSimulation::step(float deltaTime) {
    applyGravity(deltaTime);
    integrate(deltaTime);
    resolveBounds();
}

void PBMPMSimulation::applyGravity(float deltaTime) {
    for (auto& particle : mParticles) {
        particle.velocity[1] += mConfig.gravity * deltaTime;
        particle.velocity[0] *= (1.0f - mConfig.damping * deltaTime);
        particle.velocity[1] *= (1.0f - mConfig.damping * deltaTime);
        particle.velocity[2] *= (1.0f - mConfig.damping * deltaTime);
    }
}

void PBMPMSimulation::integrate(float deltaTime) {
    for (auto& particle : mParticles) {
        particle.position[0] += particle.velocity[0] * deltaTime;
        particle.position[1] += particle.velocity[1] * deltaTime;
        particle.position[2] += particle.velocity[2] * deltaTime;
    }
}

void PBMPMSimulation::resolveBounds() {
    const float floorY = -mConfig.domainHeight * 0.5f;
    for (auto& particle : mParticles) {
        if (particle.position[1] < floorY) {
            particle.position[1] = floorY;
            particle.velocity[1] *= -0.5f;
        }

        const float horizontalDistance = std::sqrt(particle.position[0] * particle.position[0] +
                                                   particle.position[2] * particle.position[2]);
        if (horizontalDistance > mConfig.domainRadius) {
            const float scale = mConfig.domainRadius / (horizontalDistance + 1e-5f);
            particle.position[0] *= scale;
            particle.position[2] *= scale;
            particle.velocity[0] *= -0.3f;
            particle.velocity[2] *= -0.3f;
        }
    }
}
