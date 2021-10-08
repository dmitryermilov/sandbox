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
#include "device.h"
#include "transcode.h"
#include "common_utils.h"
#include "cmd_options.h"

class mfxBitstreamWrapper : public mfxBitstream
{
    typedef mfxBitstream base;
public:
    mfxBitstreamWrapper()
        : base()
    {}

    mfxBitstreamWrapper(mfxU32 n_bytes)
        : base()
    {
        Extend(n_bytes);
    }

    mfxBitstreamWrapper(const mfxBitstreamWrapper& bs_wrapper)
        : base(bs_wrapper)
        , m_data(bs_wrapper.m_data)
    {
        Data = m_data.data();
    }

    mfxBitstreamWrapper& operator=(mfxBitstreamWrapper const& bs_wrapper)
    {
        mfxBitstreamWrapper tmp(bs_wrapper);

        *this = std::move(tmp);

        return *this;
    }

    mfxBitstreamWrapper(mfxBitstreamWrapper&& bs_wrapper) = default;
    mfxBitstreamWrapper& operator= (mfxBitstreamWrapper&& bs_wrapper) = default;
    ~mfxBitstreamWrapper() = default;

    void Extend(mfxU32 n_bytes)
    {
        if (MaxLength >= n_bytes)
            return;

        m_data.resize(n_bytes);

        Data = m_data.data();
        MaxLength = n_bytes;
    }
private:
    std::vector<mfxU8> m_data;
};

#define NUM_DEVICES 3
#define NUM_CAMS 5

#pragma warning (disable : 4996)  // for fopen
int main(int , char** )
{
    mfxStatus sts = MFX_ERR_NONE;
   // here we parse options

    std::string data_location = "C:\\Data\\20210311_174028"; // 1 - CHANGE THIS to required data folder
    std::string img_file_format = "img%04d_dev%02d_cam%02d.jpg";
    char str_src_buf[1000];
    int numFrameSets = 248;

    const size_t adapterNum = 0;
    std::unique_ptr<IHWDevice> device(new D3D11Device(adapterNum)); //let's have one device for all sessions (but actually we can create separete device per session)

    struct Pipeline
    {
        Pipeline(std::unique_ptr<IHWDevice>& device)
        {
            transcoder.reset(new Transcoder(device));
            outBS.Extend(1024 * 1024 * 10);
        }

        std::unique_ptr<Transcoder> transcoder;
        mfxBitstreamWrapper inBS = {};
        mfxBitstreamWrapper outBS = {};
    };

    std::vector<std::unique_ptr<Pipeline>> pipelines;
    for (auto i = 0; i < NUM_DEVICES * NUM_CAMS; pipelines.emplace_back(new Pipeline((device))), ++i);
    std::vector<std::thread> threads(NUM_DEVICES* NUM_CAMS);

    uint32_t nFrame = 0;
    auto start = std::chrono::high_resolution_clock::now();

    for (int d = 0; d < NUM_DEVICES; d++)
    {
        for (int c = 0; c < NUM_CAMS; ++c)
        {
            auto f = [&](const mfxU32 d, const mfxU32 c) -> mfxStatus 
            {
                const auto pipeNum = d * NUM_CAMS + c;
                auto& inBS = pipelines[pipeNum]->inBS;
                auto& outBS = pipelines[pipeNum]->outBS;
                auto& transcoder = pipelines[pipeNum]->transcoder;

                std::string vdo_file_format = "hevc_cam%02d.h265";
                std::string format = data_location + "\\" + vdo_file_format;
                char str_dst_buf[1000];
                snprintf(str_dst_buf, sizeof(str_dst_buf), format.c_str(), pipeNum);
                FILE* dst_file = fopen(str_dst_buf, "wb");
                for (int i = 0; i < numFrameSets; ++i)
                {
                    mfxBitstream* output = nullptr;

                    FILE* source = nullptr;
                    format = data_location + "\\" + img_file_format;
                    snprintf(str_src_buf, sizeof(str_src_buf), format.c_str(), i + 1, d, c);
                    source = fopen(str_src_buf, "rb");
                    fseek(source, 0, SEEK_END);
                    auto fsize = ftell(source);
                    inBS.Extend(fsize);
                    inBS.Extend(fsize);
                    fseek(source, 0, SEEK_SET);

                    sts = ReadBitStreamData(&inBS, source);
                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                    inBS.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
                    fclose(source);

                    output = transcoder->Process(&inBS, outBS);
                    
                    if (output)
                    {
                        sts = WriteBitStreamData(output, dst_file);
                        if (sts != MFX_ERR_NONE)
                            throw std::runtime_error("WriteBitStreamFrame failed");

                        printf("Frame number: %d\r", nFrame++);
                        fflush(stdout);
                    }

                }

                fclose(dst_file);
                return MFX_ERR_NONE;
            };

            const auto pipeNum = d * NUM_CAMS + c;
            threads[pipeNum] = std::thread(f, d, c);
        }
    }

    for (int d = 0; d < NUM_DEVICES; d++)
    {
        for (int c = 0; c < NUM_CAMS; ++c)
        {
            const auto pipeNum = d * NUM_CAMS + c;
            threads[pipeNum].join();
        }
    }
#if 0
    // drain part (decoder/encoder may cache a few frames during processing, when input EOS reached, need to drain cached frames from decoder/encoder)
    for (int d = 0; d < NUM_DEVICES; ++d)
    {
        for (int c = 0; c < NUM_CAMS; ++c)
        {
            const auto pipeNum = d * NUM_CAMS + c;
            auto& outBS = pipelines[pipeNum]->outBS;
            auto& transcoder = pipelines[pipeNum]->transcoder;
            while (!transcoder->IsDrainCompleted())
            {
                auto output = transcoder->Process(nullptr, outBS);

                if (output)
                {
                    sts = WriteBitStreamData(output, fSink.get());
                    if (sts != MFX_ERR_NONE)
                        throw std::runtime_error("WriteBitStreamFrame failed");

                    printf("Frame number: %d\r", nFrame++);
                    fflush(stdout);
                }
            }
        }
    }
#endif
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);
    double delta = std::chrono::duration<double, std::milli>(duration).count();

    double fps = nFrame / (delta / 1000.);
    printf("\nExecution time: %3.2f s (%3.2f fps)\n", delta / 1000, fps);

    return 0;
}
