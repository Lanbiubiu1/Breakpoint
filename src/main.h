#include <iostream>

#include "Support/WinInclude.h"
#include "Support/ComPointer.h"
#include "Support/Window.h"
#include "Support/Shader.h"

#include "Debug/DebugLayer.h"

#include "D3D/DXContext.h"
#include "D3D/RenderPipelineHelper.h"
#include "D3D/RenderPipeline.h"
#include "D3D/VertexBuffer.h"
#include "D3D/IndexBuffer.h"
#include "D3D/ModelMatrixBuffer.h"

#include "Scene/Camera.h"

const int voxelGridSize = 32; //voxel per axis
const float voxelGridWorldSize = 1.0f;

