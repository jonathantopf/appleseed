
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2010-2013 Francois Beaune, Jupiter Jazz Limited
// Copyright (c) 2014 Francois Beaune, The appleseedhq Organization
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

// Interface header.
#include "samplegeneratorjob.h"

// appleseed.renderer headers.
#include "renderer/kernel/rendering/progressive/samplecounter.h"
#include "renderer/kernel/rendering/isamplegenerator.h"
#include "renderer/kernel/rendering/itilecallback.h"
#include "renderer/kernel/rendering/sampleaccumulationbuffer.h"
#include "renderer/modeling/frame/frame.h"

// appleseed.foundation headers.
#include "foundation/image/canvasproperties.h"
#include "foundation/image/image.h"

// Standard headers.
#include <algorithm>

using namespace foundation;
using namespace std;

namespace renderer
{

//
// SampleGeneratorJob class implementation.
//

namespace
{
    const size_t MinSamplePassCount = 1;            // number of passes that will stick to MinSampleCount
    const size_t MinSampleCount     = 1024 * 2;     // minimum number of samples in one pass
    const size_t MaxSampleCount     = 1024 * 256;   // maximum number of samples in one pass
    const size_t SampleIncrement    = 1024 * 4;     // number of samples added at each pass

    size_t compute_sample_count(const size_t pass)
    {
        return
            pass < MinSamplePassCount ? MinSampleCount :
            min(
                MinSampleCount + (pass - MinSamplePassCount + 1) * SampleIncrement,
                MaxSampleCount);
    }
}

SampleGeneratorJob::SampleGeneratorJob(
    Frame&                      frame,
    SampleAccumulationBuffer&   buffer,
    ISampleGenerator*           sample_generator,
    SampleCounter&              sample_counter,
    ITileCallback*              tile_callback,
    JobQueue&                   job_queue,
    const size_t                job_index,
    const size_t                job_count,
    const size_t                pass,
    AbortSwitch&                abort_switch)
  : m_frame(frame)
  , m_buffer(buffer)
  , m_sample_generator(sample_generator)
  , m_sample_counter(sample_counter)
  , m_tile_callback(tile_callback)
  , m_job_queue(job_queue)
  , m_job_index(job_index)
  , m_job_count(job_count)
  , m_pass(pass)
  , m_abort_switch(abort_switch)
{
}

void SampleGeneratorJob::execute(const size_t thread_index)
{
    const size_t sample_count =
        m_sample_counter.reserve(compute_sample_count(m_pass));

    if (sample_count == 0)
        return;

    // Invoke the pre-pass callback if there is one.
    if (m_tile_callback)
    {
        const CanvasProperties& props = m_frame.image().properties();
        m_tile_callback->pre_render(
            0,
            0,
            props.m_canvas_width,
            props.m_canvas_height);
    }

    if (m_pass == 0)
    {
        // The first pass is uninterruptible in order to always get something
        // on screen during navigation. todo: this needs to change as it will
        // freeze rendering if the first pass cannot generate samples.
        AbortSwitch no_abort;
        m_sample_generator->generate_samples(sample_count, m_buffer, no_abort);
    }
    else
    {
        m_sample_generator->generate_samples(sample_count, m_buffer, m_abort_switch);
    }

    if (m_job_index == 0)
    {
        m_buffer.develop_to_frame(m_frame);

        if (m_tile_callback)
            m_tile_callback->post_render(&m_frame);
    }

    // This job reschedules itself automatically.
    if (!m_abort_switch.is_aborted())
    {
        m_job_queue.schedule(
            new SampleGeneratorJob(
                m_frame,
                m_buffer,
                m_sample_generator,
                m_sample_counter,
                m_tile_callback,
                m_job_queue,
                m_job_index,
                m_job_count,
                m_pass + 1,
                m_abort_switch));
    }
}

}   // namespace renderer
