/**
 * @class   F3DFBXReader
 * @brief   The FBX reader class
 *
 */

#ifndef F3DFBXReader_h
#define F3DFBXReader_h

#include "F3DReaderFactory.h"

#include <vtkF3DFBXImporter.h>

class F3DFBXReader : public F3DReader
{
public:
  F3DFBXReader() = default;

  /*
   * Get the name of this reader
   */
  const std::string GetName() const override { return "FBXReader"; }

  /*
   * Get the short description of this reader
   */
  const std::string GetShortDescription() const override { return "FBX files reader"; }

  /*
   * Get the extensions supported by this reader
   */
  const std::vector<std::string> GetExtensions() const override
  {
    static const std::vector<std::string> ext = { ".fbx" };
    return ext;
  }

  /*
   * Create the scene reader (VTK importer) for the given filename
   */
  vtkSmartPointer<vtkImporter> CreateSceneReader(const std::string& fileName) const override
  {
    vtkSmartPointer<vtkF3DFBXImporter> importer = vtkSmartPointer<vtkF3DFBXImporter>::New();
    importer->SetFileName(fileName.c_str());
    return importer;
  }
};

#endif
