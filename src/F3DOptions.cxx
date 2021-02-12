#include "F3DOptions.h"

#include "F3DFileSystem.h"
#include "F3DLog.h"

#include <vtk_jsoncpp.h>
#include <vtksys/SystemTools.hxx>

#include <fstream>
#include <regex>
#include <vector>

#include "cxxopts.hpp"

//----------------------------------------------------------------------------
namespace
{
template<typename T>
bool CompareVectors(const std::vector<T>& v1, const std::vector<T>& v2)
{
  if (v1.size() != v2.size())
  {
    return false;
  }

  for (size_t i = 0; i < v1.size(); i++)
  {
    if (v1[i] != v2[i])
    {
      return false;
    }
  }

  return true;
}
}  // namespace

class ConfigurationOptions
{
public:
  ConfigurationOptions(int argc, char** argv)
    : Argc(argc)
    , Argv(argv)
  {
  }

  F3DOptions GetOptionsFromArgs(std::vector<std::string>& inputs);
  bool InitializeDictionaryFromConfigFile(const std::string& filePath);

protected:
  template<class T>
  std::string GetOptionDefault(const std::string& option, T currValue) const
  {
    auto it = this->ConfigDic.find(option);
    if (it == this->ConfigDic.end())
    {
      std::stringstream ss;
      ss << currValue;
      return ss.str();
    }
    return it->second;
  }

  std::string GetOptionDefault(const std::string& option, bool currValue) const
  {
    auto it = this->ConfigDic.find(option);
    if (it == this->ConfigDic.end())
    {
      return currValue ? "true" : "false";
    }
    return it->second;
  }

  std::string CollapseName(const std::string& longName, const std::string& shortName) const
  {
    std::stringstream ss;
    if (shortName != "")
    {
      ss << shortName << ",";
    }
    ss << longName;
    return ss.str();
  }

  template<class T>
  std::string GetOptionDefault(const std::string& option, const std::vector<T>& currValue) const
  {
    auto it = this->ConfigDic.find(option);
    if (it == this->ConfigDic.end())
    {
      std::stringstream ss;
      for (size_t i = 0; i < currValue.size(); i++)
      {
        ss << currValue[i];
        if (i != currValue.size() - 1)
        {
          ss << ",";
        }
      }
      return ss.str();
    }
    return it->second;
  }

  void DeclareOption(cxxopts::OptionAdder& group, const std::string& longName,
    const std::string& shortName, const std::string& doc) const
  {
    group(this->CollapseName(longName, shortName), doc);
  }

  template<class T>
  void DeclareOption(cxxopts::OptionAdder& group, const std::string& longName,
    const std::string& shortName, const std::string& doc, T& var, bool hasDefault = true,
    const std::string& argHelp = "") const
  {
    auto val = cxxopts::value<T>(var);
    if (hasDefault)
    {
      val = val->default_value(this->GetOptionDefault(longName, var));
    }
    var = {};
    group(this->CollapseName(longName, shortName), doc, val, argHelp);
  }

  template<class T>
  void DeclareOptionWithImplicitValue(cxxopts::OptionAdder& group, const std::string& longName,
    const std::string& shortName, const std::string& doc, T& var, const std::string& implicitValue,
    bool hasDefault = true, const std::string& argHelp = "") const
  {
    auto val = cxxopts::value<T>(var)->implicit_value(implicitValue);
    if (hasDefault)
    {
      val = val->default_value(this->GetOptionDefault(longName, var));
    }
    var = {};
    group(this->CollapseName(longName, shortName), doc, val, argHelp);
  }

private:
  int Argc;
  char** Argv;

  using Dictionnary = std::map<std::string, std::string>;
  Dictionnary ConfigDic;
};

//----------------------------------------------------------------------------
F3DOptions ConfigurationOptions::GetOptionsFromArgs(std::vector<std::string>& inputs)
{
  F3DOptions options;
  try
  {
    cxxopts::Options cxxOptions(f3d::AppName, f3d::AppTitle);
    cxxOptions.positional_help("file1 file2 ...");

    // clang-format off
    auto grp1 = cxxOptions.add_options();
    this->DeclareOption(grp1, "input", "", "Input file", inputs, false, "<files>");
    this->DeclareOption(grp1, "output", "", "Render to file", options.Output, false, "<png file>");
    this->DeclareOption(grp1, "no-background", "", "No background when render to file", options.NoBackground);
    this->DeclareOption(grp1, "help", "h", "Print help");
    this->DeclareOption(grp1, "version", "", "Print version details");
    this->DeclareOption(grp1, "verbose", "", "Enable verbose mode", options.Verbose);
    this->DeclareOption(grp1, "no-render", "", "Verbose mode without any rendering, only for the first file", options.NoRender);
    this->DeclareOption(grp1, "axis", "x", "Show axes", options.Axis);
    this->DeclareOption(grp1, "grid", "g", "Show grid", options.Grid);
    this->DeclareOption(grp1, "edges", "e", "Show cell edges", options.Edges);
    this->DeclareOption(grp1, "trackball", "k", "Enable trackball interaction", options.Trackball);
    this->DeclareOption(grp1, "progress", "", "Show progress bar", options.Progress);
    this->DeclareOption(grp1, "up", "", "Up direction", options.Up, true, "[-X|+X|-Y|+Y|-Z|+Z]");
    this->DeclareOption(grp1, "animation-index", "", "Select animation to show", options.AnimationIndex, true, "<index>");
    this->DeclareOption(grp1, "geometry-only", "", "Do not read materials, cameras and lights from file", options.GeometryOnly);
    this->DeclareOption(grp1, "dry-run", "", "Do not read the configuration file", options.DryRun);

    auto grp2 = cxxOptions.add_options("Material");
    this->DeclareOption(grp2, "point-sprites", "o", "Show sphere sprites instead of geometry", options.PointSprites);
    this->DeclareOption(grp2, "point-size", "", "Point size when showing vertices or point sprites", options.PointSize, true, "<size>");
    this->DeclareOption(grp2, "line-width", "", "Line width when showing edges", options.LineWidth, true, "<width>");
    this->DeclareOption(grp2, "color", "", "Solid color", options.SolidColor, true, "<R,G,B>");
    this->DeclareOption(grp2, "opacity", "", "Opacity", options.Opacity, true, "<opacity>");
    this->DeclareOption(grp2, "roughness", "", "Roughness coefficient (0.0-1.0)", options.Roughness, true, "<roughness>");
    this->DeclareOption(grp2, "metallic", "", "Metallic coefficient (0.0-1.0)", options.Metallic, true, "<metallic>");
    this->DeclareOption(grp2, "hdri", "", "Path to an image file that will be used as a light source", options.HDRIFile, false, "<file path>");
    this->DeclareOption(grp2, "texture-base-color", "", "Path to a texture file that sets the color of the object", options.BaseColorTex, false, "<file path>");
    this->DeclareOption(grp2, "texture-material", "", "Path to a texture file that sets the Occlusion, Roughness and Metallic values of the object", options.ORMTex, false, "<file path>");
    this->DeclareOption(grp2, "texture-emissive", "", "Path to a texture file that sets the emited light of the object", options.EmissiveTex, false, "<file path>");
    this->DeclareOption(grp2, "emissive-factor", "", "Emissive factor. This value is multiplied with the emissive color when an emissive texture is present", options.EmissiveFactor, true, "<R,G,B>");
    this->DeclareOption(grp2, "texture-normal", "", "Path to a texture file that sets the normal map of the object", options.NormalTex, false, "<file path>");
    this->DeclareOption(grp2, "normal-scale", "", "Normal scale affects the strength of the normal deviation from the normal texture", options.NormalScale, true, "<normalScale>");

    auto grp3 = cxxOptions.add_options("Window");
    this->DeclareOption(grp3, "bg-color", "", "Background color", options.BackgroundColor, true, "<R,G,B>");
    this->DeclareOption(grp3, "resolution", "", "Window resolution", options.WindowSize, true, "<width,height>");
    this->DeclareOption(grp3, "fps", "z", "Display frame per second", options.FPS);
    this->DeclareOption(grp3, "filename", "n", "Display filename", options.Filename);
    this->DeclareOption(grp3, "metadata", "m", "Display file metadata", options.MetaData);
    this->DeclareOption(grp3, "fullscreen", "f", "Full screen", options.FullScreen);
    this->DeclareOption(grp3, "blur-background", "u", "Blur background", options.BlurBackground);

    auto grp4 = cxxOptions.add_options("Scientific visualization");
    this->DeclareOptionWithImplicitValue(grp4, "scalars", "s", "Color by scalars", options.Scalars, std::string(""), true, "<array_name>");
    this->DeclareOptionWithImplicitValue(grp4, "comp", "y", "Component from the scalar array to color with. -1 means magnitude, -2 or the short option, -y, means direct scalars", options.Component, "-2", true, "<comp_index>");
    this->DeclareOption(grp4, "cells", "c", "Use a scalar array from the cells", options.Cells);
    this->DeclareOption(grp4, "range", "", "Custom range for the coloring by array", options.Range, false, "<min,max>");
    this->DeclareOption(grp4, "bar", "b", "Show scalar bar", options.Bar);
    this->DeclareOption(grp4, "colormap", "", "Specify a custom colormap", options.LookupPoints,
      true, "<color_list>");
    this->DeclareOption(grp4, "volume", "v", "Show volume if the file is compatible", options.Volume);
    this->DeclareOption(grp4, "inverse", "i", "Inverse opacity function for volume rendering", options.InverseOpacityFunction);

    auto grpCamera = cxxOptions.add_options("Camera");
    this->DeclareOption(grpCamera, "camera-position", "", "Camera position", options.CameraPosition, false, "<X,Y,Z>");
    this->DeclareOption(grpCamera, "camera-focal-point", "", "Camera focal point", options.CameraFocalPoint, false, "<X,Y,Z>");
    this->DeclareOption(grpCamera, "camera-view-up", "", "Camera view up", options.CameraViewUp, false, "<X,Y,Z>");
    this->DeclareOption(grpCamera, "camera-view-angle", "", "Camera view angle (non-zero, in degress)", options.CameraViewAngle, false, "<angle>");

#if F3D_HAS_RAYTRACING
    auto grp5 = cxxOptions.add_options("Raytracing");
    this->DeclareOption(grp5, "raytracing", "r", "Enable raytracing", options.Raytracing);
    this->DeclareOption(grp5, "samples", "", "Number of samples per pixel", options.Samples, true, "<samples>");
    this->DeclareOption(grp5, "denoise", "d", "Denoise the image", options.Denoise);
#endif

    auto grp6 = cxxOptions.add_options("PostFX (OpenGL)");
    this->DeclareOption(grp6, "depth-peeling", "p", "Enable depth peeling", options.DepthPeeling);
    this->DeclareOption(grp6, "ssao", "q", "Enable Screen-Space Ambient Occlusion", options.SSAO);
    this->DeclareOption(grp6, "fxaa", "a", "Enable Fast Approximate Anti-Aliasing", options.FXAA);
    this->DeclareOption(grp6, "tone-mapping", "t", "Enable Tone Mapping", options.ToneMapping);

    auto grp7 = cxxOptions.add_options("Testing");
    this->DeclareOption(grp7, "ref", "", "Reference", options.Reference, false, "<png file>");
    this->DeclareOption(grp7, "ref-threshold", "", "Testing threshold", options.RefThreshold, false, "<threshold>");
    // clang-format on

    cxxOptions.parse_positional({ "input" });

    int argc = this->Argc;
    auto result = cxxOptions.parse(argc, this->Argv);

    if (result.count("help") > 0)
    {
      F3DLog::Print(F3DLog::Severity::Info, cxxOptions.help());
      F3DLog::Print(F3DLog::Severity::Info,
        "Keys:\n"
        " S         Toggle the coloration by scalar\n"
        " B         Toggle the scalar bar display\n"
        " P         Toggle depth peeling\n"
        " Q         Toggle SSAO\n"
        " A         Toggle FXAA\n"
        " T         Toggle tone mapping\n"
        " E         Toggle the edges display\n"
        " X         Toggle the axes display\n"
        " G         Toggle the grid display\n"
        " N         Toggle the filename display\n"
        " M         Toggle the metadata display\n"
        " Z         Toggle the FPS counter display\n"
        " R         Toggle raytracing rendering\n"
        " D         Toggle denoising when raytracing\n"
        " V         Toggle volume rendering\n"
        " I         Toggle inverse opacity\n"
        " O         Toggle point sprites rendering\n"
        " F         Toggle full screen\n"
        " U         Toggle blur background\n"
        " K         Toggle trackball interaction\n"
        " H         Toggle Cheat sheet display\n"
        " ?         Dump camera state to the terminal\n"
        " ESC       Quit\n"
        " ENTER     Reset camera to initial parameters\n"
        " SPACE     Play animation if any\n"
        " LEFT      Previous file\n"
        " RIGHT     Next file\n"
        " UP        Reload current file\n"
        );
      exit(EXIT_SUCCESS);
    }

    if (result.count("version") > 0)
    {
      std::string version = f3d::AppTitle;
      version += "\nVersion: ";
      version += f3d::AppVersion;
      version += "\nBuild date: ";
      version += f3d::AppBuildDate;
      version += "\nSystem: ";
      version += f3d::AppBuildSystem;
      version += "\nCompiler: ";
      version += f3d::AppCompiler;
      version += "\nRayTracing module: ";
#if F3D_HAS_RAYTRACING
      version += "ON";
#else
      version += "OFF";
#endif
      version += "\nAuthor: Kitware SAS";

      F3DLog::Print(F3DLog::Severity::Info, version);
      exit(EXIT_SUCCESS);
    }
  }
  catch (const cxxopts::OptionException& e)
  {
    F3DLog::Print(F3DLog::Severity::Error, "Error parsing options: ", e.what());
    exit(EXIT_FAILURE);
  }
  return options;
}

//----------------------------------------------------------------------------
bool ConfigurationOptions::InitializeDictionaryFromConfigFile(const std::string& filePath)
{
  this->ConfigDic.clear();

  const std::string& configFilePath = F3DFileSystem::GetSettingsFilePath(this->Argv[0]);
  if (configFilePath.empty())
  {
    return false;
  }

  std::ifstream file;
  file.open(configFilePath.c_str());

  if (!file.is_open())
  {
    F3DLog::Print(
      F3DLog::Severity::Error, "Unable to open the configuration file ", configFilePath);
    return false;
  }

  Json::Value root;
  Json::CharReaderBuilder builder;
  builder["collectComments"] = false;
  std::string errs;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  bool success = Json::parseFromStream(builder, file, &root, &errs);
  if (!success)
  {
    F3DLog::Print(
      F3DLog::Severity::Error, "Unable to parse the configuration file ", configFilePath);
    F3DLog::Print(F3DLog::Severity::Error, errs);
    return false;
  }

  for (auto const& id : root.getMemberNames())
  {
    std::regex re(id);
    std::smatch matches;
    if (std::regex_match(filePath, matches, re))
    {
      const Json::Value node = root[id];

      for (auto const& nl : node.getMemberNames())
      {
        const Json::Value v = node[nl];
        this->ConfigDic[nl] = v.asString();
      }
    }
  }

  return true;
}

//----------------------------------------------------------------------------
F3DOptionsParser::F3DOptionsParser() = default;

//----------------------------------------------------------------------------
F3DOptionsParser::~F3DOptionsParser() = default;

//----------------------------------------------------------------------------
void F3DOptionsParser::Initialize(int argc, char** argv)
{
  this->ConfigOptions = std::unique_ptr<ConfigurationOptions>(new ConfigurationOptions(argc, argv));
}

//----------------------------------------------------------------------------
F3DOptions F3DOptionsParser::GetOptionsFromCommandLine()
{
  std::vector<std::string> dummy;
  return this->GetOptionsFromCommandLine(dummy);
}

//----------------------------------------------------------------------------
F3DOptions F3DOptionsParser::GetOptionsFromCommandLine(std::vector<std::string>& files)
{
  return this->ConfigOptions->GetOptionsFromArgs(files);
}

//----------------------------------------------------------------------------
F3DOptions F3DOptionsParser::GetOptionsFromConfigFile(const std::string& filePath)
{
  this->ConfigOptions->InitializeDictionaryFromConfigFile(filePath);
  F3DOptions options = this->GetOptionsFromCommandLine();

  // Check the validity of the options
  if (options.Verbose || options.NoRender)
  {
    F3DOptionsParser::CheckValidity(options, filePath);
  }

  return options;
}

//----------------------------------------------------------------------------
bool F3DOptionsParser::CheckValidity(const F3DOptions& options, const std::string& filePath)
{
  bool ret = true;
  F3DOptions defaultOptions;
  bool usingDefaultScene = true;

  if (!options.GeometryOnly)
  {
    std::string ext = vtksys::SystemTools::GetFilenameLastExtension(filePath);
    ext = vtksys::SystemTools::LowerCase(ext);

    if (ext == ".3ds" || ext == ".obj" || ext == ".wrl" || ext == ".gltf" || ext == ".glb")
    {
      usingDefaultScene = false;
    }
  }

  if (!usingDefaultScene)
  {
    if (defaultOptions.MetaData != options.MetaData)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying to show meta data while not using the default scene has no effect.");
      ret = false;
    }
    if (defaultOptions.PointSprites != options.PointSprites)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying to show sphere sprites while not using the default scene has no effect.");
      ret = false;
    }
    if (!::CompareVectors(defaultOptions.SolidColor, options.SolidColor))
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying a Solid Color while not using the default scene has no effect.");
      ret = false;
    }
    if (defaultOptions.Opacity != options.Opacity)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying an Opacity while not using the default scene has no effect.");
      ret = false;
    }
    if (defaultOptions.Roughness != options.Roughness)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying a Roughness coefficient while not using the default scene has no effect.");
      ret = false;
    }
    if (defaultOptions.Metallic != options.Metallic)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying a Metallic coefficient while not using the default scene has no effect.");
      ret = false;
    }
    if (defaultOptions.Scalars != options.Scalars)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying Scalars to color with while not using the default scene has no effect.");
      ret = false;
    }
    if (defaultOptions.Component != options.Component)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying a Component to color with while not using the default scene has no effect.");
      ret = false;
    }
    if (defaultOptions.Cells != options.Cells)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying to color with Cells while not using the default scene has no effect.");
      ret = false;
    }
    if (!::CompareVectors(defaultOptions.Range, options.Range))
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying a Range to color with while not using the default scene has no effect.");
      ret = false;
    }
    if (defaultOptions.Bar != options.Bar)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying to show a scalar Bar while not using the default scene has no effect.");
      ret = false;
    }
    if (!::CompareVectors(defaultOptions.LookupPoints, options.LookupPoints))
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying a custom colormap while not using the default scene has no effect.");
      ret = false;
    }
  }
  else
  {
    if (defaultOptions.AnimationIndex != options.AnimationIndex)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying an Animation Index has no effect while using the default scene.");
      ret = false;
    }

    if (defaultOptions.Scalars == options.Scalars)
    {
      if (defaultOptions.Component != options.Component)
      {
        F3DLog::Print(F3DLog::Severity::Info,
          "Specifying a Component to color with has no effect without specifying Scalars to color "
          "with.");
        ret = false;
      }
      if (defaultOptions.Cells != options.Cells)
      {
        F3DLog::Print(F3DLog::Severity::Info,
          "Specifying to color with Cells has no effect without specifying Scalars to color with.");
        ret = false;
      }
      if (!::CompareVectors(defaultOptions.Range, options.Range))
      {
        F3DLog::Print(F3DLog::Severity::Info,
          "Specifying a Range to color with has no effect without specifying Scalars to color "
          "with.");
        ret = false;
      }
      if (defaultOptions.Bar != options.Bar)
      {
        F3DLog::Print(F3DLog::Severity::Info,
          "Specifying to show a scalar Bar has no effect without specifying Scalars to color "
          "with.");
        ret = false;
      }
    }
  }

  if (options.Volume)
  {
    if (defaultOptions.PointSprites != options.PointSprites)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying to show sphere sprites while using volume rendering has no effect.");
      ret = false;
    }
    if (!::CompareVectors(defaultOptions.SolidColor, options.SolidColor))
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying a Solid Color while using volume rendering has no effect.");
      ret = false;
    }
    if (defaultOptions.Opacity != options.Opacity)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying an Opacity while using volume rendering has no effect.");
      ret = false;
    }
    if (defaultOptions.Roughness != options.Roughness)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying a Roughness coefficient while using volume rendering has no effect.");
      ret = false;
    }
    if (defaultOptions.Metallic != options.Metallic)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying a Metallic coefficient while using volume rendering has no effect.");
      ret = false;
    }
  }
  else
  {
    if (defaultOptions.InverseOpacityFunction != options.InverseOpacityFunction)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying inverse opacity function while not using volume rendering has no effect.");
      ret = false;
    }
  }

  if (options.Raytracing)
  {
    if (defaultOptions.Volume != options.Volume)
    {
      F3DLog::Print(
        F3DLog::Severity::Info, "Specifying to show volume has no effect when using Raytracing.");
      ret = false;
    }
    if (defaultOptions.PointSprites != options.PointSprites)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying to show point sprites has no effect when using Raytracing.");
      ret = false;
    }
    if (defaultOptions.FPS != options.FPS)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying to display the Frame per second counter has no effect when using Raytracing.");
      ret = false;
    }
    if (defaultOptions.DepthPeeling != options.DepthPeeling)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying to render using Depth Peeling has no effect when using Raytracing.");
      ret = false;
    }
    if (defaultOptions.SSAO != options.SSAO)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying to render using SSAO has no effect when using Raytracing.");
      ret = false;
    }
  }
  else
  {
    if (defaultOptions.Samples != options.Samples)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying a Number of samples per pixel has no effect when not using Raytracing.");
      ret = false;
    }
    if (defaultOptions.Denoise != options.Denoise)
    {
      F3DLog::Print(F3DLog::Severity::Info,
        "Specifying to Denoise the image has no effect when not using Raytracing.");
      ret = false;
    }
    if (defaultOptions.PointSprites != options.PointSprites)
    {
      if (defaultOptions.Opacity != options.Opacity)
      {
        F3DLog::Print(
          F3DLog::Severity::Info, "Specifying an Opacity while using point sprites has no effect.");
        ret = false;
      }
      if (defaultOptions.Roughness != options.Roughness)
      {
        F3DLog::Print(F3DLog::Severity::Info,
          "Specifying a Roughness coefficient while using point sprites has no effect.");
        ret = false;
      }
      if (defaultOptions.Metallic != options.Metallic)
      {
        F3DLog::Print(F3DLog::Severity::Info,
          "Specifying a Metallic coefficient while using point sprites has no effect.");
        ret = false;
      }
    }
  }

  if (options.NoBackground && options.Output.empty())
  {
    F3DLog::Print(F3DLog::Severity::Info,
      "Specifying no background while not outputing to file has no effect.");
    ret = false;
  }

  if (!options.HDRIFile.empty())
  {
    if (!::CompareVectors(defaultOptions.BackgroundColor, options.BackgroundColor))
    {
      F3DLog::Print(
        F3DLog::Severity::Info, "Specifying a background color while a HDRI file has no effect.");
      ret = false;
    }
  }

  if (!::CompareVectors(defaultOptions.CameraPosition, options.CameraPosition) &&
    options.CameraPosition.size() != 3)
  {
    F3DLog::Print(
      F3DLog::Severity::Info, "Specifying a camera position of not 3 component has no effect.");
    ret = false;
  }
  if (!::CompareVectors(defaultOptions.CameraFocalPoint, options.CameraFocalPoint) &&
    options.CameraFocalPoint.size() != 3)
  {
    F3DLog::Print(
      F3DLog::Severity::Info, "Specifying a camera focal point of not 3 component has no effect.");
    ret = false;
  }
  if (!::CompareVectors(defaultOptions.CameraViewUp, options.CameraViewUp) &&
    options.CameraViewUp.size() != 3)
  {
    F3DLog::Print(
      F3DLog::Severity::Info, "Specifying a camera view up of not 3 component has no effect.");
    ret = false;
  }
  // No check for --no-render option
  return ret;
}
