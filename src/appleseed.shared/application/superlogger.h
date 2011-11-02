
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2010-2011 Francois Beaune
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

#ifndef APPLESEED_SHARED_APPLICATION_SUPERLOGGER_H
#define APPLESEED_SHARED_APPLICATION_SUPERLOGGER_H

// appleseed.foundation headers.
#include "foundation/utility/log.h"

// appleseed.main headers.
#include "main/dllsymbol.h"

namespace appleseed {
namespace shared {

class DLLSYMBOL SuperLogger
  : public foundation::Logger
{
  public:
    // Constructor.
    SuperLogger();

    // Destructor.
    virtual ~SuperLogger();

    void enable_message_coloring();

    foundation::LogTargetBase& get_log_target() const;

  private:
    foundation::LogTargetBase* m_log_target;
};

}       // namespace shared
}       // namespace appleseed

#endif  // !APPLESEED_SHARED_APPLICATION_SUPERLOGGER_H