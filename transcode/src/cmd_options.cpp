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

#include <cstring>
#include <stdlib.h>
#include "cmd_options.h"

void ParseOptions(int argc, char* argv[], CmdOptions* cmd_options)
{
    int i = 1;

    if (!cmd_options->ctx.program) {
        cmd_options->ctx.program = argv[0];
    }

    if (i < argc) {
        if (std::strlen(argv[i]) < MSDK_MAX_PATH) {
            strncpy_s(cmd_options->values.SinkName, argv[i], std::strlen(cmd_options->values.SinkName));
        } else {
            printf("error: destination file name is too long\n");
            exit(-1);
        }
        ++i;
    }
    if (i < argc) {
        printf("error: too many arguments\n");
        exit(-1);
    }
}
