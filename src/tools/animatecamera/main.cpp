
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

// Project headers.
#include "animationpath.h"
#include "commandlinehandler.h"

// appleseed.shared headers.
#include "application/application.h"
#include "application/superlogger.h"

// appleseed.renderer headers.
#include "renderer/api/camera.h"
#include "renderer/api/project.h"
#include "renderer/api/scene.h"
#include "renderer/api/utility.h"

// appleseed.foundation headers.
#include "foundation/math/aabb.h"
#include "foundation/math/matrix.h"
#include "foundation/math/scalar.h"
#include "foundation/math/transform.h"
#include "foundation/math/vector.h"
#include "foundation/platform/types.h"
#include "foundation/utility/autoreleaseptr.h"
#include "foundation/utility/log.h"
#include "foundation/utility/string.h"

// boost headers.
#include "boost/filesystem/path.hpp"

// Standard headers.
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace appleseed::animatecamera;
using namespace appleseed::shared;
using namespace boost;
using namespace foundation;
using namespace renderer;
using namespace std;

namespace
{
    //
    // Command line parameters are globally accessible within this file.
    //

    CommandLineHandler g_cl;


    //
    // Define a strict weak ordering on foundation::Transform<> objects.
    //

    template <typename T>
    struct TransformComparer
    {
        bool operator()(const Transform<T>& lhs, const Transform<T>& rhs) const
        {
            typedef typename Transform<T>::MatrixType MatrixType;

            const MatrixType& lhs_mat = lhs.get_local_to_parent();
            const MatrixType& rhs_mat = rhs.get_local_to_parent();

            const T Eps = make_eps<T>(1.0e-6f, 1.0e-12);

            for (size_t i = 0; i < MatrixType::Components; ++i)
            {
                const T delta = lhs_mat[i] - rhs_mat[i];

                if (delta < -Eps)
                    return true;

                if (delta > Eps)
                    return false;
            }

            return false;
        }
    };


    //
    // Define an ordering on pairs of transforms.
    //

    template <typename T>
    struct TransformPairComparer
    {
        typedef pair<Transform<T>, Transform<T> > TransformPair;

        bool operator()(const TransformPair& lhs, const TransformPair& rhs) const
        {
            const TransformComparer<T> transform_comparer;

            return
                transform_comparer(lhs.first, rhs.first) ? true :
                transform_comparer(rhs.first, lhs.first) ? false :
                transform_comparer(lhs.second, rhs.second);
        }
    };


    //
    // The base class for animation generators.
    //

    class AnimationGenerator
    {
      public:
        AnimationGenerator(
            const string&   base_output_filename,
            Logger&         logger)
          : m_base_output_filename(base_output_filename)
          , m_logger(logger)
        {
        }

        virtual ~AnimationGenerator() {}

        void generate()
        {
            const vector<size_t> frames = do_generate();

            LOG_INFO(
                m_logger,
                "generating render script%s...",
                g_cl.m_part_count.values()[0] > 1 ? "s" : "");

#ifdef _WIN32
            generate_windows_render_script(frames);
#else
            LOG_WARNING(m_logger, "oops, don't know how to generate a render script for this platform.");
#endif
        }

      protected:
        const string        m_base_output_filename;
        Logger&             m_logger;

        virtual vector<size_t> do_generate() = 0;

        static string make_numbered_filename(
            const string&   filename,
            const size_t    number,
            const size_t    digits = 4)
        {
            const filesystem::path path(filename);

            stringstream sstr;
            sstr << path.stem();
            sstr << '.';
            sstr << setw(digits) << setfill('0') << number;
            sstr << path.extension();

            return sstr.str();
        }

        static auto_release_ptr<Project> load_master_project()
        {
            // Construct the schema filename.
            const filesystem::path schema_path =
                  filesystem::path(Application::get_root_path())
                / "schemas/project.xsd";

            // Read the master project file.
            ProjectFileReader reader;
            auto_release_ptr<Project> project(
                reader.read(
                    g_cl.m_filenames.values()[0].c_str(),
                    schema_path.file_string().c_str()));

            // Bail out if the master project file couldn't be read.
            if (project.get() == 0)
                exit(1);

            return project;
        }

      private:
        void generate_windows_render_script(const vector<size_t>& frames) const
        {
            const size_t part_count = g_cl.m_part_count.values()[0];
            const size_t frames_per_part =
                static_cast<size_t>(ceil(static_cast<double>(frames.size()) / part_count));

            for (size_t part = 1, frame_begin = 0; frame_begin < frames.size(); ++part)
            {
                const size_t frame_end = min(frame_begin + frames_per_part, frames.size());

                generate_windows_render_script(
                    frames,
                    part,
                    frame_begin,
                    frame_end);

                frame_begin = frame_end;
            }
        }

        void generate_windows_render_script(
            const vector<size_t>&   frames,
            const size_t            part,
            const size_t            frame_begin,
            const size_t            frame_end) const
        {
            const string RenderScriptBaseFileName = "render.bat";

            const string script_filename =
                frame_begin == 0 && frame_end == frames.size()
                    ? RenderScriptBaseFileName
                    : make_numbered_filename(RenderScriptBaseFileName, part);

            FILE* script_file = fopen(script_filename.c_str(), "wt");

            if (script_file == 0)
                LOG_FATAL(m_logger, "could not write to %s.", script_filename.c_str());

            fprintf(
                script_file,
                "%s",
                "@echo off\n"
                "\n"
                "set bin=\"%1\"\n"
                "set outputpath=\"%2\"\n"
                "set options=/WAIT /BELOWNORMAL /MIN\n"
                "\n"
                "if %outputpath% == \"\" (\n"
                "    set outputpath=frames\n"
                ")\n"
                "\n"
                "if %bin% == \"\" (\n"
                "    echo Usage: %0 path-to-appleseed-binary\n"
                "    goto :end\n"
                ")\n"
                "\n"
                "if not exist %bin% (\n"
                "    echo Could not find %bin%, exiting.\n"
                "    goto :end\n"
                ")\n"
                "\n"
                "if not exist %outputpath% (\n"
                "    mkdir %outputpath%\n"
                ")\n"
                "\n");

            const string output_format = g_cl.m_output_format.values()[0];

            for (size_t i = frame_begin; i < frame_end; ++i)
            {
                const size_t current_frame = i + 1;
                const size_t actual_frame = frames[i];

                const string project_filename = make_numbered_filename(m_base_output_filename + ".appleseed", current_frame);
                const string image_filename = make_numbered_filename(m_base_output_filename + "." + output_format, current_frame);
                const string image_filepath = "%outputpath%\\" + image_filename;

                fprintf(script_file, "if exist \"%s\" (\n", image_filepath.c_str());
                fprintf(script_file, "    echo Skipping %s because it was already rendered...\n", project_filename.c_str());
                fprintf(script_file, ") else (\n");

                if (actual_frame == current_frame)
                {
                    fprintf(script_file, "    echo Rendering %s to %s...\n", project_filename.c_str(), image_filepath.c_str());
                    fprintf(
                        script_file,
                        "    start \"Rendering %s to %s...\" %%options%% %%bin%% %s -o \"%s\"\n",
                        project_filename.c_str(),
                        image_filepath.c_str(),
                        project_filename.c_str(),
                        image_filepath.c_str());
                }
                else
                {
                    const string source_image_filename = make_numbered_filename(m_base_output_filename + "." + output_format, actual_frame);
                    const string source_image_filepath = "%outputpath%\\" + source_image_filename;

                    fprintf(
                        script_file,
                        "    echo Copying %s to %s...\n",
                        source_image_filepath.c_str(),
                        image_filepath.c_str());
                    fprintf(
                        script_file,
                        "    copy \"%s\" \"%s\" >nul\n",
                        source_image_filepath.c_str(),
                        image_filepath.c_str());
                }
                
                fprintf(script_file, ")\n\n");
            }

            fprintf(
                script_file,
                "echo.\n"
                "echo Rendering terminated.\n"
                "echo.\n"
                "\n"
                ":end\n");

            fclose(script_file);
        }
    };


    //
    // Generate an animation using a custom camera path.
    //

    class PathAnimationGenerator
      : public AnimationGenerator
    {
      public:
        PathAnimationGenerator(
            const string&   base_output_filename,
            Logger&         logger)
          : AnimationGenerator(base_output_filename, logger)
        {
        }

      private:
        virtual vector<size_t> do_generate()
        {
            typedef pair<Transformd, Transformd> TransformPair;
            typedef map<TransformPair, size_t, TransformPairComparer<double> > TransformMap;

            TransformMap transform_map;
            vector<size_t> frames;

            // Load the animation path file from disk.
            AnimationPath animation_path(m_logger);
            animation_path.load(
                g_cl.m_animation_path.values()[0].c_str(),
                g_cl.m_3dsmax_mode.is_set() ? AnimationPath::Autodesk3dsMax : AnimationPath::Default);

            // Load the master project from disk.
            auto_release_ptr<Project> project(load_master_project());

            if (animation_path.size() == 0)
                return frames;

            const size_t frame_count =
                animation_path.size() > 1
                    ? animation_path.size() - 1
                    : 1;

            for (size_t i = 0; i < frame_count; ++i)
            {
                const size_t frame = i + 1;

                // Don't render twice the same frame.
                if (i + 1 < animation_path.size())
                {
                    const TransformPair transform_pair(animation_path[i], animation_path[i + 1]);
                    const TransformMap::const_iterator transform_map_key = transform_map.find(transform_pair);

                    if (transform_map_key != transform_map.end())
                    {
                        LOG_INFO(
                            m_logger, "frame " FMT_SIZE_T " is the same as frame " FMT_SIZE_T,
                            frame,
                            transform_map_key->second);
                        frames.push_back(transform_map_key->second);
                        continue;
                    }

                    transform_map.insert(make_pair(transform_pair, frame));
                }

                // Set the camera's transform sequence.
                Camera* camera = project->get_scene()->get_camera();
                camera->transform_sequence().clear();
                camera->transform_sequence().set_transform(0.0, animation_path[i]);
                if (i + 1 < animation_path.size())
                    camera->transform_sequence().set_transform(1.0, animation_path[i + 1]);

                // Write the project file for this frame.
                const string new_path = make_numbered_filename(m_base_output_filename + ".appleseed", frame);
                project->set_path(new_path.c_str());
                if (i == 0)
                    ProjectFileWriter::write(project.ref());
                else ProjectFileWriter::write(project.ref(), ProjectFileWriter::OmitMeshFiles);

                frames.push_back(frame);
            }

            assert(frames.size() == frame_count);

            return frames;
        }
    };


    //
    // Generate a turntable animation.
    //

    class TurntableAnimationGenerator
      : public AnimationGenerator
    {
      public:
        TurntableAnimationGenerator(
            const string&   base_output_filename,
            Logger&         logger)
          : AnimationGenerator(base_output_filename, logger)
        {
        }

      private:
        virtual vector<size_t> do_generate()
        {
            vector<size_t> frames;

            // Retrieve the command line parameter values.
            const int frame_count = g_cl.m_frame_count.values()[0];
            const Vector3d center_offset(
                g_cl.m_camera_target.values()[0],
                g_cl.m_camera_target.values()[1],
                g_cl.m_camera_target.values()[2]);
            const double normalized_distance = g_cl.m_camera_distance.values()[0];
            const double normalized_elevation = g_cl.m_camera_elevation.values()[0];

            if (frame_count < 1)
                LOG_FATAL(m_logger, "the frame count must be greater than or equal to 1.");

            // Load the master project from disk.
            auto_release_ptr<Project> project(load_master_project());

            // Retrieve the scene's bounding box.
            const AABB3d scene_bbox(project->get_scene()->compute_bbox());
            const Vector3d extent = scene_bbox.extent();
            const double max_radius = 0.5 * max(extent.x, extent.z);
            const double max_height = 0.5 * extent.y;

            // Precompute some stuff.
            const Vector3d Up(0.0, 1.0, 0.0);
            const Vector3d center = scene_bbox.center() + center_offset;
            const double distance = max_radius * normalized_distance;
            const double elevation = max_height * normalized_elevation;

            // Compute the transform of the camera at the last frame.
            const double angle = -1.0 / frame_count * TwoPi;
            const Vector3d position(distance * cos(angle), elevation, distance * sin(angle));
            Transformd previous_transform(Matrix4d::lookat(position, center, Up));

            for (int i = 0; i < frame_count; ++i)
            {
                // Compute the transform of the camera at this frame.
                const double angle = static_cast<double>(i) / frame_count * TwoPi;
                const Vector3d position(distance * cos(angle), elevation, distance * sin(angle));
                const Transformd new_transform(Matrix4d::lookat(position, center, Up));

                // Set the camera's transform sequence.
                Camera* camera = project->get_scene()->get_camera();
                camera->transform_sequence().clear();
                camera->transform_sequence().set_transform(0.0, previous_transform);
                camera->transform_sequence().set_transform(1.0, new_transform);
                previous_transform = new_transform;

                // Write the project file for this frame.
                const size_t frame = static_cast<size_t>(i + 1);
                const string new_path = make_numbered_filename(m_base_output_filename + ".appleseed", frame);
                project->set_path(new_path.c_str());
                if (i == 0)
                    ProjectFileWriter::write(project.ref());
                else ProjectFileWriter::write(project.ref(), ProjectFileWriter::OmitMeshFiles);

                frames.push_back(frame);
            }

            assert(frames.size() == static_cast<size_t>(frame_count));

            return frames;
        }
    };
}


//
// Entry point of animatecamera.
//

int main(int argc, const char* argv[])
{
    SuperLogger logger;

    Application::check_installation(logger);

    g_cl.parse(argc, argv, logger);

    global_logger().add_target(&logger.get_log_target());

    const string base_output_filename =
        filesystem::path(g_cl.m_filenames.values()[1]).stem();

    auto_ptr<AnimationGenerator> generator;

    if (g_cl.m_animation_path.is_set())
        generator.reset(new PathAnimationGenerator(base_output_filename, logger));
    else generator.reset(new TurntableAnimationGenerator(base_output_filename, logger));

    generator->generate();

    return 0;
}
