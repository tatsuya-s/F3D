#include "vtkF3DMetaReader.h"

#include <vtkCityGMLReader.h>
#include <vtkDICOMImageReader.h>
#include <vtkDemandDrivenPipeline.h>
#include <vtkEventForwarderCommand.h>
#include <vtkGLTFReader.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkMetaImageReader.h>
#include <vtkNrrdReader.h>
#include <vtkOBJReader.h>
#include <vtkObjectFactory.h>
#include <vtkPDataSetReader.h>
#include <vtkPLYReader.h>
#include <vtkPTSReader.h>
#include <vtkSTLReader.h>
#include <vtkTIFFReader.h>
#include <vtkVersion.h>
#include <vtkXMLGenericDataObjectReader.h>
#include <vtksys/SystemTools.hxx>

#include <regex>

vtkStandardNewMacro(vtkF3DMetaReader);

//----------------------------------------------------------------------------
vtkF3DMetaReader::vtkF3DMetaReader()
{
  this->SetNumberOfInputPorts(0);
}

//----------------------------------------------------------------------------
vtkF3DMetaReader::~vtkF3DMetaReader()
{
  this->SetFileName(nullptr);
}

//----------------------------------------------------------------------------
void vtkF3DMetaReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
vtkInformation* vtkF3DMetaReader::GetOutputInformation(int port)
{
  if (!this->InternalReader)
  {
    vtkErrorMacro("InternalReader has not been created yet, "
                  "make sure to set to use a supported file format and to set the FileName");
    return 0;
  }

  return this->InternalReader->GetOutputInformation(port);
}

//----------------------------------------------------------------------------
int vtkF3DMetaReader::ProcessRequest(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  if (!this->InternalReader)
  {
    vtkErrorMacro("InternalReader has not been created yet, "
                  "make sure to set to use a supported file format and to set the FileName");
    return 0;
  }

  return this->InternalReader->ProcessRequest(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
int vtkF3DMetaReader::FillOutputPortInformation(int port, vtkInformation* info)
{
  if (!this->InternalReader)
  {
    vtkErrorMacro("InternalReader has not been created yet, "
                  "make sure to set to use a supported file format and to set the FileName");
    return 0;
  }

  vtkInformation* tmpInfo = this->InternalReader->GetOutputPortInformation(port);
  if (tmpInfo)
  {
    info->Copy(tmpInfo);
    return 1;
  }
  return 0;
}

//----------------------------------------------------------------------------
void vtkF3DMetaReader::SetFileName(const std::string& fileName)
{
  vtkMTimeType time = this->GetMTime();
  this->SetFileName(fileName.c_str());
  if (time != this->GetMTime())
  {
    std::string shortName = vtksys::SystemTools::GetFilenameName(fileName);
    shortName = vtksys::SystemTools::LowerCase(shortName);

    std::string ext = vtksys::SystemTools::GetFilenameLastExtension(fileName);
    ext = vtksys::SystemTools::LowerCase(ext);

    if (!this->InternalReader && ext == ".vtk")
    {
      vtkNew<vtkPDataSetReader> reader;
      reader->SetFileName(this->FileName);
      this->InternalReader = reader;
    }
    if (!this->InternalReader &&
      (ext == ".vtp" || ext == ".vtu" || ext == ".vti" || ext == ".vtr" || ext == ".vts" ||
        ext == ".vtm"))
    {
      vtkNew<vtkXMLGenericDataObjectReader> reader;
      reader->SetFileName(this->FileName);
      this->InternalReader = reader;
    }
    if (!this->InternalReader && ext == ".ply")
    {
      vtkNew<vtkPLYReader> reader;
      if (reader->CanReadFile(this->FileName))
      {
        reader->SetFileName(this->FileName);
        this->InternalReader = reader;
      }
    }
    if (!this->InternalReader && ext == ".stl")
    {
      vtkNew<vtkSTLReader> reader;
#if VTK_VERSION_NUMBER >= VTK_VERSION_CHECK(9, 0, 20200616)
      reader->MergingOff();
#endif
      reader->SetFileName(this->FileName);
      this->InternalReader = reader;
    }
    if (!this->InternalReader && ext == ".dcm")
    {
      vtkNew<vtkDICOMImageReader> reader;
      reader->SetFileName(this->FileName);
      this->InternalReader = reader;
    }
    if (!this->InternalReader && (ext == ".nrrd" || ext == ".nhdr"))
    {
      vtkNew<vtkNrrdReader> reader;
      reader->SetFileName(this->FileName);
      this->InternalReader = reader;
    }
    if (!this->InternalReader && (ext == ".mha" || ext == ".mhd"))
    {
      vtkNew<vtkMetaImageReader> reader;
      reader->SetFileName(this->FileName);
      this->InternalReader = reader;
    }
    if (!this->InternalReader && (ext == ".tiff" || ext == ".tif"))
    {
      vtkNew<vtkTIFFReader> reader;
      reader->SetFileName(this->FileName);
      this->InternalReader = reader;
    }
    if (!this->InternalReader && ext == ".obj")
    {
      vtkNew<vtkOBJReader> reader;
      reader->SetFileName(this->FileName);
      this->InternalReader = reader;
    }
    if (!this->InternalReader && (ext == ".gltf" || ext == ".glb"))
    {
      vtkNew<vtkGLTFReader> reader;
      reader->SetFileName(this->FileName);

      // Enable all animations in the GLTFReader
      reader->SetFrameRate(30);
      reader->ApplyDeformationsToGeometryOn();
      reader->UpdateInformation(); // Read model metadata to get the number of animations
      for (vtkIdType i = 0; i < reader->GetNumberOfAnimations(); i++)
      {
        reader->EnableAnimation(i);
      }
      // It is needed to update the information directly in order to recover it later
      // Not entirely understood, TODO
      reader->UpdateInformation();
      this->InternalReader = reader;
    }
    if (!this->InternalReader && ext == ".gml")
    {
      vtkNew<vtkCityGMLReader> reader;
      reader->SetFileName(this->FileName);
      this->InternalReader = reader;
    }
    if (!this->InternalReader && ext == ".pts")
    {
      vtkNew<vtkPTSReader> reader;
      reader->SetFileName(this->FileName);
      this->InternalReader = reader;
    }

    if (this->InternalReader)
    {
      // forward progress event
      vtkNew<vtkEventForwarderCommand> forwarder;
      forwarder->SetTarget(this);
      this->InternalReader->AddObserver(vtkCommand::ProgressEvent, forwarder);
    }
  }
}
