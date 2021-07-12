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

#include "ofbx.h"

vtkStandardNewMacro(vtkF3DFBXImporter);

class vtkF3DFBXImporterInternals
{
public:
  void ConvertMatrix(const ofbx::Matrix& in, vtkMatrix4x4* out)
  {
    for (int i = 0; i < 16; i++)
    {
      out->SetElement(i%4, i/4, in.m[i]);
    }
  }

  ofbx::IScene* Scene = nullptr;
};

//------------------------------------------------------------------------------
vtkF3DFBXImporter::vtkF3DFBXImporter()
{
  this->Internals = new vtkF3DFBXImporterInternals;
}

//------------------------------------------------------------------------------
vtkF3DFBXImporter::~vtkF3DFBXImporter()
{
  if (this->Internals->Scene)
  {
    this->Internals->Scene->destroy();
  }

  delete this->Internals;
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

  std::ifstream in(this->FileName);
  std::vector<char> contents((std::istreambuf_iterator<char>(in)),
    std::istreambuf_iterator<char>());

  const ofbx::u8* data = reinterpret_cast<ofbx::u8*>(contents.data());

  this->Internals->Scene = ofbx::load(data, contents.size(), 0);

  return 1;
}

//------------------------------------------------------------------------------
void vtkF3DFBXImporter::ImportActors(vtkRenderer* renderer)
{
  if (this->Internals->Scene)
  {
    int nbMeshes = this->Internals->Scene->getMeshCount();

    for (int i = 0; i < nbMeshes; i++)
    {
      const ofbx::Mesh* mesh = this->Internals->Scene->getMesh(i);
      const ofbx::Geometry* geometry = mesh->getGeometry();

      // points
      vtkNew<vtkPolyData> polyData;
      vtkNew<vtkPoints> points;

      int nbPoints = geometry->getVertexCount();
      const ofbx::Vec3* fbxPoints = geometry->getVertices();

      points->SetNumberOfPoints(nbPoints);

      for (int j = 0; j < nbPoints; j++)
      {
        const ofbx::Vec3& p = fbxPoints[j];
        points->SetPoint(j, p.x, p.y, p.z);
      }

      // normals
      const ofbx::Vec3* fbxNormals = geometry->getNormals();

      if (fbxNormals)
      {
        vtkNew<vtkDoubleArray> normals;
        normals->SetNumberOfComponents(3);
        normals->SetNumberOfTuples(nbPoints);
        normals->SetName("Normals");

        for (int j = 0; j < nbPoints; j++)
        {
          const ofbx::Vec3& n = fbxNormals[j];
          double tuple[] = { n.x, n.y, n.z };
          normals->SetTypedTuple(j, tuple);
        }

        polyData->GetPointData()->SetNormals(normals);
      }

      // Tangents
      const ofbx::Vec3* fbxTangents = geometry->getTangents();

      if (fbxTangents)
      {
        vtkNew<vtkDoubleArray> tangents;
        tangents->SetNumberOfComponents(3);
        tangents->SetNumberOfTuples(nbPoints);
        tangents->SetName("Tangents");

        for (int j = 0; j < nbPoints; j++)
        {
          const ofbx::Vec3& t = fbxTangents[j];
          double tuple[] = { t.x, t.y, t.z };
          tangents->SetTypedTuple(j, tuple);
        }

        polyData->GetPointData()->SetTangents(tangents);
      }

      // uvs
      const ofbx::Vec2* fbxUVs = geometry->getUVs();

      if (fbxUVs)
      {
        vtkNew<vtkDoubleArray> uvs;
        uvs->SetNumberOfComponents(2);
        uvs->SetNumberOfTuples(nbPoints);
        uvs->SetName("uvs");

        for (int j = 0; j < nbPoints; j++)
        {
          const ofbx::Vec2& uv = fbxUVs[j];
          double tuple[] = { uv.x, uv.y };
          uvs->SetTypedTuple(j, tuple);
        }

        polyData->GetPointData()->SetTCoords(uvs);
      }

      // faces
      vtkNew<vtkCellArray> cells;

      int nbIndices = geometry->getIndexCount();
      const int* fbxIndices = geometry->getFaceIndices();

      vtkNew<vtkIdList> poly;

      for (int j = 0; j < nbIndices; j++)
      {
        if (fbxIndices[j] < 0)
        {
          poly->InsertNextId(-fbxIndices[j]-1);
          cells->InsertNextCell(poly);
          poly->Reset();
        }
        else
        {
          poly->InsertNextId(fbxIndices[j]);
        }
      }

      polyData->SetPoints(points);
      polyData->SetPolys(cells);

      vtkNew<vtkPolyDataMapper> mapper;
      mapper->SetInputData(polyData);

      vtkNew<vtkActor> actor;
      actor->SetMapper(mapper);

      vtkNew<vtkMatrix4x4> matrix;
      this->Internals->ConvertMatrix(mesh->getGeometricMatrix(), matrix);
      actor->SetUserMatrix(matrix);

      renderer->AddActor(actor);
    }
  }
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
