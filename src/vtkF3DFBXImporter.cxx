#include "vtkF3DFBXImporter.h"

#include "vtkActor.h"
#include "vtkCamera.h"
#include "vtkDoubleArray.h"
#include "vtkEventForwarderCommand.h"
#include "vtkFloatArray.h"
#include "vtkInformation.h"
#include "vtkLight.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkPolyDataMapper.h"
#include "vtkPolyDataTangents.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkShaderProperty.h"
#include "vtkSmartPointer.h"
#include "vtkTexture.h"
#include "vtkTransform.h"
#include "vtkUniforms.h"
#include "vtksys/SystemTools.hxx"

vtkStandardNewMacro(vtkF3DFBXImporter);

//------------------------------------------------------------------------------
vtkF3DFBXImporter::~vtkF3DFBXImporter()
{
  this->SetFileName(nullptr);
}

//------------------------------------------------------------------------------
int vtkF3DFBXImporter::ImportBegin()
{
  // Make sure we have a file to read.
  if (!this->FileName)
  {
    vtkErrorMacro("A FileName must be specified.");
    return 0;
  }

  return 1;
}

//------------------------------------------------------------------------------
void vtkF3DFBXImporter::ImportActors(vtkRenderer* renderer)
{

}

//----------------------------------------------------------------------------
void vtkF3DFBXImporter::UpdateTimeStep(double timestep)
{

}

//----------------------------------------------------------------------------
vtkIdType vtkF3DFBXImporter::GetNumberOfAnimations()
{
  return 0;
}

//----------------------------------------------------------------------------
std::string vtkF3DFBXImporter::GetAnimationName(vtkIdType animationIndex)
{
  return "";
}

//----------------------------------------------------------------------------
void vtkF3DFBXImporter::EnableAnimation(vtkIdType animationIndex)
{
}

//----------------------------------------------------------------------------
void vtkF3DFBXImporter::DisableAnimation(vtkIdType animationIndex)
{
}

//----------------------------------------------------------------------------
bool vtkF3DFBXImporter::IsAnimationEnabled(vtkIdType animationIndex)
{
  return false;
}

//----------------------------------------------------------------------------
bool vtkF3DFBXImporter::GetTemporalInformation(vtkIdType animationIndex, double frameRate,
  int& nbTimeSteps, double timeRange[2], vtkDoubleArray* timeSteps)
{
  return false;
}

//------------------------------------------------------------------------------
void vtkF3DFBXImporter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "File Name: " << (this->FileName ? this->FileName : "(none)") << "\n";
}
