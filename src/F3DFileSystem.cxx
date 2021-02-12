#include "F3DFileSystem.h"

#include <vtksys/SystemTools.hxx>

#include <cstring>

//----------------------------------------------------------------------------
std::string F3DFileSystem::GetUserSettingsDirectory()
{
  std::string applicationName = "f3d";
#if defined(_WIN32)
  const char* appData = vtksys::SystemTools::GetEnv("APPDATA");
  if (!appData)
  {
    return std::string();
  }
  std::string separator("\\");
  std::string directoryPath(appData);
  if (directoryPath[directoryPath.size() - 1] != separator[0])
  {
    directoryPath.append(separator);
  }
  directoryPath += applicationName + separator;
#else
  std::string directoryPath;
  std::string separator("/");

  // Implementing XDG specifications
  const char* xdgConfigHome = vtksys::SystemTools::GetEnv("XDG_CONFIG_HOME");
  if (xdgConfigHome && strlen(xdgConfigHome) > 0)
  {
    directoryPath = xdgConfigHome;
    if (directoryPath[directoryPath.size() - 1] != separator[0])
    {
      directoryPath += separator;
    }
  }
  else
  {
    const char* home = vtksys::SystemTools::GetEnv("HOME");
    if (!home)
    {
      return std::string();
    }
    directoryPath = home;
    if (directoryPath[directoryPath.size() - 1] != separator[0])
    {
      directoryPath += separator;
    }
    directoryPath += ".config/";
  }
  directoryPath += applicationName + separator;
#endif
  return directoryPath;
}

//----------------------------------------------------------------------------
std::string F3DFileSystem::GetSystemSettingsDirectory()
{
  std::string directoryPath = "";
// No support implemented for system wide settings on Windows yet
#ifndef _WIN32
#ifdef __APPLE__
  // Implementing simple /usr/local/etc/ system wide config
  directoryPath = "/usr/local/etc/f3d/";
#else
  // Implementing simple /etc/ system wide config
  directoryPath = "/etc/f3d/";
#endif
#endif
  return directoryPath;
}

//----------------------------------------------------------------------------
std::string F3DFileSystem::GetUserCacheDirectory()
{
  std::string applicationName = "f3d";
#if defined(_WIN32)
  const char* localappData = vtksys::SystemTools::GetEnv("LOCALAPPDATA");
  if (!localappData)
  {
    return std::string();
  }
  std::string separator("\\");
  std::string directoryPath(appData);
  if (directoryPath[directoryPath.size() - 1] != separator[0])
  {
    directoryPath.append(separator);
  }
  directoryPath += applicationName + separator;
#else
  std::string directoryPath;
  std::string separator("/");

  // Implementing XDG specifications
  const char* xdgCacheHome = vtksys::SystemTools::GetEnv("XDG_CACHE_HOME");
  if (xdgCacheHome && strlen(xdgCacheHome) > 0)
  {
    directoryPath = xdgCacheHome;
    if (directoryPath[directoryPath.size() - 1] != separator[0])
    {
      directoryPath += separator;
    }
  }
  else
  {
    const char* home = vtksys::SystemTools::GetEnv("HOME");
    if (!home)
    {
      return std::string();
    }
    directoryPath = home;
    if (directoryPath[directoryPath.size() - 1] != separator[0])
    {
      directoryPath += separator;
    }
    directoryPath += ".cache/";
  }
  directoryPath += applicationName + separator;
#endif
  return directoryPath;
}

//----------------------------------------------------------------------------
std::string F3DFileSystem::GetBinarySettingsDirectory(const char* argv0)
{
  std::string directoryPath = "";
  std::string errorMsg, programFilePath;
  if (vtksys::SystemTools::FindProgramPath(argv0, programFilePath, errorMsg))
  {
    // resolve symlinks
    programFilePath = vtksys::SystemTools::GetRealPath(programFilePath);
    directoryPath = vtksys::SystemTools::GetProgramPath(programFilePath);
    std::string separator;
#if defined(_WIN32)
    separator = "\\";
    if (directoryPath[directoryPath.size() - 1] != separator[0])
    {
      directoryPath.append(separator);
    }
#else
    separator = "/";
    directoryPath += separator;
#endif
    directoryPath += "..";
#ifdef F3D_OSX_BUNDLE
    if (vtksys::SystemTools::FileExists(directoryPath + "/Resources"))
    {
      directoryPath += "/Resources";
    }
#endif
    directoryPath = vtksys::SystemTools::CollapseFullPath(directoryPath);
    directoryPath += separator;
  }
  return directoryPath;
}

//----------------------------------------------------------------------------
std::string F3DFileSystem::GetSettingsFilePath(const char* argv0)
{
  std::string fileName = "config.json";
  std::string filePath = F3DFileSystem::GetUserSettingsDirectory() + fileName;
  if (!vtksys::SystemTools::FileExists(filePath))
  {
    filePath = F3DFileSystem::GetBinarySettingsDirectory(argv0) + fileName;
    if (!vtksys::SystemTools::FileExists(filePath))
    {
      filePath = F3DFileSystem::GetSystemSettingsDirectory() + fileName;
      if (!vtksys::SystemTools::FileExists(filePath))
      {
        filePath = "";
      }
    }
  }
  return filePath;
}