/**
 * @class   F3DLoader
 * @brief   The loader class
 *
 */

#ifndef F3DLoader_h
#define F3DLoader_h

#include <vtkNew.h>
#include <vtkSmartPointer.h>

#include "vtkF3DRenderer.h"
#include "F3DAnimationManager.h"

#include <android_native_app_glue.h>

class vtkF3DRenderer;
class vtkImporter;
struct F3DOptions;

class F3DLoader
{
public:
  enum LoadFileEnum
  {
    LOAD_PREVIOUS = -1,
    LOAD_CURRENT = 0,
    LOAD_NEXT = 1
  };

  enum vtkCustomEvents
  {
    NewFilesEvent = vtkCommand::UserEvent + 100,
    LoadFileEvent,
    ToggleAnimationEvent
  };

  /**
   * Parse the options, create an importer and start the rendering
   */
  int Start(int argc, char** argv);

  /**
   * Add a list of files or directory to be loaded
   */
  void AddFiles(const std::vector<std::string>& files);

  /**
   * Add a file or directory to be loaded
   * Set recursive to true to add all the file in a directory
   */
  void AddFile(const std::string& path, bool recursive = true);

  /**
   * Load a file if any have been added
   * Set the load arguement to LOAD_PREVIOUS or LOAD_NEXT to change file index
   */
  void LoadFile(int load = F3DLoader::LOAD_CURRENT);

  F3DLoader();
  ~F3DLoader();

  static struct android_app* AndroidState;

protected:
  static vtkSmartPointer<vtkImporter> GetImporter(
    const F3DOptions& options, const std::string& file);

  std::vector<std::string> FilesList;
  int CurrentFileIndex = 0;
  F3DOptionsParser Parser;
  F3DOptions Options;
  F3DAnimationManager AnimationManager;
  vtkSmartPointer<vtkF3DRenderer> Renderer;
  vtkSmartPointer<vtkImporter> Importer;
  vtkSmartPointer<vtkRenderWindow> RenWin;

private:
  F3DLoader(F3DLoader const&) = delete;
  void operator=(F3DLoader const&) = delete;
};

#endif
