
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
#include "imagestack.h"

// appleseed.renderer headers.
#include "renderer/kernel/aov/tilestack.h"

// appleseed.foundation headers.
#include "foundation/image/image.h"

// Standard headers.
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

using namespace foundation;
using namespace std;

namespace renderer
{

struct ImageStack::Impl
{
    size_t                  m_canvas_width;
    size_t                  m_canvas_height;
    size_t                  m_tile_width;
    size_t                  m_tile_height;

    struct NamedImage
    {
        string              m_name;
        ImageStack::Type    m_type;
        Image*              m_image;
    };

    vector<NamedImage>      m_images;
};

ImageStack::ImageStack(
    const size_t            canvas_width,
    const size_t            canvas_height,
    const size_t            tile_width,
    const size_t            tile_height)
  : impl(new Impl())
{
    impl->m_canvas_width = canvas_width;
    impl->m_canvas_height = canvas_height;
    impl->m_tile_width = tile_width;
    impl->m_tile_height = tile_height;
}

ImageStack::~ImageStack()
{
    clear();

    delete impl;
}

void ImageStack::clear()
{
    const size_t size = impl->m_images.size();

    for (size_t i = 0; i < size; ++i)
        delete impl->m_images[i].m_image;

    impl->m_images.clear();
}

bool ImageStack::empty() const
{
    return impl->m_images.empty();
}

size_t ImageStack::size() const
{
    return impl->m_images.size();
}

const char* ImageStack::get_name(const size_t index) const
{
    assert(index < impl->m_images.size());
    return impl->m_images[index].m_name.c_str();
}

ImageStack::Type ImageStack::get_type(const size_t index) const
{
    assert(index < impl->m_images.size());
    return impl->m_images[index].m_type;
}

const Image& ImageStack::get_image(const size_t index) const
{
    assert(index < impl->m_images.size());
    return *impl->m_images[index].m_image;
}

size_t ImageStack::get(const char* name) const
{
    const size_t size = impl->m_images.size();

    for (size_t i = 0; i < size; ++i)
    {
        if (strcmp(impl->m_images[i].m_name.c_str(), name) == 0)
            return i;
    }

    return ~0;
}

size_t ImageStack::append(
    const char*             name,
    const Type              type,
    const PixelFormat       pixel_format)
{
    Impl::NamedImage named_image;

    named_image.m_name = name;
    named_image.m_type = type;
    named_image.m_image =
        new Image(
            impl->m_canvas_width,
            impl->m_canvas_height,
            impl->m_tile_width,
            impl->m_tile_height,
            4,
            pixel_format);

    const size_t aov_index = impl->m_images.size();

    impl->m_images.push_back(named_image);

    return aov_index;
}

TileStack ImageStack::tiles(
    const size_t            tile_x,
    const size_t            tile_y) const
{
    TileStack tile_stack;

    const size_t size = impl->m_images.size();

    for (size_t i = 0; i < size; ++i)
        tile_stack.append(&impl->m_images[i].m_image->tile(tile_x, tile_y));

    return tile_stack;
}

}   // namespace renderer
