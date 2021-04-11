/**
 * @class   vtkF3DAssimpImporter
 * @brief   
 */

#ifndef vtkF3DAssimpImporter_h
#define vtkF3DAssimpImporter_h

#include <vtkImporter.h>
#include <vtkVersion.h>

class vtkF3DAssimpImporterInternal;

class vtkF3DAssimpImporter : public vtkImporter
{
public:
  static vtkF3DAssimpImporter* New();
  vtkTypeMacro(vtkF3DAssimpImporter, vtkImporter);
  void PrintSelf(ostream& os, vtkIndent indent) override;

#if VTK_VERSION_NUMBER >= VTK_VERSION_CHECK(9, 0, 20210118)
  //@{
  /**
   * Get/Set the file name.
   */
  vtkSetStdStringFromCharMacro(FileName);
  vtkGetCharFromStdStringMacro(FileName);
  //@}
#else
  /**
   * Get the filename
   */
  const char* GetFileName()
  {
    return this->FileName.c_str();
  }

  /**
   * Set the filename
   */
  void SetFileName(const char* fileName)
  {
    this->FileName = fileName;
    this->Modified();
  }
#endif

  void UpdateTimeStep(double timestep) override;

  /**
   * Get the number of available animations.
   * Returns 1 if an animation is available or
   * 0 if not.
   */
  vtkIdType GetNumberOfAnimations() override;

  /**
   * Return a dummy name of the first animation if any, empty string otherwise.
   */
  std::string GetAnimationName(vtkIdType animationIndex) override;

  //@{
  /**
   * Enable/Disable/Get the status of specific animations
   * Only the first animation can be enabled
   */
  void EnableAnimation(vtkIdType animationIndex) override;
  void DisableAnimation(vtkIdType animationIndex) override;
  bool IsAnimationEnabled(vtkIdType animationIndex) override;
  //@}

  /**
   * Get temporal informations for the currently enabled animations.
   * the three return arguments can be defined or not.
   * Return true in case of success, false otherwise.
   */
  bool GetTemporalInformation(vtkIdType animationIndex, double frameRate, int& nbTimeSteps,
    double timeRange[2], vtkDoubleArray* timeSteps) override;

protected:
  vtkF3DAssimpImporter();
  ~vtkF3DAssimpImporter() override;

  int ImportBegin() override;
  void ImportActors(vtkRenderer*) override;

  std::string FileName;

private:
  vtkF3DAssimpImporter(const vtkF3DAssimpImporter&) = delete;
  void operator=(const vtkF3DAssimpImporter&) = delete;

  vtkF3DAssimpImporterInternal* Internals = nullptr;
};

#endif
