#include "MeshPipeline.h"

MeshPipeline::MeshPipeline(std::string meshShaderName, std::string fragShaderName, std::string rootSignatureShaderName, DXContext& context,
    CommandListID cmdID, D3D12_DESCRIPTOR_HEAP_TYPE type, unsigned int numberOfDescriptors, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
	: Pipeline(rootSignatureShaderName, context, cmdID, type, numberOfDescriptors, flags), meshShader(meshShaderName), fragShader(fragShaderName)
{
    // TODO: this should be in the base pipeline class (same for compute pipeline)
    createPSOD();
    createPipelineState(context.getDevice());
}

void MeshPipeline::createPSOD() {
    psod.pRootSignature = rootSignature;
    psod.MS.BytecodeLength = meshShader.getSize();
    psod.MS.pShaderBytecode = meshShader.getBuffer();
    psod.PS.BytecodeLength = fragShader.getSize();
    psod.PS.pShaderBytecode = fragShader.getBuffer();

    // Primitive topology type for mesh pipelines
    psod.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    // Render target formats
    psod.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psod.NumRenderTargets = 1;

    // Depth-stencil format
    psod.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    // Other states
    psod.SampleDesc.Count = 1;
    psod.SampleMask = UINT_MAX;
    psod.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psod.RasterizerState.FrontCounterClockwise = TRUE;
    psod.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psod.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psod.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
}

void MeshPipeline::createPipelineState(ComPointer<ID3D12Device6> device) {
    CD3DX12_PIPELINE_MESH_STATE_STREAM psoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(psod);
    D3D12_PIPELINE_STATE_STREAM_DESC streamDesc = { sizeof(psoStream), &psoStream };
    HRESULT hr = device->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&pso));

    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create compute pipeline state");
    }
}
