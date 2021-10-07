// Copyright (c) 2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <stdexcept>
#include "mfxvideo++.h"
#include "mfxjpeg.h"
#include "mfxvp8.h"
#include "mfxplugin.h"

#include "common_utils.h"

class Transcoder
{
public:
    Transcoder()
    {
        mfxIMPL impl = MFX_IMPL_HARDWARE;
        mfxVersion ver = { {0, 1} };

        _session.reset(new MFXVideoSession());

        auto sts = Initialize(impl, ver, _session.get(), &_allocator);
        if (sts != MFX_ERR_NONE)
            throw std::runtime_error("Initialize failed");

        // Create Media SDK decoder & encoder
        _decode.reset(new MFXVideoDECODE(_session->operator mfxSession() ));
        _encode.reset(new MFXVideoENCODE(_session->operator mfxSession()));
    }
    virtual ~Transcoder()
    {
        _encode.reset(nullptr);
        _decode.reset(nullptr);
        _session.reset(nullptr);

        _allocator.Free(_allocator.pthis, &_allocResponse);

        Release();
    }

    bool IsDrainCompleted()
    {
        return !!(_decodeEOS && _encodeEOS);
    }

    mfxBitstream* Process(mfxBitstream* inBs, mfxBitstream& outBs)
    {
        if (!_initialized)
        {
            if (!inBs)
                throw std::logic_error("Attempt to train not initialized decoder");
            InternalInit(*inBs);
        }

        auto DecodeFrame = [&](mfxBitstream* bs) -> mfxFrameSurface1*
        {
            mfxStatus sts = MFX_ERR_NONE;
            mfxFrameSurface1* out = nullptr;

            for (;;) {
                auto idx = GetFreeSurfaceIndex(_surfaces);
                if (MFX_ERR_NOT_FOUND == idx)
                    throw std::runtime_error("no free surface");

                mfxSyncPoint syncp;
                // Decode a frame asychronously (returns immediately)
                sts = _decode->DecodeFrameAsync(bs, &_surfaces[idx], &out, &syncp);

                if (MFX_WRN_DEVICE_BUSY == sts) {
                    MSDK_SLEEP(1);
                    continue;
                }

                if (bs == nullptr && sts == MFX_ERR_MORE_DATA) {
                    _decodeEOS = true;
                    break;
                }

                if (MFX_WRN_VIDEO_PARAM_CHANGED == sts && !out)
                    continue;

                // Ignore warnings if output is available,
                if (MFX_ERR_NONE < sts && syncp)
                    sts = MFX_ERR_NONE;

                if (sts == MFX_ERR_MORE_DATA)
                {
                    // decoder cache input bs and didn't return output surface, it's not an error
                    break;
                }

                if (sts == MFX_ERR_MORE_SURFACE)
                {
                    continue;
                }

                if (sts != MFX_ERR_NONE)
                    throw std::runtime_error("DecodeFrameAsync failed");

                break;
            }

            return out;
        };

        auto EncodeFrame = [&](mfxFrameSurface1* srf, mfxBitstream& outBs) -> mfxBitstream*
        {
            auto sts = MFX_ERR_NONE;
            mfxSyncPoint syncp;

            for (;;)
            {
                // Encode a frame asychronously (returns immediately)
                sts = _encode->EncodeFrameAsync(NULL, srf, &outBs, &syncp);

                if (MFX_WRN_DEVICE_BUSY == sts)    // repeat the call if warning and no output
                {
                    MSDK_SLEEP(1);  // wait if device is busy
                    continue;
                }
                else if (MFX_ERR_NONE < sts && syncp) {
                    sts = MFX_ERR_NONE;     // ignore warnings if output is available
                    break;
                }
                else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
                    // Allocate more bitstream buffer memory here if needed...
                    throw std::runtime_error("MFX_ERR_NOT_ENOUGH_BUFFER failed");
                }
                else if (!srf && MFX_ERR_MORE_DATA == sts) {
                    _encodeEOS = true;
                    return nullptr;
                }
                break;
            }

            if (MFX_ERR_NONE == sts) {
                sts = _session->SyncOperation(syncp, 60000);     // Synchronize. Wait until frame processing is ready
                if (sts != MFX_ERR_NONE)
                    throw std::runtime_error("SyncOperation failed");

                return &outBs;

            }
            return nullptr;
        };

        mfxFrameSurface1* decodedSurface = nullptr;
        if (!_decodeEOS)
        {
            decodedSurface = DecodeFrame(inBs);
            if (nullptr == decodedSurface && !_decodeEOS)
                return nullptr;
        }
        
        if (!_encodeEOS)
        {
            return EncodeFrame(decodedSurface, outBs);
        }

        return nullptr;
    }

private:
    std::unique_ptr <MFXVideoSession> _session = {};

    mfxFrameAllocator _allocator = {};
    mfxFrameAllocResponse _allocResponse = {};
    std::vector<mfxFrameSurface1> _surfaces;

    std::unique_ptr<MFXVideoDECODE> _decode = {};
    std::unique_ptr<MFXVideoENCODE> _encode = {};

    mfxVideoParam _decParams = {};
    mfxVideoParam _encParams = {};
    bool _initialized = false;

    bool _decodeEOS = false;
    bool _encodeEOS = false;

    void InternalInit(mfxBitstream& bs)
    {
        _decParams.AsyncDepth = 1;
        _decParams.mfx.CodecId = MFX_CODEC_AVC;
        _decParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;

        auto sts = _decode->DecodeHeader(&bs, &_decParams);
        if (sts != MFX_ERR_NONE)
            throw std::runtime_error("DecodeHeader failed");

        // Query number required surfaces for decoder
        mfxFrameAllocRequest decRequest = {};
        sts = _decode->QueryIOSurf(&_decParams, &decRequest);
        if (sts != MFX_ERR_NONE)
            throw std::runtime_error("decode QueryIOSurf failed");

        _encParams.AsyncDepth = 1;
        _encParams.mfx.LowPower = MFX_CODINGOPTION_ON;
        _encParams.mfx.CodecId = MFX_CODEC_AVC;
        _encParams.mfx.TargetUsage = MFX_TARGETUSAGE_BALANCED;
        _encParams.mfx.TargetKbps = 4000;
        _encParams.mfx.GopRefDist = 1;
        _encParams.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
        _encParams.mfx.FrameInfo.FrameRateExtN = 30;
        _encParams.mfx.FrameInfo.FrameRateExtD = 1;
        _encParams.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        _encParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        _encParams.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        _encParams.mfx.FrameInfo.CropX = 0;
        _encParams.mfx.FrameInfo.CropY = 0;
        _encParams.mfx.FrameInfo.CropW = _decParams.mfx.FrameInfo.CropW;
        _encParams.mfx.FrameInfo.CropH = _decParams.mfx.FrameInfo.CropH;
        // width must be a multiple of 16
        // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
        _encParams.mfx.FrameInfo.Width = MSDK_ALIGN16(_encParams.mfx.FrameInfo.CropW);
        _encParams.mfx.FrameInfo.Height = (MFX_PICSTRUCT_PROGRESSIVE == _encParams.mfx.FrameInfo.PicStruct) ? MSDK_ALIGN16(_encParams.mfx.FrameInfo.CropH) : MSDK_ALIGN32(_encParams.mfx.FrameInfo.CropH);
        _encParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

        // Query number required surfaces for encoder
        mfxFrameAllocRequest encRequest = {};
        sts = _encode->QueryIOSurf(&_encParams, &encRequest);
        if (sts != MFX_ERR_NONE)
            throw std::runtime_error("encode QueryIOSurf failed");

        decRequest.NumFrameMin = decRequest.NumFrameMin + encRequest.NumFrameMin;
        decRequest.NumFrameSuggested = decRequest.NumFrameSuggested + encRequest.NumFrameSuggested;

        // Allocate required surfaces for decoder and encoder
        sts = _allocator.Alloc(_allocator.pthis, &decRequest, &_allocResponse);
        if (sts != MFX_ERR_NONE)
            throw std::runtime_error("Alloc failed");

        auto numSurfacesDec = _allocResponse.NumFrameActual;

        // Allocate surface headers (mfxFrameSurface1) for decoder and encoder
        _surfaces.resize(numSurfacesDec);
        for (int i = 0; i < numSurfacesDec; i++) {
            _surfaces[i].Info =_decParams.mfx.FrameInfo;
            _surfaces[i].Data.MemId = _allocResponse.mids[i];
        }

        // Initialize the Media SDK decoder
        sts = _decode->Init(&_decParams);
        if (sts != MFX_ERR_NONE)
            throw std::runtime_error("decode Init failed");

        // Initialize the Media SDK encoder
        sts = _encode->Init(&_encParams);
        
        if (sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
            sts = MFX_ERR_NONE;
        if (sts != MFX_ERR_NONE)
            throw std::runtime_error("encode Init failed");

        // Retrieve video parameters selected by encoder.
        // - BufferSizeInKB parameter is required to set bit stream buffer size
        mfxVideoParam par = {};
        sts = _encode->GetVideoParam(&par);
        if (sts != MFX_ERR_NONE)
            throw std::runtime_error("GetVideoParam failed");

        _initialized = true;
    }
};
