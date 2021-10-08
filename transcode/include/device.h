/******************************************************************************\
Copyright (c) 2005-2019, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#pragma once

#include <dxgi1_2.h>
#include <windows.h>
#include <d3d11.h>
#include <atlbase.h>
#include "mfxvideo++.h"

/// Base class for hw device
class IHWDevice
{
public:
    virtual ~IHWDevice() {}
    // Get handle can be used for MFX session SetHandle calls
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL* pHdl) = 0;
};

class D3D11Device : public IHWDevice
{
public:
    D3D11Device(mfxU32 nAdapterNum)
    {
        static D3D_FEATURE_LEVEL FeatureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0
        };
        D3D_FEATURE_LEVEL pFeatureLevelsOut;

        HRESULT hres = CreateDXGIFactory(__uuidof(IDXGIFactory2), (void**)(&m_pDXGIFactory));
        if (FAILED(hres))
            throw std::runtime_error("Initialize failed");

        hres = m_pDXGIFactory->EnumAdapters(nAdapterNum, &m_pAdapter);
        if (FAILED(hres))
            throw std::runtime_error("Initialize failed");

        hres = D3D11CreateDevice(m_pAdapter,
            D3D_DRIVER_TYPE_UNKNOWN,
            NULL,
            0,
            FeatureLevels,
            (sizeof(FeatureLevels) / sizeof(FeatureLevels[0])),
            D3D11_SDK_VERSION,
            &m_pD3D11Device,
            &pFeatureLevelsOut,
            &m_pD3D11Ctx);

        if (FAILED(hres))
            throw std::runtime_error("Initialize failed");

        m_pDXGIDev = m_pD3D11Device;
        m_pDX11VideoDevice = m_pD3D11Device;
        m_pVideoContext = m_pD3D11Ctx;

        // turn on multithreading for the Context
        CComQIPtr<ID3D10Multithread> p_mt(m_pVideoContext);

        if (p_mt)
            p_mt->SetMultithreadProtected(true);
        else
            throw std::runtime_error("Initialize failed");
    }

    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL* pHdl)
    {
        if (MFX_HANDLE_D3D11_DEVICE == type)
        {
            *pHdl = m_pD3D11Device.p;
            return MFX_ERR_NONE;
        }
        return MFX_ERR_UNSUPPORTED;
    }
protected:

    CComPtr<ID3D11Device>                   m_pD3D11Device;
    CComPtr<ID3D11DeviceContext>            m_pD3D11Ctx;
    CComQIPtr<ID3D11VideoDevice>            m_pDX11VideoDevice;
    CComQIPtr<ID3D11VideoContext>           m_pVideoContext;
    CComPtr<ID3D11VideoProcessorEnumerator> m_VideoProcessorEnum;

    CComQIPtr<IDXGIDevice1>                 m_pDXGIDev;
    CComQIPtr<IDXGIAdapter>                 m_pAdapter;

    CComPtr<IDXGIFactory2>                  m_pDXGIFactory;

    CComPtr<IDXGISwapChain1>                m_pSwapChain;
    CComPtr<ID3D11VideoProcessor>           m_pVideoProcessor;

private:
    CComPtr<ID3D11VideoProcessorInputView>  m_pInputViewLeft;
    CComPtr<ID3D11VideoProcessorInputView>  m_pInputViewRight;
    CComPtr<ID3D11VideoProcessorOutputView> m_pOutputView;

    CComPtr<ID3D11Texture2D>                m_pDXGIBackBuffer;
    CComPtr<ID3D11Texture2D>                m_pTempTexture;
    CComPtr<IDXGIDisplayControl>            m_pDisplayControl;
    CComPtr<IDXGIOutput>                    m_pDXGIOutput;
    mfxU16                                  m_nViews;
    BOOL                                    m_bDefaultStereoEnabled;
    BOOL                                    m_bIsA2rgb10;
    HWND                                    m_HandleWindow;
};
