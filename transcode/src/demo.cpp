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
#include "transcode.h"
#include "common_utils.h"
#include "cmd_options.h"

int main(int argc, char** argv)
{
    mfxStatus sts = MFX_ERR_NONE;
    CmdOptions options;
   // here we parse options
    ParseOptions(argc, argv, &options);

    // Open input H.264 elementary stream (ES) file
    fileUniPtr fSource(OpenFile(options.values.SourceName, "rb"), &CloseFile);
    MSDK_CHECK_POINTER(fSource, MFX_ERR_NULL_PTR);

    // Create output elementary stream (ES) H.264 file
    fileUniPtr fSink(nullptr, &CloseFile);
    fSink.reset(OpenFile(options.values.SinkName, "wb"));
    MSDK_CHECK_POINTER(fSink, MFX_ERR_NULL_PTR);
 
    Transcoder transcoder;

    // Prepare buffers for decoder/encoder
    mfxBitstream mfxBS;
    memset(&mfxBS, 0, sizeof(mfxBS));
    mfxBS.MaxLength = 1024 * 1024;
    std::vector<mfxU8> bstData(mfxBS.MaxLength);
    mfxBS.Data = bstData.data();

    mfxBitstream encodedBS = {};
    encodedBS.MaxLength = 1024 * 1024 * 10;
    std::vector<mfxU8> encodedData(encodedBS.MaxLength);
    encodedBS.Data = encodedData.data();

    uint32_t nFrame = 0;
    mfxTime tStart, tEnd;
    mfxGetTime(&tStart);

    auto readerSts = MFX_ERR_NONE;
    while (readerSts != MFX_ERR_MORE_DATA || mfxBS.DataLength) // process while there's data from the reader
    {
        if (readerSts != MFX_ERR_MORE_DATA)
        {
            readerSts = ReadBitStreamData(&mfxBS, fSource.get());
        }
 
        auto output = transcoder.Process(&mfxBS, encodedBS);

        if (output)
        {
            sts = WriteBitStreamFrame(output, fSink.get());
            if (sts != MFX_ERR_NONE)
                throw std::runtime_error("WriteBitStreamFrame failed");

            printf("Frame number: %d\r", nFrame++);
            fflush(stdout);
        }
    }

    // drain part (decoder/encoder may cache a few frames during processing, when input EOS reached, need to drain cached frames from decoder/encoder)
    while (!transcoder.IsDrainCompleted())
    {
        auto output = transcoder.Process(nullptr, encodedBS);

        if (output)
        {
            sts = WriteBitStreamFrame(output, fSink.get());
            if (sts != MFX_ERR_NONE)
                throw std::runtime_error("WriteBitStreamFrame failed");

            printf("Frame number: %d\r", nFrame++);
            fflush(stdout);
        }
    }

    mfxGetTime(&tEnd);
    double elapsed = TimeDiffMsec(tEnd, tStart) / 1000;
    double fps = ((double)nFrame / elapsed);
    printf("\nExecution time: %3.2f s (%3.2f fps)\n", elapsed, fps);

    return 0;
}
