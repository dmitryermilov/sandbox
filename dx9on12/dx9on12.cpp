
#include "pch.h"
#include "d3d9on12.h"
#include "dxva.h"
#include "dxva2api.h"
#include "d3d11.h"
#include <iostream>
#include <vector>
#include <comdef.h>
#include <string>
#include <iostream>
#include <chrono>
#include <thread>
#include <dxgi1_2.h>
#include <d3d11_1.h>
#include <d3d11_4.h>

template <typename T>
static inline T mfx_print_err(T sts, const char* file, int line, const char* func)
{
    if (sts < 0)
    {
        _com_error err(sts);
        LPCTSTR errMsg = err.ErrorMessage();
        std::wstring  s(errMsg);

        std::wcout << "Failed at  line " << line << " with error " << sts << ") " << s << std::endl;
    }
    return sts;
}
#define MFX_STS_TRACE(sts) mfx_print_err(sts, __FILE__, __LINE__, __FUNCTION__)
#define MFX_RETURN(sts)         { return MFX_STS_TRACE(sts); }

#define MFX_CHECK(EXPR)    { if (FAILED(EXPR)) MFX_RETURN(EXPR); }

std::chrono::system_clock::time_point start;

int Routine(int idx, CComPtr<ID3D11Device> & device, CComPtr<ID3D11DeviceContext> & context, std::pair<CComPtr<ID3D11Texture2D>, UINT64> &texture)
{
    std::chrono::duration<double, std::milli> d = std::chrono::system_clock::now() - start;
    printf("[%f]        Child session(%d) started\n", d.count(), idx );

    CComPtr<IDXGIResource1> dxgiResource;
    CComPtr<ID3D11Texture2D> sharedTexture;
    MFX_CHECK(texture.first->QueryInterface(IID_PPV_ARGS(&dxgiResource)));

    HANDLE h;
    MFX_CHECK(dxgiResource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &h));
    printf("[%f]        Child session(%d) called CreateSharedHandle\n", d.count(), idx);

    CComPtr<ID3D11Device1> device1;
    MFX_CHECK(device->QueryInterface(IID_PPV_ARGS(&device1)));
    MFX_CHECK(device1->OpenSharedResource1(h, IID_PPV_ARGS(&sharedTexture)));
    printf("[%f]        Child session(%d) called OpenSharedResource1\n", d.count(), idx);

    CComPtr<IDXGIKeyedMutex> mutex;
    MFX_CHECK(sharedTexture->QueryInterface(IID_PPV_ARGS(&mutex)));

    d = std::chrono::system_clock::now() - start;
    printf("[%f]        Child session(%d) about to call AcquireSync(%llu)\n", d.count(), idx, texture.second);
    MFX_CHECK(mutex->AcquireSync(texture.second, INFINITE));


    D3D11_TEXTURE2D_DESC desc = { 0 };
    texture.first->GetDesc(&desc);

    desc.Width = desc.Width;
    desc.Height = desc.Height;
    desc.MipLevels = 1;
    desc.Format = desc.Format;
    desc.SampleDesc.Count = 1;
    desc.ArraySize = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;

    CComPtr<ID3D11Texture2D> staging;
    MFX_CHECK(device->CreateTexture2D(&desc, NULL, &staging));

    d = std::chrono::system_clock::now() - start;
    printf("[%f]       Child session(%d) started READ from surface \n", d.count(), idx);
    context->CopySubresourceRegion(staging, 0, 0, 0, 0, sharedTexture, 0, NULL);
    D3D11_MAPPED_SUBRESOURCE lockRect = {};
    MFX_CHECK(context->Map(staging, 0, D3D11_MAP_READ, 0, &lockRect));
    context->Unmap(staging, 0);

    d = std::chrono::system_clock::now() - start;
    printf("[%f]       Child session(%d) finished READ from surface\n", d.count(), idx);

    MFX_CHECK(mutex->ReleaseSync(texture.second));

    d = std::chrono::system_clock::now() - start;
    printf("[%f]       Child session(%d) called ReleaseSync(%llu)\n", d.count(), idx, texture.second);
}

int main(int argc, char* argv[])
{

    CComPtr<ID3D11Debug> debugController;
    HRESULT hr = S_OK;
    static D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL pFeatureLevelsOut;
    CComPtr<ID3D11Device>         masterDevice;
    CComPtr<ID3D11DeviceContext>  masterContext;

    hr = D3D11CreateDevice(nullptr,    // provide real adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_DEBUG,
        FeatureLevels,
        sizeof(FeatureLevels) / sizeof(D3D_FEATURE_LEVEL),
        D3D11_SDK_VERSION,
        &masterDevice,
        &pFeatureLevelsOut,
        &masterContext);

    CComQIPtr<ID3D10Multithread> p_mt(masterContext);
    p_mt->SetMultithreadProtected(true);

    std::vector<CComPtr<ID3D11Device>> childDevices(2);
    std::vector<CComPtr<ID3D11DeviceContext>>  child2ndContext(childDevices.size());
    std::vector <std::thread> threads(childDevices.size());

    for (size_t i = 0; i < childDevices.size(); ++i) {
        hr = D3D11CreateDevice(nullptr,    // provide real adapter
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            D3D11_CREATE_DEVICE_DEBUG,
            FeatureLevels,
            sizeof(FeatureLevels) / sizeof(D3D_FEATURE_LEVEL),
            D3D11_SDK_VERSION,
            &childDevices[i],
            &pFeatureLevelsOut,
            &child2ndContext[i]);

        CComQIPtr<ID3D10Multithread> _2_mt(child2ndContext[i]);
        _2_mt->SetMultithreadProtected(true);

    }


    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = 4096;
    desc.Height = 4096;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_NV12;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DECODER;
    desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    std::pair<CComPtr<ID3D11Texture2D>, UINT64> texture;
    hr = masterDevice->CreateTexture2D(&desc, NULL, &texture.first);

    CComPtr<IDXGIKeyedMutex> mutex;
    if (desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX)
    {
        hr = texture.first->QueryInterface(IID_PPV_ARGS(&mutex));
        hr = mutex->AcquireSync(0, INFINITE);
        texture.second++;
    }

    CComPtr<ID3D11Device5> device5;
    masterDevice->QueryInterface(IID_PPV_ARGS(&device5));
    std::pair<CComPtr<ID3D11Fence>, UINT64> fence;
    device5->CreateFence(0, D3D11_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence.first));
    auto fenceEvent = CreateEvent(nullptr, false, false, nullptr);
    fence.second = 1;

    desc = {};
    texture.first->GetDesc(&desc);

    desc.Width = desc.Width;
    desc.Height = desc.Height;
    desc.MipLevels = 1;
    desc.Format = desc.Format;
    desc.SampleDesc.Count = 1;
    desc.ArraySize = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;

    CComPtr<ID3D11Texture2D> staging;
    hr = masterDevice->CreateTexture2D(&desc, NULL, &staging);

    CComPtr<ID3D11Texture2D> staging1;
    hr = masterDevice->CreateTexture2D(&desc, NULL, &staging1);

    start = std::chrono::system_clock::now();

    D3D11_MAPPED_SUBRESOURCE lockRect = {};
    hr = masterContext->Map(staging, 0, D3D11_MAP_WRITE, 0, &lockRect);
    auto buf = (char*)lockRect.pData;
    buf = 0x0;
    masterContext->Unmap(staging, 0);

    masterContext->CopySubresourceRegion(texture.first, 0, 0, 0, 0, staging, 0, NULL);
    std::chrono::duration<double, std::milli> d = std::chrono::system_clock::now() - start;
    printf("[%f] master device started WRITE async copy\n", d.count());

    CComPtr<ID3D11DeviceContext4> masterContext4;
    MFX_CHECK(masterContext->QueryInterface(IID_PPV_ARGS(&masterContext4)));
    MFX_CHECK(masterContext4->Signal(fence.first, fence.second));
    MFX_CHECK(fence.first->SetEventOnCompletion(fence.second, fenceEvent));
    fence.second++;

    for (size_t i = 0; i < threads.size(); ++i)  threads[i] = std::thread{ Routine, (int)i, std::ref(childDevices[i]), std::ref(child2ndContext[i]), std::ref(texture) };

    WaitForSingleObject(fenceEvent, INFINITE);
    d = std::chrono::system_clock::now() - start;
    printf("[%f] master device finished WRITE and about to ReleaseSync(%llu)\n", d.count(), texture.second);
    hr = mutex->ReleaseSync(texture.second);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    d = std::chrono::system_clock::now() - start;
    printf("[%f] master device started READ copy\n", d.count());
    masterContext->CopySubresourceRegion(staging1, 0, 0, 0, 0, texture.first, 0, NULL);
    MFX_CHECK(masterContext4->Signal(fence.first, fence.second));
    MFX_CHECK(fence.first->SetEventOnCompletion(fence.second, fenceEvent));
    WaitForSingleObject(fenceEvent, INFINITE);
    hr = masterContext->Map(staging1, 0, D3D11_MAP_READ, 0, &lockRect);
    masterContext->Unmap(staging1, 0);

    d = std::chrono::system_clock::now() - start;
    printf("[%f] master device finished READ copy\n", d.count());
    
    for (size_t i = 0; i < threads.size(); ++i) threads[i].join();
     
    return 0;
}
