/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <osg2vsg/convert.h>
#include <osg2vsg/OSG.h>
#include <osgDB/FileNameUtils>

#include "ConvertToVsg.h"
#include "ImageUtils.h"
#include <filesystem>

using namespace osg2vsg;

vsg::ref_ptr<vsg::Data> osg2vsg::convert(const osg::Image& image, vsg::ref_ptr<const vsg::Options> options)
{
    bool mapRGBtoRGBAHint = !options || options->mapRGBtoRGBAHint;
    return convertToVsg(&image, mapRGBtoRGBAHint);
}

vsg::ref_ptr<vsg::Data> osg2vsg::convert(const osg::Array& src_array, vsg::ref_ptr<const vsg::Options> /*options*/)
{
    switch (src_array.getType())
    {
    case (osg::Array::ByteArrayType): return {};
    case (osg::Array::ShortArrayType): return {};
    case (osg::Array::IntArrayType): return {};

    case (osg::Array::UByteArrayType): return convert<vsg::ubyteArray>(src_array);
    case (osg::Array::UShortArrayType): return convert<vsg::ushortArray>(src_array);
    case (osg::Array::UIntArrayType): return convert<vsg::uintArray>(src_array);

    case (osg::Array::FloatArrayType): return convert<vsg::floatArray>(src_array);
    case (osg::Array::DoubleArrayType): return convert<vsg::doubleArray>(src_array);

    case (osg::Array::Vec2bArrayType): return {};
    case (osg::Array::Vec3bArrayType): return {};
    case (osg::Array::Vec4bArrayType): return {};

    case (osg::Array::Vec2sArrayType): return {};
    case (osg::Array::Vec3sArrayType): return {};
    case (osg::Array::Vec4sArrayType): return {};

    case (osg::Array::Vec2iArrayType): return {};
    case (osg::Array::Vec3iArrayType): return {};
    case (osg::Array::Vec4iArrayType): return {};

    case (osg::Array::Vec2ubArrayType): return convert<vsg::ubvec2Array>(src_array);
    case (osg::Array::Vec3ubArrayType): return convert<vsg::ubvec3Array>(src_array);
    case (osg::Array::Vec4ubArrayType): return convert<vsg::ubvec4Array>(src_array);

    case (osg::Array::Vec2usArrayType): return convert<vsg::usvec2Array>(src_array);
    case (osg::Array::Vec3usArrayType): return convert<vsg::usvec3Array>(src_array);
    case (osg::Array::Vec4usArrayType): return convert<vsg::usvec4Array>(src_array);

    case (osg::Array::Vec2uiArrayType): return convert<vsg::uivec2Array>(src_array);
    case (osg::Array::Vec3uiArrayType): return convert<vsg::usvec3Array>(src_array);
    case (osg::Array::Vec4uiArrayType): return convert<vsg::usvec4Array>(src_array);

    case (osg::Array::Vec2ArrayType): return convert<vsg::vec2Array>(src_array);
    case (osg::Array::Vec3ArrayType): return convert<vsg::vec3Array>(src_array);
    case (osg::Array::Vec4ArrayType): return convert<vsg::vec4Array>(src_array);

    case (osg::Array::Vec2dArrayType): return convert<vsg::dvec2Array>(src_array);
    case (osg::Array::Vec3dArrayType): return convert<vsg::dvec2Array>(src_array);
    case (osg::Array::Vec4dArrayType): return convert<vsg::dvec2Array>(src_array);

    case (osg::Array::MatrixArrayType): return convert<vsg::mat4Array>(src_array);
    case (osg::Array::MatrixdArrayType): return convert<vsg::dmat4Array>(src_array);

#if OSG_MIN_VERSION_REQUIRED(3, 5, 7)
    case (osg::Array::QuatArrayType): return {};

    case (osg::Array::UInt64ArrayType): return {};
    case (osg::Array::Int64ArrayType): return {};
#endif
    default: return {};
    }
}

class ProcessTextureVisitor final : public osg::NodeVisitor
{
public:
    ProcessTextureVisitor(std::string in_path = {}) :
        osg::NodeVisitor(TRAVERSE_ALL_CHILDREN),
        m_path{ std::move(in_path) }
    {
        m_path = m_path.substr(0, m_path.find_last_of('\\'));
    }

    ProcessTextureVisitor(const ProcessTextureVisitor&) = default;
    ProcessTextureVisitor(ProcessTextureVisitor&&) = default;

    ProcessTextureVisitor& operator=(const ProcessTextureVisitor&) = default;
    ProcessTextureVisitor& operator=(ProcessTextureVisitor&&) = default;

    ~ProcessTextureVisitor() = default;

public:
    void apply(osg::Node& in_node)
    {
        processStateSet(in_node.getStateSet());
        traverse(in_node);
    }

    void apply(osg::Geode& in_geode)
    {
        processStateSet(in_geode.getStateSet());

        for (unsigned int i = 0; i < in_geode.getNumDrawables(); i++)
        {
            osg::Drawable* drawable{ in_geode.getDrawable(i) };

            if (drawable)
                apply(*drawable);
        }
    }

    void apply(osg::Drawable& in_drawable)
    {
        processStateSet(in_drawable.getStateSet());
    }

private:
    std::string m_path;

private:
    void processStateSet(osg::StateSet* in_stateSet)
    {
        if (!in_stateSet)
            return;

        bool processed{ false };
        const bool hasProcessedValue{ in_stateSet->getUserValue("processed", processed) };
        const bool needToProcess{ (hasProcessedValue == false) || (hasProcessedValue == true && processed == false) };

        if (!needToProcess)
            return;

        in_stateSet->setUserValue("processed", true);
        std::cout << "Processing state set\n";

        osg::ref_ptr<osgDB::ReaderWriter::Options> options{ new osgDB::ReaderWriter::Options("dds_flip") };

        std::string baseImagePath{ findTextureUnit(0, in_stateSet) };

        if (baseImagePath.empty())
            return;

        osg::Texture2D* baseTexture{ dynamic_cast<osg::Texture2D*> (in_stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE)) };
        std::string base{ osgDB::getNameLessExtension(baseImagePath) };

        size_t pos = base.rfind("_diffuse");

        if (pos != std::string::npos) {
            base = base.erase(pos);
        }
        else {
            pos = base.rfind("_Diffuse");
            if (pos != std::string::npos) {
                base = base.erase(pos);
            }
            else {
                pos = base.rfind("_DIFFUSE");
                if (pos != std::string::npos) {
                    base = base.erase(pos);
                }
            }
        }

        std::string extension{ osgDB::getFileExtension(baseImagePath) };

        auto loadAndSetTexture = [&](const std::string& in_textureType, unsigned int in_unit, const std::string& in_userValue)
        {
            std::string imageFileName{ m_path + "\\" + base + in_textureType + extension };

            if (std::filesystem::exists(imageFileName))
            {
                osg::ref_ptr<osg::Image> image{ osgDB::readImageFile(imageFileName) };

                if (image->valid())
                {
                    osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D;
                    texture->setWrap(osg::Texture::WRAP_S, baseTexture->getWrap(osg::Texture::WRAP_S));
                    texture->setWrap(osg::Texture::WRAP_T, baseTexture->getWrap(osg::Texture::WRAP_T));
                    texture->setWrap(osg::Texture::WRAP_R, baseTexture->getWrap(osg::Texture::WRAP_R));
                    texture->setFilter(osg::Texture::MIN_FILTER, baseTexture->getFilter(osg::Texture::MIN_FILTER));
                    texture->setFilter(osg::Texture::MAG_FILTER, baseTexture->getFilter(osg::Texture::MAG_FILTER));
                    texture->setMaxAnisotropy(baseTexture->getMaxAnisotropy());

                    texture->setImage(image);
                    in_stateSet->setTextureAttributeAndModes(in_unit, texture);

                    in_stateSet->setUserValue(in_userValue, true);

                    std::cout << "Loaded " << imageFileName << '\n';

                    if (extension == "dds")
                        in_stateSet->setUserValue("normNeedsFlip", true);
                }
            }
        };
        
        loadAndSetTexture("_NML.", NORMAL_TEXTURE_UNIT, "norm");
        //loadAndSetTexture("_NML.", 1, "norm");
        //loadAndSetTexture("_AORM.", 3, "aorm");
        //loadAndSetTexture("_EMISS.", 7, "emiss");
        //loadAndSetTexture("_HEIGHT.", 9, "height");
    }

    std::string findTextureUnit(int in_unit, osg::StateSet* in_stateSet)
    {
        if (!in_stateSet)
            return {};

        osg::Texture2D* texture{ dynamic_cast<osg::Texture2D*>(in_stateSet->getTextureAttribute(in_unit, osg::StateAttribute::TEXTURE)) };

        if (!texture)
            return {};

        osg::Image* image{ texture->getImage(0) };

        return std::string(image ? image->getFileName() : "");
    }

}; // !class ProcessTextureVisitor

vsg::ref_ptr<vsg::Node> osg2vsg::convert(const osg::Node& node, vsg::ref_ptr<const vsg::Options> options, const vsg::Path& filePath)
{
    bool mapRGBtoRGBAHint = !options || options->mapRGBtoRGBAHint;
    vsg::Paths searchPaths = options ? options->paths : vsg::getEnvPaths("VSG_FILE_PATH");

    vsg::ref_ptr<osg2vsg::BuildOptions> buildOptions;
    auto pipelineCache = osg2vsg::PipelineCache::create();

    std::string build_options_filename;
    if (options->getValue(OSG::read_build_options, build_options_filename))
    {
        buildOptions = vsg::read_cast<osg2vsg::BuildOptions>(build_options_filename, options);
    }

    if (!buildOptions)
    {
        buildOptions = osg2vsg::BuildOptions::create();
        buildOptions->mapRGBtoRGBAHint = mapRGBtoRGBAHint;
    }

    if (options->getValue(OSG::write_build_options, build_options_filename))
    {
        vsg::write(buildOptions, build_options_filename, options);
    }

    buildOptions->options = options;
    buildOptions->pipelineCache = pipelineCache;

    auto osg_scene = const_cast<osg::Node*>(&node);
    ProcessTextureVisitor processTextureVisitor{ filePath.string() };
    osg_scene->traverse(processTextureVisitor);

    if (vsg::value<bool>(false, OSG::original_converter, options))
    {
        osg2vsg::SceneBuilder sceneBuilder(buildOptions);
        auto vsg_scene = sceneBuilder.optimizeAndConvertToVsg(osg_scene, searchPaths);
        return vsg_scene;
    }
    else
    {
        vsg::ref_ptr<vsg::StateGroup> inheritedStateGroup;

        osg2vsg::ConvertToVsg sceneBuilder(buildOptions, inheritedStateGroup);

        sceneBuilder.optimize(osg_scene);
        auto vsg_scene = sceneBuilder.convert(osg_scene);

        if (sceneBuilder.numOfPagedLOD > 0)
        {
            uint32_t maxLevel = 20;
            uint32_t estimatedNumOfTilesBelow = 0;
            uint32_t maxNumTilesBelow = 1024;

            uint32_t level = 0;
            for (uint32_t i = level; i < maxLevel; ++i)
            {
                estimatedNumOfTilesBelow += std::pow(4, i - level);
            }

            uint32_t tileMultiplier = std::min(estimatedNumOfTilesBelow, maxNumTilesBelow) + 1;

            vsg::CollectResourceRequirements collectResourceRequirements;
            vsg_scene->accept(collectResourceRequirements);
            vsg_scene->setObject("ResourceHints", collectResourceRequirements.createResourceHints(tileMultiplier));
        }
        return vsg_scene;
    }
}
