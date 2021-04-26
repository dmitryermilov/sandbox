
#include "pch.h"
#include "d3d9on12.h"
#include "dxva.h"
#include "dxva2api.h"
#include <iostream>
#include <vector>
#include <comdef.h>
#include <string>

struct GetMonitorRect_data {
    int current;
    int required;
    RECT requiredRect;
};
BOOL CALLBACK GetMonitorRect_MonitorEnumProc(HMONITOR /*hMonitor*/,
    HDC /*hdcMonitor*/,
    LPRECT lprcMonitor,
    LPARAM dwData)
{
    GetMonitorRect_data* data = reinterpret_cast<GetMonitorRect_data*>(dwData);
    RECT r = { 0 };
    if (NULL == lprcMonitor)
        lprcMonitor = &r;

    if (data->current == data->required)
        data->requiredRect = *lprcMonitor;
    data->current++;

    return TRUE;
}

class DeviceHandle
{
public:
    DeviceHandle(IDirect3DDeviceManager9* manager)
        : m_manager(manager)
        , m_handle(0)
    {
        if (manager)
        {
            HRESULT hr = manager->OpenDeviceHandle(&m_handle);
            if (FAILED(hr))
                m_manager = 0;
        }
    }

    ~DeviceHandle()
    {
        if (m_manager && m_handle)
            m_manager->CloseDeviceHandle(m_handle);
    }

    HANDLE Detach()
    {
        HANDLE tmp = m_handle;
        m_manager = 0;
        m_handle = 0;
        return tmp;
    }

    operator HANDLE()
    {
        return m_handle;
    }

    bool operator !() const
    {
        return m_handle == 0;
    }

protected:
    CComPtr<IDirect3DDeviceManager9> m_manager;
    HANDLE m_handle;
};

CComPtr<IDirect3D9Ex> d3d9;
CComPtr<IDirect3DDeviceManager9> deviceManager;
CComPtr<IDirectXVideoProcessorService> service;
CComPtr<IDirect3DDevice9Ex> d3d9Device;

CComPtr<IDirect3DDevice9On12> d3dOn12;
CComPtr<ID3D12Device>         d3d12;
CComPtr<ID3D12CommandAllocator>     commandAllocator;
CComPtr<ID3D12GraphicsCommandList>  commandList;
CComPtr<ID3D12CommandQueue>         queue;
CComPtr<ID3D12Fence>                fence;
UINT64                              signalValue = 1;
HANDLE                              hFenceEvent;

int initDX9Device() {
    D3D9ON12_ARGS args = {};
    args.Enable9On12 = true;

    HRESULT hr = Direct3DCreate9On12Ex(D3D_SDK_VERSION, &args, 1, &d3d9);
    if (FAILED(hr)) {
        std::cout << "Direct3DCreate9On12 failed" << hr << std::endl;
        return 1;
    }

    GetMonitorRect_data monitor = { 0 };
    monitor.required = 0;
    EnumDisplayMonitors(NULL, NULL, &GetMonitorRect_MonitorEnumProc, (LPARAM)&monitor);

    POINT point = { monitor.requiredRect.left, monitor.requiredRect.top };
    HWND  hWindow = WindowFromPoint(point);

    D3DPRESENT_PARAMETERS d3dPar = {};
    d3dPar.BackBufferFormat = (D3DFORMAT)22;
    d3dPar.BackBufferCount = 24;
    d3dPar.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dPar.hDeviceWindow = (HWND)hWindow;
    d3dPar.Windowed = 1;
    d3dPar.Flags = D3DPRESENTFLAG_VIDEO;
    d3dPar.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT | D3DPRESENT_DONOTWAIT;

    hr = d3d9->CreateDeviceEx(0,
        D3DDEVTYPE_HAL,
        (HWND)hWindow,
        D3DCREATE_HARDWARE_VERTEXPROCESSING |
        D3DCREATE_FPU_PRESERVE |
        D3DCREATE_MULTITHREADED,
        &d3dPar,
        nullptr,
        &d3d9Device);
    if (FAILED(hr)) {
        std::cout << "CreateDeviceEx failed " << hr << std::endl;
        return 1;
    }

    UINT resetToken = 0;
    hr = DXVA2CreateDirect3DDeviceManager9(&resetToken, &deviceManager);
    if (FAILED(hr)) {
        std::cout << "DXVA2CreateDirect3DDeviceManager9 failed " << hr << std::endl;
        return 1;
    }

    hr = deviceManager->ResetDevice(d3d9Device, resetToken);
    if (FAILED(hr)) {
        std::cout << "deviceManager->ResetDevice failed " << hr << std::endl;
        return 1;
    }

    DeviceHandle device = DeviceHandle(deviceManager);
    hr = deviceManager->GetVideoService(device, IID_PPV_ARGS(&service));
    if (FAILED(hr)) {
        std::cout << "deviceManager->GetVideoService failed " << hr << std::endl;
        return 1;
    }

    return 0;
}

int initDX12() {

    auto hr = d3d9Device->QueryInterface(IID_PPV_ARGS(&d3dOn12));
    if (FAILED(hr)) {
        std::cout << "QueryInterface failed " << hr << std::endl;
        return 1;
    }

    hr = d3dOn12->GetD3D12Device(IID_PPV_ARGS(&d3d12));
    if (FAILED(hr)) {
        std::cout << "GetD3D12Device failed " << hr << std::endl;
        return 1;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;  //Specifies a command buffer for copying.
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; //    Global realtime priority.
    queueDesc.NodeMask = 0; //For single GPU operation, set this to zero.
    hr = d3d12->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue));
    if (FAILED(hr)) {
        std::cout << "CreateCommandQueue failed " << hr << std::endl;
        return 1;
    }

    hr = d3d12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&commandAllocator));
    if (FAILED(hr)) {
        std::cout << "CreateCommandAllocator failed " << hr << std::endl;
        return 1;
    }

    hr = d3d12->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (FAILED(hr)) {
        std::cout << "CreateFence failed " << hr << std::endl;
        return 1;
    }

    hr = d3d12->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_COPY,
        commandAllocator,
        nullptr,
        IID_PPV_ARGS(&commandList));
    if (FAILED(hr)) {
        std::cout << "CreateCommandList failed " << hr << std::endl;
        return 1;
    }

    hr = commandList->Close();
    if (FAILED(hr)) {
        std::cout << "commandList::Close failed " << hr << std::endl;
        return 1;
    }

    hFenceEvent = CreateEvent(nullptr, false, false, nullptr);
    if (!hFenceEvent) {
        std::cout << "CreateEvent failed" << std::endl;
        return 1;
    }
    return 0;
}

int copy(IDirect3DSurface9* d3d9Dst, IDirect3DSurface9* d3d9Src) {
    std::cout << "going to copy " << d3d9Dst << " to " << d3d9Src  << std::endl;
    CComPtr <ID3D12Resource>  d3d12Src;
    HRESULT hr = d3dOn12->UnwrapUnderlyingResource(d3d9Src, queue, IID_PPV_ARGS(&d3d12Src));
    if (FAILED(hr)) {
        std::cout << "UnwrapUnderlyingResource failed " << hr << std::endl;
        return 1;
    }

    CComPtr <ID3D12Resource>  d3d12Dst;
    hr = d3dOn12->UnwrapUnderlyingResource(d3d9Dst, queue, IID_PPV_ARGS(&d3d12Dst));
    if (FAILED(hr)) {
        std::cout << "UnwrapUnderlyingResource failed " << hr << std::endl;
        return 1;
    }

    ID3D12Pageable* p[] = { d3d12Dst, d3d12Src };
    hr = d3d12->MakeResident(2, p);
    if (FAILED(hr)) {
        std::cout << "MakeResident failed " << hr << std::endl;
        return 1;
    }

    auto sdesc = d3d12Src->GetDesc();
    auto ddesc = d3d12Dst->GetDesc();

    hr = commandAllocator->Reset();
    if (FAILED(hr)) {
        std::cout << "m_commandAllocator->Reset failed " << hr << std::endl;
        return 1;
    }

    hr = commandList->Reset(commandAllocator, nullptr);
    if (FAILED(hr)) {
        std::cout << "commandList->Reset failed " << hr << std::endl;
        return 1;
    }
#if 0
    commandList->CopyResource(d3d12Dst, d3d12Src);
    if (FAILED(hr)) {
        std::cout << "commandList->CopyResource failed " << hr << std::endl;
        return 1;
    }
#else
    D3D12_TEXTURE_COPY_LOCATION srcLocation{};
    D3D12_TEXTURE_COPY_LOCATION dstLocation{};

    srcLocation.pResource = d3d12Src;
    srcLocation.SubresourceIndex = 0;
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

    dstLocation.pResource = d3d12Dst;
    dstLocation.SubresourceIndex = 1;
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

    commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

#endif

    hr = commandList->Close();
    if (FAILED(hr)) {
        std::cout << "commandList->Close failed " << hr << std::endl;
        return 1;
    }

    std::vector<ID3D12CommandList*> cmdLists = { commandList };
    queue->ExecuteCommandLists((UINT)cmdLists.size(), cmdLists.data());

    hr = queue->Signal(fence, signalValue);
    if (FAILED(hr)) {
        std::cout << "queue->Signal failed " << hr << std::endl;
        return 1;
    }

    hr = fence->SetEventOnCompletion(signalValue, hFenceEvent);
    if (FAILED(hr)) {
        std::cout << "fence->SetEventOnCompletion failed " << hr << std::endl;
        return 1;
    }

    DWORD dw = WaitForSingleObject(hFenceEvent, INFINITE);
    std::cout << "WaitForSingleObject returned " << dw << std::endl;
    ++signalValue;

    ID3D12Fence* ar1[] = { fence };
    UINT64 ar0[] = { signalValue };
    hr = d3dOn12->ReturnUnderlyingResource(d3d9Src, 1, ar0, ar1);
    if (FAILED(hr)) {
        std::cout << "d3dOn12->ReturnUnderlyingResource failed " << hr << std::endl;
        return 1;
    }

    hr = d3dOn12->ReturnUnderlyingResource(d3d9Dst, 1, ar0, ar1);
    if (FAILED(hr)) {
        std::cout << "d3dOn12->ReturnUnderlyingResource failed " << hr << std::endl;
        return 1;
    }

    hr = d3d9Device->CheckDeviceState(nullptr);
    if (FAILED(hr)) {
        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        std::wstring  s(errMsg);
        std::wcout << "d3d9Device->CheckDeviceState failed(" <<  hr << ") "<< s << std::endl;
        
        return 1;
    }

    return 0;
}

int main(int argc, char* argv[])
{
    CComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        debugController->EnableDebugLayer();

    if (initDX9Device()) {
        std::cout << "initDX9Device failed " << std::endl;
        return -1;
    }

    for (int k = 0; k < 10;++k) {

        if (initDX12()) {
            std::cout << "initDX12 failed " << std::endl;
            return -1;
        }

        std::vector<IDirect3DSurface9*> d3d9Surfaces(2);
        auto hr = service->CreateSurface(
            16000, 12000,
            (UINT)(d3d9Surfaces.size() - 1),
            (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2'),
            D3DPOOL_DEFAULT,
            0,
            DXVA2_VideoDecoderRenderTarget,
            d3d9Surfaces.data(),
            NULL);
        if (FAILED(hr)) {
            std::cout << "CreateSurface " << hr << std::endl;
            return 1;
        }

        if (copy(d3d9Surfaces[0], d3d9Surfaces[1])) {
            std::cout << "Copy failed" << std::endl;
            return -1;
        }

        commandAllocator = 0;
        commandList = 0;
        queue = 0;
        fence = 0;
        d3d12 = 0;
        d3dOn12 = 0;

        for (int i = 0; i < d3d9Surfaces.size(); ++i) {
            d3d9Surfaces[i]->Release();
        }
    }

    std::cout << "PASS" << std::endl;
    return 0;
}
