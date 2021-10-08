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

#pragma once

// =================================================================
// Helper macro definitions...
#define MSDK_CHECK_RESULT(P, X, ERR)    {if ((X) > (P)) {return ERR;}}
#define MSDK_CHECK_POINTER(P, ERR)      {if (!(P)) {return ERR;}}
#define MSDK_ALIGN32(X)                 (((mfxU32)((X)+31)) & (~ (mfxU32)31))
#define MSDK_ALIGN16(value)             (((value + 15) >> 4) << 4)

// =================================================================
void PrintErrString(int err,const char* filestr,int line);
FILE* OpenFile(const char* fileName, const char* mode);
void CloseFile(FILE* fHdl);

using fileUniPtr = std::unique_ptr<FILE, decltype(&CloseFile)>;

// Write bit stream data to file
mfxStatus WriteBitStreamData(mfxBitstream* pMfxBitstream, FILE* fSink);
// Read bit stream data from file. Stream is read as large chunks (= many frames)
mfxStatus ReadBitStreamData(mfxBitstream* pBS, FILE* fSource);

// Get free raw frame surface
int GetFreeSurfaceIndex(const std::vector<mfxFrameSurface1>& pSurfacesPool);
//This is the utility to show the current processor id of the platform.
void showCPUInfo();
