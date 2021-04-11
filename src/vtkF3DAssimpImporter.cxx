#include "vtkF3DAssimpImporter.h"

#include <vtkObjectFactory.h>
#include <vtkMatrix4x4.h>
#include <vtkImageData.h>
#include <vtkActor.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderer.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkFloatArray.h>
#include <vtkSmartPointer.h>
#include <vtkPointData.h>
#include <vtkTexture.h>
#include <vtkImageReader2Factory.h>
#include <vtkImageReader2.h>
#include <vtkDoubleArray.h>
#include <vtkPNGReader.h>
#include <vtkJPEGReader.h>
#include <vtkStringArray.h>
#include <vtkQuaternion.h>
#include <vtkActorCollection.h>
#include <vtkUnsignedShortArray.h>
#include <vtkShaderProperty.h>
#include <vtkUniforms.h>
#include <vtkXMLPolyDataWriter.h>

#include <vtksys/SystemTools.hxx>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

vtkStandardNewMacro(vtkF3DAssimpImporter);

class vtkF3DAssimpImporterInternal
{
public:
  //----------------------------------------------------------------------------
  vtkF3DAssimpImporterInternal(vtkF3DAssimpImporter* parent)
  {
    this->Parent = parent;
  }

  //----------------------------------------------------------------------------
  vtkSmartPointer<vtkTexture> CreateTexture(const char* path, bool sRGB = false)
  {
    vtkSmartPointer<vtkTexture> texture;

    if (path[0] == '*')
    {
      int texIndex = std::atoi(path + 1);
      texture = this->EmbeddedTextures[texIndex];
    }
    else
    {
      // sometimes, embedded textures are indexed by filename
      const aiTexture* tex = this->Scene->GetEmbeddedTexture(path);

      if (tex)
      {
        texture = this->CreateEmbeddedTexture(tex);
      }
      else
      {
        std::string dir = vtksys::SystemTools::GetParentDirectory(this->Parent->GetFileName());
        std::string texPath = vtksys::SystemTools::CollapseFullPath(path, dir);

        if (vtksys::SystemTools::FileExists(texPath))
        {
          vtkSmartPointer<vtkImageReader2> reader;
          reader.TakeReference(vtkImageReader2Factory::CreateImageReader2(texPath.c_str()));
          reader->SetFileName(texPath.c_str());
          reader->Update();

          texture = vtkSmartPointer<vtkTexture>::New();
          texture->SetInputConnection(reader->GetOutputPort());
          texture->Update();
        }
        else
        {
          vtkWarningWithObjectMacro(this->Parent, "Cannot find texture: " << texPath);
          return nullptr;
        }
      }
    }

    texture->MipmapOn();
    texture->InterpolateOn();
    texture->SetUseSRGBColorSpace(sRGB);

    return texture;
  }

  //----------------------------------------------------------------------------
  vtkSmartPointer<vtkTexture> CreateEmbeddedTexture(const aiTexture* texture)
  {
    vtkNew<vtkTexture> oTexture;

    if (texture->mHeight == 0)
    {
      std::string fileType = texture->achFormatHint;

      // unfortunately, vtkImageReader2Factory::CreateImageReader2 does not work if the file does
      // not exist, so we have to reproduce the logic ourselves
      vtkSmartPointer<vtkImageReader2> reader;

      if (fileType == "png")
      {
        reader = vtkSmartPointer<vtkPNGReader>::New();
      }
      else if (fileType == "jpg")
      {
        reader = vtkSmartPointer<vtkJPEGReader>::New();
      }

      if (reader)
      {
        reader->SetMemoryBuffer(texture->pcData);
        reader->SetMemoryBufferLength(texture->mWidth);
      }

      oTexture->SetInputConnection(reader->GetOutputPort());
    }
    else
    {
      vtkNew<vtkImageData> img;
      img->SetDimensions(texture->mWidth, texture->mHeight, 1);
      img->AllocateScalars(VTK_UNSIGNED_CHAR, 4);

      unsigned char* p = reinterpret_cast<unsigned char *>(img->GetScalarPointer());
      std::copy(p, p + 4 * texture->mWidth * texture->mHeight, reinterpret_cast<unsigned char *>(texture->pcData));

      oTexture->SetInputData(img);
    }

    return oTexture;
  }

  //----------------------------------------------------------------------------
  vtkSmartPointer<vtkProperty> CreateMaterial(const aiMaterial* material)
  {
    vtkNew<vtkProperty> property;

    // todo: remove
    for (unsigned int i = 0; i < material->mNumProperties; i++)
    {
      aiMaterialProperty* matProp = material->mProperties[i];
      std::cout << matProp->mKey.data << " " << matProp->mSemantic << std::endl;
    }
    std::cout << std::endl;

    int shadingModel;
    if (material->Get(AI_MATKEY_SHADING_MODEL, shadingModel) == aiReturn_SUCCESS)
    {
      switch (shadingModel)
      {
        case aiShadingMode_Flat:
          property->SetInterpolationToFlat();
          break;
        case aiShadingMode_Gouraud:
        case aiShadingMode_Phong:
        case aiShadingMode_Blinn:
        case aiShadingMode_Minnaert:
          property->SetInterpolationToPhong();
          break;
        case aiShadingMode_OrenNayar:
        case aiShadingMode_CookTorrance:
        case aiShadingMode_Fresnel:
          property->SetInterpolationToPBR();
          break;
        case aiShadingMode_Toon:
        case aiShadingMode_NoShading:
          property->LightingOff();
      }
    }

    ai_real opacity;
    if (material->Get(AI_MATKEY_OPACITY, opacity) == aiReturn_SUCCESS)
    {
      property->SetOpacity(opacity);
    }

    aiColor4D diffuse;
    if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == aiReturn_SUCCESS)
    {
      property->SetColor(diffuse.r, diffuse.g, diffuse.b);
    }

    aiColor4D specular;
    if (material->Get(AI_MATKEY_COLOR_SPECULAR, specular) == aiReturn_SUCCESS)
    {
      property->SetSpecularColor(specular.r, specular.g, specular.b);
    }

    aiColor4D ambient;
    if (material->Get(AI_MATKEY_COLOR_AMBIENT, ambient) == aiReturn_SUCCESS)
    {
      property->SetAmbientColor(ambient.r, ambient.g, ambient.b);
    }

    aiString texDiffuse;
    if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texDiffuse) == aiReturn_SUCCESS)
    {
      vtkSmartPointer<vtkTexture> tex = this->CreateTexture(texDiffuse.data);
      if (tex)
      {
        property->SetTexture("diffuseTex", tex);
      }
    }

    aiString texNormal;
    if (material->GetTexture(aiTextureType_NORMALS, 0, &texNormal) == aiReturn_SUCCESS)
    {
      vtkSmartPointer<vtkTexture> tex = this->CreateTexture(texNormal.data);
      if (tex)
      {
        property->SetTexture("normalTex", tex);
      }
    }

    aiString texAlbedo;
    if (material->GetTexture(aiTextureType_BASE_COLOR, 0, &texAlbedo) == aiReturn_SUCCESS)
    {
      vtkSmartPointer<vtkTexture> tex = this->CreateTexture(texAlbedo.data, true);
      if (tex)
      {
        property->SetTexture("albedoTex", tex);
      }
    }

    aiString texEmissive;
    if (material->GetTexture(aiTextureType_EMISSIVE, 0, &texEmissive) == aiReturn_SUCCESS)
    {
      vtkSmartPointer<vtkTexture> tex = this->CreateTexture(texEmissive.data, true);
      if (tex)
      {
        property->SetTexture("emissiveTex", tex);
      }
    }

/*
TODO:

    "materialTex"
    aiTextureType_METALNESS = 15,
    aiTextureType_DIFFUSE_ROUGHNESS = 16,
    aiTextureType_AMBIENT_OCCLUSION = 17,

    aiString texnormal;
    if (aiGetMaterialTexture(material, aiTextureType_NORMALS, 0, &texnormal) == aiReturn_SUCCESS)
    {
      property->SetTexture("normalTex", this->CreateTexture(texnormal.data));
    }
    */

    return property;
  }

  //----------------------------------------------------------------------------
  vtkSmartPointer<vtkPolyData> CreateMesh(const aiMesh* mesh)
  {
    vtkNew<vtkPolyData> polyData;

    vtkNew<vtkPoints> points;
    points->SetNumberOfPoints(mesh->mNumVertices);
    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
      const aiVector3D& p = mesh->mVertices[i];
      points->SetPoint(i, p.x, p.y, p.z);
    }
    polyData->SetPoints(points);

    if (mesh->HasNormals())
    {
      vtkNew<vtkFloatArray> normals;
      normals->SetNumberOfComponents(3);
      normals->SetName("Normal");
      normals->SetNumberOfTuples(mesh->mNumVertices);
      for (unsigned int i = 0; i < mesh->mNumVertices; i++)
      {
        const aiVector3D& n = mesh->mNormals[i];
        float tuple[3] = { n.x, n.y, n.z };
        normals->SetTypedTuple(i, tuple);
      }
      polyData->GetPointData()->SetNormals(normals);
    }

    // currently, VTK only supports 1 texture coordinates
    const unsigned int textureIndex = 0;
    if (mesh->HasTextureCoords(textureIndex) && mesh->mNumUVComponents[textureIndex] == 2)
    {
      vtkNew<vtkFloatArray> tcoords;
      tcoords->SetNumberOfComponents(2);
      tcoords->SetName("UV");
      tcoords->SetNumberOfTuples(mesh->mNumVertices);
      for (unsigned int i = 0; i < mesh->mNumVertices; i++)
      {
        const aiVector3D& t = mesh->mTextureCoords[textureIndex][i];
        float tuple[2] = { t.x, t.y };
        tcoords->SetTypedTuple(i, tuple);
      }
      polyData->GetPointData()->SetTCoords(tcoords);
    }

    if (mesh->HasTangentsAndBitangents())
    {
      vtkNew<vtkFloatArray> tangents;
      tangents->SetNumberOfComponents(3);
      tangents->SetName("Tangents");
      tangents->SetNumberOfTuples(mesh->mNumVertices);
      for (unsigned int i = 0; i < mesh->mNumVertices; i++)
      {
        const aiVector3D& t = mesh->mTangents[i];
        float tuple[3] = { t.x, t.y, t.z };
        tangents->SetTypedTuple(i, tuple);
      }
      polyData->GetPointData()->SetTangents(tangents);
    }

    if (mesh->HasVertexColors(0))
    {
      vtkNew<vtkFloatArray> colors;
      colors->SetNumberOfComponents(4);
      colors->SetName("Colors");
      colors->SetNumberOfTuples(mesh->mNumVertices);
      for (unsigned int i = 0; i < mesh->mNumVertices; i++)
      {
        const aiColor4D& c = mesh->mColors[0][i];
        float tuple[4] = { c.r, c.g, c.b, c.a };
        colors->SetTypedTuple(i, tuple);
      }
      polyData->GetPointData()->SetScalars(colors);
    }

    vtkNew<vtkCellArray> verticesCells;
    vtkNew<vtkCellArray> linesCells;
    vtkNew<vtkCellArray> polysCells;

    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
      const aiFace& face = mesh->mFaces[i];

      if (face.mNumIndices == 1)
      {
        vtkIdType vId = face.mIndices[0];
        verticesCells->InsertNextCell(1, &vId);
      }
      else if (face.mNumIndices == 2)
      {
        vtkIdType lId[2] = { face.mIndices[0], face.mIndices[1] };
        linesCells->InsertNextCell(2, lId);
      }
      else
      {
        vtkIdType fId[AI_MAX_FACE_INDICES];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
        {
          fId[j] = face.mIndices[j];
        }
        polysCells->InsertNextCell(face.mNumIndices, fId);
      }
    }

    polyData->SetVerts(verticesCells);
    polyData->SetLines(linesCells);
    polyData->SetPolys(polysCells);

    //cout << mesh->mName.data << endl;
    //cout << mesh->mNumAnimMeshes << endl;

    if (mesh->mNumBones > 0)
    {
      struct SkinData
      {
        unsigned short boneId[4] = { 0, 0, 0, 0 };
        float weight[4] = { 0, 0, 0, 0 };
        unsigned int nb = 0;
      };

      std::vector<SkinData> skinPoints(mesh->mNumVertices);

      vtkNew<vtkStringArray> bonesList;
      bonesList->SetName("Bones");

      vtkNew<vtkDoubleArray> bonesTransform;
      bonesTransform->SetName("InverseBindMatrices");
      bonesTransform->SetNumberOfComponents(16);

      for (unsigned int i = 0; i < mesh->mNumBones; i++)
      {
        aiBone* bone = mesh->mBones[i];

        bonesList->InsertValue(i, bone->mName.data);

        for (unsigned int j = 0; j < bone->mNumWeights; j++)
        {
          const aiVertexWeight& vw = bone->mWeights[j];

          SkinData& data = skinPoints[vw.mVertexId];
          if (data.nb >= 4)
          {
            continue;
          }
          data.boneId[data.nb] = i;
          data.weight[data.nb] = vw.mWeight;
          data.nb++;
        }

        vtkNew<vtkMatrix4x4> ibm;
        ConvertMatrix(bone->mOffsetMatrix, ibm);

        bonesTransform->InsertNextTypedTuple(ibm->GetData());
      }

      vtkNew<vtkFloatArray> weights;
      weights->SetName("WEIGHTS_0");
      weights->SetNumberOfComponents(4);
      weights->SetNumberOfTuples(mesh->mNumVertices);

      vtkNew<vtkUnsignedShortArray> boneIds;
      boneIds->SetName("JOINTS_0");
      boneIds->SetNumberOfComponents(4);
      boneIds->SetNumberOfTuples(mesh->mNumVertices);

      for (unsigned int i = 0; i < mesh->mNumVertices; i++)
      {
        for (int j = 0; j < 4; j++)
        {
          weights->SetTypedComponent(i, j, skinPoints[i].weight[j]);
          boneIds->SetTypedComponent(i, j, skinPoints[i].boneId[j]);
        }
      }

      polyData->GetPointData()->AddArray(weights);
      polyData->GetPointData()->AddArray(boneIds);

      polyData->GetFieldData()->AddArray(bonesList);
      polyData->GetFieldData()->AddArray(bonesTransform);
    }

    return polyData;
  }

  //----------------------------------------------------------------------------
  void ReadScene(const std::string& filePath)
  {
   // this->Importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);

    this->Scene = this->Importer.ReadFile(filePath,
      aiProcess_CalcTangentSpace |
      aiProcess_Triangulate      |
      aiProcess_LimitBoneWeights |
      aiProcess_SortByPType);

    if (this->Scene && this->Scene->mNumMeshes > 0)
    {
      // convert meshes to polyData
      this->Meshes.resize(this->Scene->mNumMeshes);
      for (unsigned int i = 0; i < this->Scene->mNumMeshes; i++)
      {
        this->Meshes[i] = this->CreateMesh(this->Scene->mMeshes[i]);
      }

      // read embedded textures
      this->EmbeddedTextures.resize(this->Scene->mNumTextures);
      for (unsigned int i = 0; i < this->Scene->mNumTextures; i++)
      {
        this->EmbeddedTextures[i] = this->CreateEmbeddedTexture(this->Scene->mTextures[i]);
      }

      // convert materials to properties
      this->Properties.resize(this->Scene->mNumMeshes);
      for (unsigned int i = 0; i < this->Scene->mNumMaterials; i++)
      {
        this->Properties[i] = this->CreateMaterial(this->Scene->mMaterials[i]);
      }

      // enable all animations by default
      this->EnabledAnimations.resize(this->Scene->mNumAnimations, true);
    }
  }

  //----------------------------------------------------------------------------
  void ConvertMatrix(const aiMatrix4x4& aMat, vtkMatrix4x4* vMat)
  {
    vMat->SetElement(0, 0, aMat.a1);
    vMat->SetElement(0, 1, aMat.a2);
    vMat->SetElement(0, 2, aMat.a3);
    vMat->SetElement(0, 3, aMat.a4);
    vMat->SetElement(1, 0, aMat.b1);
    vMat->SetElement(1, 1, aMat.b2);
    vMat->SetElement(1, 2, aMat.b3);
    vMat->SetElement(1, 3, aMat.b4);
    vMat->SetElement(2, 0, aMat.c1);
    vMat->SetElement(2, 1, aMat.c2);
    vMat->SetElement(2, 2, aMat.c3);
    vMat->SetElement(2, 3, aMat.c4);
    vMat->SetElement(3, 0, aMat.d1);
    vMat->SetElement(3, 1, aMat.d2);
    vMat->SetElement(3, 2, aMat.d3);
    vMat->SetElement(3, 3, aMat.d4);
  }

  //----------------------------------------------------------------------------
  void ImportNode(vtkRenderer* renderer, const aiNode* node, vtkMatrix4x4* parentMat, int level = 0)
  {
    vtkNew<vtkMatrix4x4> mat;
    vtkNew<vtkMatrix4x4> localMat;

    this->ConvertMatrix(node->mTransformation, localMat);

    vtkMatrix4x4::Multiply4x4(parentMat, localMat, mat);

    vtkNew<vtkActorCollection> actors;

    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
      vtkNew<vtkActor> actor;
      vtkNew<vtkPolyDataMapper> mapper;
      mapper->SetInputData(this->Meshes[node->mMeshes[i]]);
      actor->SetMapper(mapper);
      actor->SetUserMatrix(mat);
      actor->SetProperty(this->Properties[this->Scene->mMeshes[i]->mMaterialIndex]);

      renderer->AddActor(actor);
      actors->AddItem(actor);
    }

    for (int i = 0; i < level; i++) std::cout << " ";
    std::cout << node->mName.data << " : " << node->mNumMeshes << std::endl;

    this->NodeActors.insert({ node->mName.data, actors });
    this->NodeLocalMatrix.insert({ node->mName.data, localMat });
    this->NodeGlobalMatrix.insert({ node->mName.data, mat });

    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
      this->ImportNode(renderer, node->mChildren[i], mat, level + 1);
    }
  }

  //----------------------------------------------------------------------------
  void ImportRoot(vtkRenderer* renderer)
  {
    if (this->Scene)
    {
      vtkNew<vtkMatrix4x4> identity;
      this->ImportNode(renderer, this->Scene->mRootNode, identity);
    }
  }

  //----------------------------------------------------------------------------
  void UpdateNodeTransform(const aiNode* node, const vtkMatrix4x4* parentMat)
  {
    vtkSmartPointer<vtkMatrix4x4> localMat = this->NodeLocalMatrix[node->mName.data];

    vtkNew<vtkMatrix4x4> mat;
    vtkMatrix4x4::Multiply4x4(parentMat, localMat, mat);

    this->NodeGlobalMatrix[node->mName.data] = mat;

    // update current node actors
    vtkActorCollection* actors = this->NodeActors[node->mName.data];
    actors->InitTraversal();

    vtkActor* actor = nullptr;
    while (actor = actors->GetNextActor())
    {
      actor->SetUserMatrix(mat);
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
      this->UpdateNodeTransform(node->mChildren[i], mat);
    }
  }

  //----------------------------------------------------------------------------
  void ClearBones()
  {
    for (auto& pairsActor : NodeActors)
    {
      vtkActorCollection* actors = pairsActor.second;
      actors->InitTraversal();

      vtkActor* actor = nullptr;
      while (actor = actors->GetNextActor())
      {
        vtkPolyDataMapper* mapper = vtkPolyDataMapper::SafeDownCast(actor->GetMapper());

        if (mapper)
        {
          vtkPolyData* polyData = mapper->GetInput();

          if (polyData)
          {
            vtkStringArray* bonesList = vtkStringArray::SafeDownCast(polyData->GetFieldData()->GetAbstractArray("Bones"));
            if (bonesList)
            {
              vtkIdType nbBones = bonesList->GetNumberOfValues();

              for (vtkIdType i = 0; i < nbBones; i++)
              {
                std::string boneName = bonesList->GetValue(i);
                this->NodeLocalMatrix[boneName]->Identity();
              }
            }
          }
        }
      }
    }
  }

  //----------------------------------------------------------------------------
  void UpdateBones()
  {
    vtkNew<vtkMatrix4x4> inverseRoot;
    ConvertMatrix(this->Scene->mRootNode->mTransformation, inverseRoot);
    inverseRoot->Invert();

    for (auto& pairsActor : NodeActors)
    {
      vtkActorCollection* actors = pairsActor.second;
      actors->InitTraversal();

      vtkActor* actor = nullptr;
      while (actor = actors->GetNextActor())
      {
        vtkPolyDataMapper* mapper = vtkPolyDataMapper::SafeDownCast(actor->GetMapper());

        if (mapper)
        {
          vtkPolyData* polyData = mapper->GetInput();

          if (polyData)
          {
            vtkStringArray* bonesList = vtkStringArray::SafeDownCast(polyData->GetFieldData()->GetAbstractArray("Bones"));
            vtkDoubleArray* bonesTransform = vtkDoubleArray::SafeDownCast(polyData->GetFieldData()->GetArray("InverseBindMatrices"));
            if (bonesList && bonesTransform)
            {
              vtkIdType nbBones = bonesList->GetNumberOfValues();

              if (nbBones > 0)
              {
                std::vector<float> vec;
                vec.reserve(16 * nbBones);

                for (vtkIdType i = 0; i < nbBones; i++)
                {
                  std::string boneName = bonesList->GetValue(i);

                  // std::cout << "bone: " << boneName << std::endl;

                  vtkNew<vtkMatrix4x4> boneMat;
                  bonesTransform->GetTypedTuple(i, boneMat->GetData());

                  vtkMatrix4x4::Multiply4x4(this->NodeGlobalMatrix[boneName], boneMat, boneMat);
                  vtkMatrix4x4::Multiply4x4(inverseRoot, boneMat, boneMat);

                  for (int j = 0; j < 4; j++)
                  {
                    for (int k = 0; k < 4; k++)
                    {
                      vec.push_back(static_cast<float>(boneMat->GetElement(k, j)));
                    }
                  }
                }

                vtkShaderProperty* shaderProp = actor->GetShaderProperty();
                vtkUniforms* uniforms = shaderProp->GetVertexCustomUniforms();
                uniforms->RemoveAllUniforms();
                uniforms->SetUniformMatrix4x4v(
                  "jointMatrices", static_cast<int>(nbBones), vec.data());
              }
            }
          }
        }
      }
    }
  }

  Assimp::Importer Importer;
  const aiScene* Scene;
  std::vector<vtkSmartPointer<vtkPolyData>> Meshes;
  std::vector<vtkSmartPointer<vtkProperty>> Properties;
  std::vector<vtkSmartPointer<vtkTexture>> EmbeddedTextures;
  std::vector<bool> EnabledAnimations;
  std::unordered_map<std::string, vtkSmartPointer<vtkActorCollection>> NodeActors;
  std::unordered_map<std::string, vtkSmartPointer<vtkMatrix4x4>> NodeLocalMatrix;
  std::unordered_map<std::string, vtkSmartPointer<vtkMatrix4x4>> NodeTRSMatrix;
  std::unordered_map<std::string, vtkSmartPointer<vtkMatrix4x4>> NodeGlobalMatrix;
  vtkF3DAssimpImporter* Parent;
};

vtkF3DAssimpImporter::vtkF3DAssimpImporter()
{
  this->Internals = new vtkF3DAssimpImporterInternal(this);
}

vtkF3DAssimpImporter::~vtkF3DAssimpImporter()
{
  delete this->Internals;
}

int vtkF3DAssimpImporter::ImportBegin()
{
  this->Internals->ReadScene(this->FileName);

  return 1;
}

void vtkF3DAssimpImporter::ImportActors(vtkRenderer* renderer)
{
  this->Internals->ImportRoot(renderer);
}

#include <set>

void vtkF3DAssimpImporter::UpdateTimeStep(double timestep)
{
  timestep *= this->Internals->Scene->mAnimations[0]->mTicksPerSecond;
  timestep *= this->Internals->Scene->mAnimations[0]->mTicksPerSecond;

  // Assimp seems to have a bug with pivots nodes, forcing them to identity fix the issue
  // https://github.com/assimp/assimp/issues/1974
  // for (auto& localMat : this->Internals->NodeLocalMatrix)
  // {
  //   if (localMat.first.find("$AssimpFbx$") != std::string::npos)
  //   {
  //     localMat.second->Identity();
  //   }
  // }

  this->Internals->ClearBones();

  for (int animationId = 0; animationId < this->GetNumberOfAnimations(); animationId++)
  {
    aiAnimation* anim = this->Internals->Scene->mAnimations[animationId];

    Assimp::Interpolator<aiVectorKey> vectorInterpolator;
    Assimp::Interpolator<aiQuatKey> quaternionInterpolator;

    for (unsigned int nodeChannelId = 0; nodeChannelId < anim->mNumChannels; nodeChannelId++)
    {
      aiNodeAnim* nodeAnim = anim->mChannels[nodeChannelId];

      std::cout << "-> " << nodeAnim->mNodeName.data << std::endl;

      aiVector3D translation;
      aiVector3D scaling;
      aiQuaternion quaternion;

      aiVectorKey* positionKey = std::lower_bound(
        nodeAnim->mPositionKeys,
        nodeAnim->mPositionKeys + nodeAnim->mNumPositionKeys,
        timestep,
        [](const aiVectorKey& key, const double &time)
        {
          return key.mTime < time;
        });

      if (positionKey == nodeAnim->mPositionKeys)
      {
        // todo: handle mPreState
        switch (nodeAnim->mPreState)
        {
          case aiAnimBehaviour_DEFAULT:
          break;
        }
        translation = positionKey->mValue;
        std::cout << "Tpre for " << nodeAnim->mNodeName.data << std::endl;
      }
      else if (positionKey == nodeAnim->mPositionKeys + nodeAnim->mNumPositionKeys)
      {
        // todo: handle mPostState
        translation = (positionKey - 1)->mValue;
        std::cout << "Tpost for " << nodeAnim->mNodeName.data << std::endl;
      }
      else
      {
        aiVectorKey* prev = positionKey - 1;
        ai_real d = (timestep - prev->mTime) / (positionKey->mTime - prev->mTime);
        vectorInterpolator(translation, *prev, *positionKey, d);
      }

      aiQuatKey* rotationKey = std::lower_bound(
        nodeAnim->mRotationKeys,
        nodeAnim->mRotationKeys + nodeAnim->mNumRotationKeys,
        timestep,
        [](const aiQuatKey& key, const double &time)
        {
          return key.mTime < time;
        });

      if (rotationKey == nodeAnim->mRotationKeys)
      {
        // todo: handle mPreState
        quaternion = rotationKey->mValue;
        std::cout << "Rpre for " << nodeAnim->mNodeName.data << std::endl;
      }
      else if (rotationKey == nodeAnim->mRotationKeys + nodeAnim->mNumRotationKeys)
      {
        // todo: handle mPostState
        quaternion = (rotationKey - 1)->mValue;
        std::cout << "Rpost for " << nodeAnim->mNodeName.data << std::endl;
      }
      else
      {
        aiQuatKey* prev = rotationKey - 1;
        ai_real d = (timestep - prev->mTime) / (rotationKey->mTime - prev->mTime);
        quaternionInterpolator(quaternion, *prev, *rotationKey, d);
      }

      aiVectorKey* scalingKey = std::lower_bound(
        nodeAnim->mScalingKeys,
        nodeAnim->mScalingKeys + nodeAnim->mNumScalingKeys,
        timestep,
        [](const aiVectorKey& key, const double &time)
        {
          return key.mTime < time;
        });

      if (scalingKey == nodeAnim->mScalingKeys)
      {
        // todo: handle mPreState
        scaling = scalingKey->mValue;
        std::cout << "Spre for " << nodeAnim->mNodeName.data << std::endl;
      }
      else if (scalingKey == nodeAnim->mScalingKeys + nodeAnim->mNumScalingKeys)
      {
        // todo: handle mPostState
        scaling = (scalingKey - 1)->mValue;
        std::cout << "Spost for " << nodeAnim->mNodeName.data << std::endl;
      }
      else
      {
        aiVectorKey* prev = scalingKey - 1;
        ai_real d = (timestep - prev->mTime) / (scalingKey->mTime - prev->mTime);
        vectorInterpolator(scaling, *prev, *scalingKey, d);
      }

      vtkMatrix4x4* transform = this->Internals->NodeLocalMatrix[nodeAnim->mNodeName.data];

      if (transform)
      {
        // Initialize quaternion
        vtkQuaternion<double> rotation;
        rotation.Set(quaternion.w, quaternion.x, quaternion.y, quaternion.z);
        rotation.Normalize();

        double rotationMatrix[3][3];
        rotation.ToMatrix3x3(rotationMatrix);

        // Apply transformations
        for (int i = 0; i < 3; i++)
        {
          for (int j = 0; j < 3; j++)
          {
            transform->SetElement(i, j, scaling[j] * rotationMatrix[i][j]);
          }
          transform->SetElement(i, 3, translation[i]);
        }
      }
    }
  }

  vtkNew<vtkMatrix4x4> identity;
  this->Internals->UpdateNodeTransform(this->Internals->Scene->mRootNode, identity);

  this->Internals->UpdateBones();
}

vtkIdType vtkF3DAssimpImporter::GetNumberOfAnimations()
{
  return this->Internals->Scene ? this->Internals->Scene->mNumAnimations : 0;
}

std::string vtkF3DAssimpImporter::GetAnimationName(vtkIdType animationIndex)
{
  return this->Internals->Scene->mAnimations[animationIndex]->mName.data;
}

void vtkF3DAssimpImporter::EnableAnimation(vtkIdType animationIndex)
{
  this->Internals->EnabledAnimations[animationIndex] = true;
}

void vtkF3DAssimpImporter::DisableAnimation(vtkIdType animationIndex)
{
  this->Internals->EnabledAnimations[animationIndex] = false;
}

bool vtkF3DAssimpImporter::IsAnimationEnabled(vtkIdType animationIndex)
{
  return this->Internals->EnabledAnimations[animationIndex];
}

bool vtkF3DAssimpImporter::GetTemporalInformation(vtkIdType animationIndex, double frameRate, int& nbTimeSteps,
    double timeRange[2], vtkDoubleArray* timeSteps)
{
  double duration = this->Internals->Scene->mAnimations[animationIndex]->mDuration;
  double fps = this->Internals->Scene->mAnimations[animationIndex]->mTicksPerSecond;

  if (fps == 0.0)
  {
    fps = frameRate;
  }

  timeRange[0] = 0.0;
  timeRange[1] = duration / (fps * fps); // why do we need to square it?

  timeSteps->SetNumberOfComponents(1);
  timeSteps->SetNumberOfTuples(0);

  nbTimeSteps = 0;

  for (double time = 0.0; time < timeRange[1]; time += (1.0 / frameRate))
  {
    timeSteps->InsertNextTuple(&time);
    nbTimeSteps++;
  }

  return true;
}

void vtkF3DAssimpImporter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
