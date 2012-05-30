
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2010-2012 Francois Beaune, Jupiter Jazz Limited
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
#include "textureobject.h"

// appleseed.foundation headers.
#include "foundation/image/canvasproperties.h"
#include "foundation/image/color.h"
#include "foundation/image/image.h"
#include "foundation/image/tile.h"
#include "foundation/math/scalar.h"

// Standard headers.
#include <cassert>
#include <cstring>

using namespace foundation;
using namespace std;

namespace renderer
{

TextureObject::TextureObject(Image& texture)
  : m_texture(texture)
  , m_width(texture.properties().m_canvas_width)
  , m_height(texture.properties().m_canvas_height)
  , m_channel_count(texture.properties().m_channel_count)
{
    m_texels.resize(m_width * m_height * m_channel_count);

    const CanvasProperties& props = texture.properties();

    for (size_t ty = 0; ty < props.m_tile_count_y; ++ty)
    {
        for (size_t tx = 0; tx < props.m_tile_count_x; ++tx)
        {
            const Tile& tile = texture.tile(tx, ty);
            const size_t tile_width = tile.get_width();
            const size_t tile_height = tile.get_height();

            for (size_t py = 0; py < tile_height; ++py)
            {
                for (size_t px = 0; px < tile_width; ++px)
                {
                    const size_t ix = tx * props.m_tile_width + px;
                    const size_t iy = ty * props.m_tile_height + py;
                    const size_t index = iy * m_width + ix;

                    for (size_t c = 0; c < m_channel_count; ++c)
                        m_texels[index * m_channel_count + c] = tile.get_component<float>(px, py, c);
                }
            }
        }
    }
}

void TextureObject::get(
    const int   x,
    const int   y,
    float       texel[]) const
{
    const int clamped_x = clamp(x, 0, static_cast<int>(m_width) - 1);
    const int clamped_y = clamp(y, 0, static_cast<int>(m_height) - 1);
    const size_t base_index = (clamped_y * m_width + clamped_x) * m_channel_count;

    for (size_t i = 0; i < m_channel_count; ++i)
        texel[i] = m_texels[base_index + i];
}

void TextureObject::put(
    const int   x,
    const int   y,
    const float texel[])
{
    assert(x >= 0 && x < static_cast<int>(m_width));
    assert(y >= 0 && y < static_cast<int>(m_height));

    memcpy(m_texture.pixel(x, y), texel, m_channel_count * sizeof(float));
}

}   // namespace renderer