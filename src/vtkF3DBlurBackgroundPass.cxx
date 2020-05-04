/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkF3DBlurBackgroundPass.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkF3DBlurBackgroundPass.h"

#include "vtkMatrix4x4.h"
#include "vtkObjectFactory.h"
#include "vtkOpenGLCamera.h"
#include "vtkOpenGLError.h"
#include "vtkOpenGLFramebufferObject.h"
#include "vtkOpenGLPolyDataMapper.h"
#include "vtkOpenGLQuadHelper.h"
#include "vtkOpenGLRenderUtilities.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkOpenGLShaderCache.h"
#include "vtkOpenGLState.h"
#include "vtkOpenGLVertexArrayObject.h"
#include "vtkRenderState.h"
#include "vtkRenderer.h"
#include "vtkShaderProgram.h"
#include "vtkTextureObject.h"

#include <algorithm>
#include <random>

vtkStandardNewMacro(vtkF3DBlurBackgroundPass);

// ----------------------------------------------------------------------------
void vtkF3DBlurBackgroundPass::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "DelegateFBO:";
  if (this->DelegateFBO != nullptr)
  {
    this->DelegateFBO->PrintSelf(os, indent);
  }
  else
  {
    os << "(none)" << endl;
  }
  os << indent << "TempFBO:";
  if (this->TempFBO != nullptr)
  {
    this->TempFBO->PrintSelf(os, indent);
  }
  else
  {
    os << "(none)" << endl;
  }
  os << indent << "ColorTexture:";
  if (this->ColorTexture != nullptr)
  {
    this->ColorTexture->PrintSelf(os, indent);
  }
  else
  {
    os << "(none)" << endl;
  }
  os << indent << "ColorTexture2:";
  if (this->ColorTexture2 != nullptr)
  {
    this->ColorTexture2->PrintSelf(os, indent);
  }
  else
  {
    os << "(none)" << endl;
  }
  os << indent << "DepthTexture:";
  if (this->DepthTexture != nullptr)
  {
    this->DepthTexture->PrintSelf(os, indent);
  }
  else
  {
    os << "(none)" << endl;
  }
}

// ----------------------------------------------------------------------------
void vtkF3DBlurBackgroundPass::BuildBlurShader(vtkOpenGLRenderWindow* renWin)
{
  if (this->QuadHelper && this->QuadHelper->ShaderChangeValue < this->GetMTime())
  {
    delete this->QuadHelper;
    this->QuadHelper = nullptr;
  }

  if (!this->QuadHelper)
  {
    std::string FSSource = vtkOpenGLRenderUtilities::GetFullScreenQuadFragmentShaderTemplate();

    std::stringstream ssDecl;
    ssDecl << "uniform sampler2D texColor;\n"
              "uniform sampler2D texDepth;\n"
              "uniform float offset;\n"
              "//VTK::FSQ::Decl";

    vtkShaderProgram::Substitute(FSSource, "//VTK::FSQ::Decl", ssDecl.str());

    std::stringstream ssImpl;
    ssImpl << "  ivec2 size = textureSize(texColor, 0);\n"
              "  vec2 step = (vec2(offset + 0.5)) / size;\n"
              "  vec4 col1 = texture(texColor, texCoord + step);\n"
              "  float depth1 = texture(texDepth, texCoord + step).r;\n"
              "  vec4 col2 = texture(texColor, texCoord - step);\n"
              "  float depth2 = texture(texColor, texCoord - step).r;\n"
              "  step.x = -step.x;\n"
              "  vec4 col3 = texture(texColor, texCoord + step);\n"
              "  float depth3 = texture(texColor, texCoord + step).r;\n"
              "  vec4 col4 = texture(texColor, texCoord - step);\n"
              "  float depth4 = texture(texColor, texCoord - step).r;\n"
              "  gl_FragData[0] = 0.25 * (col1 + col2 + col3 + col4);\n"
              "  gl_FragDepth = texture(texDepth, texCoord).r;\n";

    vtkShaderProgram::Substitute(FSSource, "//VTK::FSQ::Impl", ssImpl.str());

    this->QuadHelper = new vtkOpenGLQuadHelper(renWin,
      vtkOpenGLRenderUtilities::GetFullScreenQuadVertexShader().c_str(), FSSource.c_str(), "");

    this->QuadHelper->ShaderChangeValue = this->GetMTime();
  }
  else
  {
    renWin->GetShaderCache()->ReadyShaderProgram(this->QuadHelper->Program);
  }

  if (!this->QuadHelper->Program || !this->QuadHelper->Program->GetCompiled())
  {
    vtkErrorMacro("Couldn't build the SSAO Combine shader program.");
    return;
  }
}

// ----------------------------------------------------------------------------
void vtkF3DBlurBackgroundPass::RenderDelegate(const vtkRenderState* s, int w, int h)
{
  this->PreRender(s);

  this->DelegateFBO->GetContext()->GetState()->PushFramebufferBindings();
  this->DelegateFBO->Bind();
  this->DelegateFBO->StartNonOrtho(w, h);

  this->DelegatePass->Render(s);
  this->NumberOfRenderedProps += this->DelegatePass->GetNumberOfRenderedProps();

  this->DelegateFBO->GetContext()->GetState()->PopFramebufferBindings();

  this->PostRender(s);
}

// ----------------------------------------------------------------------------
void vtkF3DBlurBackgroundPass::Render(const vtkRenderState* s)
{
  vtkOpenGLClearErrorMacro();

  this->NumberOfRenderedProps = 0;

  vtkRenderer* r = s->GetRenderer();
  vtkOpenGLRenderWindow* renWin = static_cast<vtkOpenGLRenderWindow*>(r->GetRenderWindow());
  vtkOpenGLState* ostate = renWin->GetState();

  vtkOpenGLState::ScopedglEnableDisable bsaver(ostate, GL_BLEND);
  vtkOpenGLState::ScopedglEnableDisable dsaver(ostate, GL_DEPTH_TEST);

  if (this->DelegatePass == nullptr)
  {
    vtkWarningMacro("no delegate in vtkF3DBlurBackgroundPass.");
    return;
  }

  // create FBO and texture
  int x, y, w, h;
  r->GetTiledSizeAndOrigin(&w, &h, &x, &y);

  // initialize textures
  if (this->ColorTexture == nullptr)
  {
    this->ColorTexture = vtkTextureObject::New();
    this->ColorTexture->SetContext(renWin);
    this->ColorTexture->SetFormat(GL_RGBA);
    this->ColorTexture->SetInternalFormat(GL_RGBA32F);
    this->ColorTexture->SetDataType(GL_FLOAT);
    this->ColorTexture->SetMinificationFilter(vtkTextureObject::Linear);
    this->ColorTexture->SetMagnificationFilter(vtkTextureObject::Linear);
    this->ColorTexture->SetWrapS(vtkTextureObject::MirroredRepeat);
    this->ColorTexture->SetWrapT(vtkTextureObject::MirroredRepeat);
    this->ColorTexture->Allocate2D(w, h, 4, VTK_FLOAT);
  }

  if (this->ColorTexture2 == nullptr)
  {
    this->ColorTexture2 = vtkTextureObject::New();
    this->ColorTexture2->SetContext(renWin);
    this->ColorTexture2->SetFormat(GL_RGBA);
    this->ColorTexture2->SetInternalFormat(GL_RGBA32F);
    this->ColorTexture2->SetDataType(GL_FLOAT);
    this->ColorTexture2->SetMinificationFilter(vtkTextureObject::Linear);
    this->ColorTexture2->SetMagnificationFilter(vtkTextureObject::Linear);
    this->ColorTexture2->SetWrapS(vtkTextureObject::MirroredRepeat);
    this->ColorTexture2->SetWrapT(vtkTextureObject::MirroredRepeat);
    this->ColorTexture2->Allocate2D(w, h, 4, VTK_FLOAT);
  }

  if (this->DepthTexture == nullptr)
  {
    this->DepthTexture = vtkTextureObject::New();
    this->DepthTexture->SetContext(renWin);
    this->DepthTexture->AllocateDepth(w, h, vtkTextureObject::Float32);
  }

  this->ColorTexture->Resize(w, h);
  this->ColorTexture2->Resize(w, h);
  this->DepthTexture->Resize(w, h);

  if (this->DelegateFBO == nullptr)
  {
    this->DelegateFBO = vtkOpenGLFramebufferObject::New();
    this->DelegateFBO->SetContext(renWin);

    renWin->GetState()->PushFramebufferBindings();
    this->DelegateFBO->Bind();
    this->DelegateFBO->AddColorAttachment(0, this->ColorTexture);
    this->DelegateFBO->ActivateDrawBuffers(1);
    this->DelegateFBO->AddDepthAttachment(this->DepthTexture);
    renWin->GetState()->PopFramebufferBindings();
  }

  if (this->TempFBO == nullptr)
  {
    this->TempFBO = vtkOpenGLFramebufferObject::New();
    this->TempFBO->SetContext(renWin);
    renWin->GetState()->PushFramebufferBindings();
    this->TempFBO->Bind();
    this->TempFBO->AddColorAttachment(0, this->ColorTexture2);
    this->TempFBO->ActivateDrawBuffers(1);
    renWin->GetState()->PopFramebufferBindings();
  }

  ostate->vtkglViewport(x, y, w, h);
  ostate->vtkglScissor(x, y, w, h);

  this->RenderDelegate(s, w, h);

  ostate->vtkglDisable(GL_BLEND);
  ostate->vtkglDisable(GL_DEPTH_TEST);

  this->BuildBlurShader(renWin);

  this->ColorTexture->Activate();
  this->ColorTexture2->Activate();
  this->DepthTexture->Activate();

  // pass 1
  renWin->GetState()->PushFramebufferBindings();
  this->TempFBO->Bind();
  this->TempFBO->StartNonOrtho(w, h);
  this->QuadHelper->Program->SetUniformi("texColor", this->ColorTexture->GetTextureUnit());
  this->QuadHelper->Program->SetUniformi("texDepth", this->DepthTexture->GetTextureUnit());
  this->QuadHelper->Program->SetUniformf("offset", 0);
  this->QuadHelper->Render();
  renWin->GetState()->PopFramebufferBindings();

  // pass 2
  renWin->GetState()->PushFramebufferBindings();
  this->DelegateFBO->Bind();
  this->DelegateFBO->StartNonOrtho(w, h);
  this->QuadHelper->Program->SetUniformi("texColor", this->ColorTexture2->GetTextureUnit());
  this->QuadHelper->Program->SetUniformi("texDepth", this->DepthTexture->GetTextureUnit());
  this->QuadHelper->Program->SetUniformf("offset", 1);
  this->QuadHelper->Render();
  renWin->GetState()->PopFramebufferBindings();

  // pass 3
  renWin->GetState()->PushFramebufferBindings();
  this->TempFBO->Bind();
  this->TempFBO->StartNonOrtho(w, h);
  this->QuadHelper->Program->SetUniformi("texColor", this->ColorTexture->GetTextureUnit());
  this->QuadHelper->Program->SetUniformi("texDepth", this->DepthTexture->GetTextureUnit());
  this->QuadHelper->Program->SetUniformf("offset", 1);
  this->QuadHelper->Render();
  renWin->GetState()->PopFramebufferBindings();

  // pass 4
  renWin->GetState()->PushFramebufferBindings();
  this->DelegateFBO->Bind();
  this->DelegateFBO->StartNonOrtho(w, h);
  this->QuadHelper->Program->SetUniformi("texColor", this->ColorTexture2->GetTextureUnit());
  this->QuadHelper->Program->SetUniformi("texDepth", this->DepthTexture->GetTextureUnit());
  this->QuadHelper->Program->SetUniformf("offset", 2);
  this->QuadHelper->Render();
  renWin->GetState()->PopFramebufferBindings();

  // final pass
  this->QuadHelper->Program->SetUniformi("texColor", this->ColorTexture->GetTextureUnit());
  this->QuadHelper->Program->SetUniformf("offset", 3);
  this->QuadHelper->Render();

  this->DepthTexture->Deactivate();
  this->ColorTexture->Deactivate();
  this->ColorTexture2->Deactivate();

  vtkOpenGLCheckErrorMacro("failed after Render");
}

// ----------------------------------------------------------------------------
void vtkF3DBlurBackgroundPass::ReleaseGraphicsResources(vtkWindow* w)
{
  this->Superclass::ReleaseGraphicsResources(w);

  if (this->QuadHelper)
  {
    delete this->QuadHelper;
    this->QuadHelper = nullptr;
  }
  if (this->DelegateFBO)
  {
    this->DelegateFBO->Delete();
    this->DelegateFBO = nullptr;
  }
  if (this->TempFBO)
  {
    this->TempFBO->Delete();
    this->TempFBO = nullptr;
  }
  if (this->ColorTexture)
  {
    this->ColorTexture->Delete();
    this->ColorTexture = nullptr;
  }
  if (this->ColorTexture2)
  {
    this->ColorTexture2->Delete();
    this->ColorTexture2 = nullptr;
  }
  if (this->DepthTexture)
  {
    this->DepthTexture->Delete();
    this->DepthTexture = nullptr;
  }
}
