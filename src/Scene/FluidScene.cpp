#include "FluidScene.h"

FluidScene::FluidScene(DXContext* context, RenderPipeline* pipeline, ComputePipeline* bilevelUniformGridCP)
    : Drawable(context, pipeline), bilevelUniformGridCP(bilevelUniformGridCP)
{
    constructScene();
}

void FluidScene::draw(Camera* camera) {

}

float getRandomFloatInRange(float min, float max) {
    return min + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (max - min)));
}

void FluidScene::constructScene() {
    unsigned int numParticles = 3 * CELLS_PER_BLOCK_EDGE * 3 * CELLS_PER_BLOCK_EDGE * 3 * CELLS_PER_BLOCK_EDGE;
    gridConstants = { numParticles, {3 * CELLS_PER_BLOCK_EDGE, 3 * CELLS_PER_BLOCK_EDGE, 3 * CELLS_PER_BLOCK_EDGE}, {0.f, 0.f, 0.f}, 0.1f };

    // Populate position data. 1000 partices in a 12x12x12 block of cells, each at a random position in a cell.
    // (Temporary, eventually, position data will come from simulation)
    for (int i = 0; i < gridConstants.gridDim.x; ++i) {
        for (int j = 0; j < gridConstants.gridDim.y; ++j) {
            for (int k = 0; k < gridConstants.gridDim.z; ++k) {
                positions.push_back({ 
                    gridConstants.minBounds.x + gridConstants.resolution * i + getRandomFloatInRange(0.f, gridConstants.resolution),
                    gridConstants.minBounds.y + gridConstants.resolution * j + getRandomFloatInRange(0.f, gridConstants.resolution),
                    gridConstants.minBounds.z + gridConstants.resolution * k + getRandomFloatInRange(0.f, gridConstants.resolution) 
                });
            }
        }
    }

    positionBuffer = StructuredBuffer(positions.data(), gridConstants.numParticles, sizeof(XMFLOAT3), bilevelUniformGridCP->getDescriptorHeap());
    positionBuffer.passSRVDataToGPU(*context, bilevelUniformGridCP->getCommandList(), bilevelUniformGridCP->getCommandListID());

    // Create cells and blocks buffers
    int numCells = gridConstants.gridDim.x * gridConstants.gridDim.y * gridConstants.gridDim.z;
    int numBlocks = numCells / (CELLS_PER_BLOCK_EDGE * CELLS_PER_BLOCK_EDGE * CELLS_PER_BLOCK_EDGE);
    
    std::vector<Cell> cells(numCells);
    std::vector<Block> blocks(numBlocks);
    
    blocksBuffer = StructuredBuffer(blocks.data(), numBlocks, sizeof(Block), bilevelUniformGridCP->getDescriptorHeap());
    blocksBuffer.passUAVDataToGPU(*context, bilevelUniformGridCP->getCommandList(), bilevelUniformGridCP->getCommandListID());

    cellsBuffer = StructuredBuffer(cells.data(), numCells, sizeof(Cell), bilevelUniformGridCP->getDescriptorHeap());
    cellsBuffer.passUAVDataToGPU(*context, bilevelUniformGridCP->getCommandList(), bilevelUniformGridCP->getCommandListID());

    // Create fence
    context->getDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
}

void FluidScene::compute() {
    auto cmdList = bilevelUniformGridCP->getCommandList();

    cmdList->SetPipelineState(bilevelUniformGridCP->getPSO());
    cmdList->SetComputeRootSignature(bilevelUniformGridCP->getRootSignature());

    // Set descriptor heap
    ID3D12DescriptorHeap* computeDescriptorHeaps[] = { bilevelUniformGridCP->getDescriptorHeap()->Get() };
    cmdList->SetDescriptorHeaps(_countof(computeDescriptorHeaps), computeDescriptorHeaps);

    // Set compute root descriptor table
    cmdList->SetComputeRootDescriptorTable(0, positionBuffer.getGPUDescriptorHandle());
    cmdList->SetComputeRootDescriptorTable(1, cellsBuffer.getGPUDescriptorHandle());
    cmdList->SetComputeRootDescriptorTable(2, blocksBuffer.getGPUDescriptorHandle());
    cmdList->SetComputeRoot32BitConstants(3, 8, &gridConstants, 0);

    // Dispatch
    int workgroupSize = (gridConstants.numParticles + BILEVEL_UNIFORM_GRID_THREADS_X - 1) / BILEVEL_UNIFORM_GRID_THREADS_X;
    cmdList->Dispatch(workgroupSize, 1, 1);

    // Execute command list
    context->executeCommandList(bilevelUniformGridCP->getCommandListID());
    context->signalAndWaitForFence(fence, fenceValue);

    // Reinitialize command list
    context->resetCommandList(bilevelUniformGridCP->getCommandListID());
}

void FluidScene::releaseResources() {
    renderPipeline->releaseResources();
}