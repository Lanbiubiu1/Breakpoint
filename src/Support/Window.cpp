#include "Window.h"

bool Window::init(DXContext* contextPtr, int w, int h) {
    width = w;
    height = h;

    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_OWNDC;
    wcex.lpfnWndProc = &Window::OnWindowMessage;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = GetModuleHandle(nullptr);
    wcex.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"BreakpointWndCls";
    wcex.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    wndClass = RegisterClassExW(&wcex);
    if (wndClass == 0) {
        return false;
    }

    POINT pos{ 0, 0 };
    GetCursorPos(&pos);
    HMONITOR monitor = MonitorFromPoint(pos, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfoW(monitor, &monitorInfo);

    window = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW | WS_EX_APPWINDOW,
        (LPCWSTR)wndClass,
        L"Breakpoint",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        monitorInfo.rcWork.left + 0,
        monitorInfo.rcWork.top + 0,
        width,
        height,
        nullptr,
        nullptr,
        wcex.hInstance,
        nullptr);

    if (!window) {
        return false;
    }

    //describe swap chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = false; //3d
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0; //no MSAA
    swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = FRAME_COUNT;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapChainFullScreenDesc{};
    swapChainFullScreenDesc.Windowed = true;

    //swap chain creation
    dxContext = contextPtr;
    auto& factory = dxContext->getFactory();
    ComPointer<IDXGISwapChain1> swapChain1;
    factory->CreateSwapChainForHwnd(dxContext->getCommandQueue(), window, &swapChainDesc, &swapChainFullScreenDesc, nullptr, &swapChain1);
    if (!swapChain1.QueryInterface(swapChain)) {
        return false;
    }

    // Create RTV Heap
    D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc{};
    descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    descHeapDesc.NumDescriptors = FRAME_COUNT;
    descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    descHeapDesc.NodeMask = 0;
    if (FAILED(dxContext->getDevice()->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&rtvDescHeap)))) {
        return false;
    }

    // Create handles to view
    auto firstHandle = rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
    auto handleIncrement = dxContext->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (size_t i = 0; i < FRAME_COUNT; i++) {
        rtvHandles[i] = firstHandle;
        rtvHandles[i].ptr += handleIncrement * i;
    }

    //get buffers
    if (!getBuffers()) {
        return false;
    }

    return true;
}

void Window::update() {
    MSG msg;
    while (PeekMessageW(&msg, window, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void Window::present() {
    swapChain->Present(1, 0);
}

void Window::resize() {
    releaseBuffers();

    RECT rect;
    if (GetClientRect(window, &rect)) {
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;

        //unknown keeps old format
        swapChain->ResizeBuffers(FRAME_COUNT, width, height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
        shouldResize = false;
    }

    getBuffers();
}

void Window::beginFrame(ID3D12GraphicsCommandList6* cmdList) {
    currentSwapChainBufferIdx = swapChain->GetCurrentBackBufferIndex();

    D3D12_RESOURCE_BARRIER barrier;
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainBuffers[currentSwapChainBufferIdx];
    barrier.Transition.Subresource = 0;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

    cmdList->ResourceBarrier(1, &barrier);

    float clearColor[] = { .4f, .4f, .8f, 1.f };
    cmdList->ClearRenderTargetView(rtvHandles[currentSwapChainBufferIdx], clearColor, 0, nullptr);

    cmdList->OMSetRenderTargets(1, &rtvHandles[currentSwapChainBufferIdx], false, nullptr);
}

void Window::endFrame(ID3D12GraphicsCommandList6* cmdList) {
    D3D12_RESOURCE_BARRIER barrier;
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainBuffers[currentSwapChainBufferIdx];
    barrier.Transition.Subresource = 0;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

    cmdList->ResourceBarrier(1, &barrier);
}

void Window::shutdown() {
    releaseBuffers();

    rtvDescHeap.Release();

    swapChain.Release();

    if (window) {
        DestroyWindow(window);
    }

    if (wndClass) {
        UnregisterClassW((LPCWSTR)wndClass, GetModuleHandleW(nullptr));
    }
}

bool Window::getBuffers() {
    for (size_t i = 0; i < FRAME_COUNT; i++) {
        if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainBuffers[i])))) {
            return false;
        }

        D3D12_RENDER_TARGET_VIEW_DESC rtv{};
        rtv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtv.Texture2D.MipSlice = 0;
        rtv.Texture2D.PlaneSlice = 0;
        dxContext->getDevice()->CreateRenderTargetView(swapChainBuffers[i], &rtv, rtvHandles[i]);
    }
    return true;
}

void Window::releaseBuffers() {
    for (size_t i = 0; i < FRAME_COUNT; i++) {
        swapChainBuffers[i].Release();
    }
}

LRESULT Window::OnWindowMessage(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            //only resize if size is not 0 and size has been changed from expected size
            if (lParam && (LOWORD(lParam) != get().width || HIWORD(lParam) != get().height)) {
                get().shouldResize = true;
            }
            break;
        case WM_CLOSE:
            get().shouldClose = true;
            return 0;
        case WM_ACTIVATEAPP:
            DirectX::Keyboard::ProcessMessage(msg, wParam, lParam);
            DirectX::Mouse::ProcessMessage(msg, wParam, lParam);
            break;
        case WM_ACTIVATE:
        case WM_INPUT:
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHOVER:
            DirectX::Mouse::ProcessMessage(msg, wParam, lParam);
            break;
        case WM_MOUSEACTIVATE:
            //ignore first click when returning to window
            return MA_ACTIVATEANDEAT;
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            DirectX::Keyboard::ProcessMessage(msg, wParam, lParam);
            break;
        case WM_SYSKEYDOWN:
            DirectX::Keyboard::ProcessMessage(msg, wParam, lParam);
            if (wParam == VK_RETURN && (lParam & 0x60000000) == 0x20000000) {}
            break;
        case WM_CHAR:
            switch (wParam)
                case VK_ESCAPE:
                    get().shouldClose = true;
                    DirectX::Keyboard::ProcessMessage(msg, wParam, lParam);
                    return 0;
    }
    return DefWindowProc(wnd, msg, wParam, lParam);
}
