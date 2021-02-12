/**
 * @class   F3DFileSystem
 * @brief   The class that holds and manages options
 *
 */

#ifndef F3DFileSystem_h
#define F3DFileSystem_h

#include "Config.h"

struct F3DFileSystem
{
  static std::string GetBinarySettingsDirectory(const char* argv0);
  static std::string GetSettingsFilePath(const char* argv0);

  static std::string GetSystemSettingsDirectory();
  static std::string GetUserSettingsDirectory();
  static std::string GetUserCacheDirectory();
};

#endif
