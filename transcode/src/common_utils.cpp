// Copyright (c) 2019-2020 Intel Corporation
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


#include <algorithm>
#include <intrin.h>
#include <array>
#include <vector>
#include <memory>
#include "mfxvideo++.h"
#include "common_utils.h"

// =================================================================
// Utility functions, not directly tied to Intel Media SDK functionality
//

FILE* OpenFile(const char* fileName, const char* mode)
{
    FILE* openFile = nullptr;
    fopen_s(&openFile, fileName, mode);
    return openFile;
}

void CloseFile(FILE* fHdl)
{
    if(fHdl)
        fclose(fHdl);
}

mfxStatus WriteBitStreamData(mfxBitstream* pMfxBitstream, FILE* fSink)
{
    if (!pMfxBitstream)
       return MFX_ERR_NULL_PTR;

    if (fSink) {
        mfxU32 nBytesWritten =
            (mfxU32) fwrite(pMfxBitstream->Data + pMfxBitstream->DataOffset, 1,
                            pMfxBitstream->DataLength, fSink);

        if (nBytesWritten != pMfxBitstream->DataLength)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    pMfxBitstream->DataLength = 0;

    return MFX_ERR_NONE;
}

mfxStatus ReadBitStreamData(mfxBitstream* pBS, FILE* fSource)
{
    memmove(pBS->Data, pBS->Data + pBS->DataOffset, pBS->DataLength);
    pBS->DataOffset = 0;

    if (pBS->DataLength == pBS->MaxLength)
        return MFX_ERR_NONE;

    mfxU32 nBytesRead = (mfxU32) fread(pBS->Data + pBS->DataLength, 1,
                                       pBS->MaxLength - pBS->DataLength,
                                       fSource);

    if (0 == nBytesRead)
        return MFX_ERR_MORE_DATA;

    pBS->DataLength += nBytesRead;

    return MFX_ERR_NONE;
}

int GetFreeSurfaceIndex(const std::vector<mfxFrameSurface1>& pSurfacesPool)
{
    auto it = std::find_if(pSurfacesPool.begin(), pSurfacesPool.end(), [](const mfxFrameSurface1& surface) {
                        return 0 == surface.Data.Locked;
                    });

     return static_cast<int>((it == pSurfacesPool.end()) ? MFX_ERR_NOT_FOUND : std::distance(pSurfacesPool.begin(), it));
}

#if defined(_WIN32) || defined(_WIN64)
//This function is modified according to the MSDN example at
// https://msdn.microsoft.com/en-us/library/hskdteyh(v=vs.140).aspx#Example
void showCPUInfo() {
    int nIds;
    int nExIds;
    std::vector<std::array<int, 4>> vendorData;
    std::vector<std::array<int, 4>> procData;
    std::array<int, 4> cpui;

    // Calling __cpuid with 0x0 as the function_id argument
    // gets the number of the highest valid function ID.
    __cpuid(cpui.data(), 0);
    nIds = cpui[0];

    for (int i = 0; i <= nIds; ++i)
    {
        __cpuidex(cpui.data(), i, 0);
        vendorData.push_back(cpui);
    }

    // Capture vendor string
    char vendor[0x20];
    memset(vendor, 0, sizeof(vendor));
    *reinterpret_cast<int*>(vendor) = vendorData[0][1];
    *reinterpret_cast<int*>(vendor + 4) = vendorData[0][3];
    *reinterpret_cast<int*>(vendor + 8) = vendorData[0][2];

    // Calling __cpuid with 0x80000000 as the function_id argument
    // gets the number of the highest valid extended ID.
    __cpuid(cpui.data(), 0x80000000);
    nExIds = cpui[0];

    char processor[0x40];
    memset(processor, 0, sizeof(processor));

    for (int i = 0x80000000; i <= nExIds; ++i)
    {
        __cpuidex(cpui.data(), i, 0);
        procData.push_back(cpui);
    }

    // Interpret CPU brand string if reported
    if (nExIds >= 0x80000004)
    {
        memcpy(processor, procData[2].data(), sizeof(cpui));
        memcpy(processor + 16, procData[3].data(), sizeof(cpui));
        memcpy(processor + 32, procData[4].data(), sizeof(cpui));
    }

    printf("Vendor: %s\n", vendor);
    printf("Processor: %s\n", processor);
    printf("Please check http://ark.intel.com/ for the GPU info\n");
}
#endif
