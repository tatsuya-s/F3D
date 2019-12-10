/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkPlanarReflectionPass.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkPlanarReflectionPass.h"

#include "vtkMatrix4x4.h"
#include "vtkObjectFactory.h"
#include "vtkOpenGLCamera.h"
#include "vtkOpenGLError.h"
#include "vtkOpenGLFramebufferObject.h"
#include "vtkOpenGLPolyDataMapper.h"
#include "vtkQuaternion.h"
#include "vtkOpenGLRenderUtilities.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkOpenGLShaderCache.h"
#include "vtkOpenGLState.h"
#include "vtkOpenGLVertexArrayObject.h"
#include "vtkRenderState.h"
#include "vtkRenderer.h"
#include "vtkShaderProgram.h"
#include "vtkTextureObject.h"

vtkStandardNewMacro(vtkPlanarReflectionPass);

// ----------------------------------------------------------------------------
void vtkPlanarReflectionPass::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

// ----------------------------------------------------------------------------
void vtkPlanarReflectionPass::InitializeGraphicsResources(vtkOpenGLRenderWindow* renWin, int w, int h)
{
  if (this->ColorTexture->GetContext() != renWin)
  {
    this->ColorTexture->SetContext(renWin);
    this->ColorTexture->SetFormat(GL_RGBA);
    this->ColorTexture->SetInternalFormat(GL_RGBA32F);
    this->ColorTexture->SetDataType(GL_FLOAT);
    this->ColorTexture->SetMinificationFilter(vtkTextureObject::Linear);
    this->ColorTexture->SetMagnificationFilter(vtkTextureObject::Linear);
    this->ColorTexture->Allocate2D(w, h, 4, VTK_FLOAT);
  }

  if (this->DepthTexture->GetContext() != renWin)
  {
    this->DepthTexture->SetContext(renWin);
    this->DepthTexture->AllocateDepth(w, h, vtkTextureObject::Float32);
  }

  if (this->FrameBufferObject == nullptr)
  {
    this->FrameBufferObject = vtkOpenGLFramebufferObject::New();
    this->FrameBufferObject->SetContext(renWin);
  }
}

// ----------------------------------------------------------------------------
void vtkPlanarReflectionPass::Render(const vtkRenderState* s)
{
  vtkOpenGLClearErrorMacro();

  this->NumberOfRenderedProps = 0;

  vtkRenderer* r = s->GetRenderer();
  vtkOpenGLRenderWindow* renWin = static_cast<vtkOpenGLRenderWindow*>(r->GetRenderWindow());
  vtkOpenGLState* ostate = renWin->GetState();

  if (this->OpaquePass == nullptr)
  {
    vtkWarningMacro("no delegate in vtkPlanarReflectionPass.");
    return;
  }

  // create FBO and texture
  int x, y, w, h;
  r->GetTiledSizeAndOrigin(&w, &h, &x, &y);

  // @todo do not compute every time
  this->ComputeMirrorTransform();

  vtkSmartPointer<vtkCamera> oldCamera = r->GetActiveCamera();
  vtkNew<vtkCamera> newCamera;
  newCamera->DeepCopy(oldCamera);
  r->SetActiveCamera(newCamera);

  newCamera->SetPosition(oldCamera->GetPosition());
  newCamera->SetFocalPoint(oldCamera->GetFocalPoint());
  newCamera->SetViewUp(oldCamera->GetViewUp());
  newCamera->OrthogonalizeViewUp();

  double focalPoint[4];
  newCamera->GetFocalPoint(focalPoint);
  focalPoint[3] = 1.0;
  this->MirrorTransform->MultiplyPoint(focalPoint, focalPoint);
  newCamera->SetFocalPoint(focalPoint);

  double position[4];
  newCamera->GetPosition(position);
  position[3] = 1.0;
  this->MirrorTransform->MultiplyPoint(position, position);
  newCamera->SetPosition(position);

  double up[4];
  newCamera->GetViewUp(up);
  up[3] = 0.0;
  this->MirrorTransform->MultiplyPoint(up, up);
  newCamera->SetViewUp(up);

  this->InitializeGraphicsResources(renWin, w, h);

  this->ColorTexture->Resize(w, h);
  this->DepthTexture->Resize(w, h);

  ostate->vtkglViewport(x, y, w, h);
  ostate->vtkglScissor(x, y, w, h);

  this->PreRender(s);

  this->FrameBufferObject->GetContext()->GetState()->PushFramebufferBindings();
  this->FrameBufferObject->Bind();

  this->FrameBufferObject->AddColorAttachment(0, this->ColorTexture);
  this->FrameBufferObject->ActivateDrawBuffers(1);
  this->FrameBufferObject->AddDepthAttachment(this->DepthTexture);
  this->FrameBufferObject->StartNonOrtho(w, h);

  this->OpaquePass->Render(s);
  this->NumberOfRenderedProps += this->OpaquePass->GetNumberOfRenderedProps();

  this->FrameBufferObject->GetContext()->GetState()->PopFramebufferBindings();

  this->PostRender(s);

  r->SetActiveCamera(oldCamera);

  vtkOpenGLCheckErrorMacro("failed after Render");
}

// ----------------------------------------------------------------------------
void vtkPlanarReflectionPass::ComputeMirrorTransform()
{
  // see Real-Time Rendering Third Edition - 9.3.1
  double origin[3] = {};
  double projPos[3];
  this->Plane->ProjectPoint(origin, projPos);

  vtkNew<vtkMatrix4x4> T;
  for (int i = 0; i < 3; i++)
  {
    T->SetElement(i, 3, -projPos[i]);
  }

  // get rotation from the normal to camera up
  vtkNew<vtkMatrix4x4> R;

  double planeNormal[3];
  this->Plane->GetNormal(planeNormal);

  double k = planeNormal[1];
  if (std::abs(k) < 0.999)
  {
    double cross[3] = { -planeNormal[2], 0.0, planeNormal[0] };
    vtkQuaternion<double> quat(k + 1.0, cross[0], cross[1], cross[2]);
    quat.Normalize();

    double mat[3][3];
    quat.ToMatrix3x3(mat);

    for (int i = 0; i < 3; i++)
    {
      for (int j = 0; j < 3; j++)
      {
        R->SetElement(i, j, mat[i][j]);
      }
    }
  }

  vtkNew<vtkMatrix4x4> F;
  vtkMatrix4x4::Multiply4x4(R, T, F);

  vtkNew<vtkMatrix4x4> S;
  S->SetElement(1, 1, -1.0);

  vtkMatrix4x4::Multiply4x4(S, F, this->MirrorTransform);

  F->Invert();
  vtkMatrix4x4::Multiply4x4(F, this->MirrorTransform, this->MirrorTransform);
}

// ----------------------------------------------------------------------------
void vtkPlanarReflectionPass::ReleaseGraphicsResources(vtkWindow* w)
{
  this->Superclass::ReleaseGraphicsResources(w);

  if (this->FrameBufferObject)
  {
    this->FrameBufferObject->Delete();
    this->FrameBufferObject = nullptr;
  }
}
