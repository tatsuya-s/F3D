/**
 * @class   vtkF3DFBXImporter
 * @brief   Import a FBX file.
 *
 * vtkF3DFBXImporter is a concrete subclass of vtkImporter that reads FBX files.
 *
 * @sa
 * vtkImporter
 * vtkGLTFReader
 */

#ifndef vtkF3DFBXImporter_h
#define vtkF3DFBXImporter_h

#include "vtkImporter.h"
#include "vtkSmartPointer.h" // For SmartPointer

class vtkF3DFBXImporterInternals;

class vtkF3DFBXImporter : public vtkImporter
{
public:
  static vtkF3DFBXImporter* New();

  vtkTypeMacro(vtkF3DFBXImporter, vtkImporter);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  ///@{
  /**
   * Specify the name of the file to read.
   */
  vtkSetStringMacro(FileName);
  vtkGetStringMacro(FileName);
  ///@}

  /**
   * update timestep
   */
  void UpdateTimeStep(double timestep) override;

  /**
   * Get the number of available animations.
   */
  vtkIdType GetNumberOfAnimations() override;

  /**
   * Return the name of the animation.
   */
  std::string GetAnimationName(vtkIdType animationIndex) override;

  ///@{
  /**
   * Enable/Disable/Get the status of specific animations
   */
  void EnableAnimation(vtkIdType animationIndex) override;
  void DisableAnimation(vtkIdType animationIndex) override;
  bool IsAnimationEnabled(vtkIdType animationIndex) override;
  ///@}

  /**
   * Get temporal informations for the currently enabled animations.
   * frameRate is used to define the number of frames for one second of simulation.
   * the three return arguments are defined in this implementation.
   */
  bool GetTemporalInformation(vtkIdType animationIndex, double frameRate, int& nbTimeSteps,
    double timeRange[2], vtkDoubleArray* timeSteps) override;

protected:
  vtkF3DFBXImporter();
  ~vtkF3DFBXImporter() override;

  int ImportBegin() override;
  void ImportActors(vtkRenderer* renderer) override;

  char* FileName = nullptr;

  vtkF3DFBXImporterInternals* Internals;

private:
  vtkF3DFBXImporter(const vtkF3DFBXImporter&) = delete;
  void operator=(const vtkF3DFBXImporter&) = delete;
};

#endif
