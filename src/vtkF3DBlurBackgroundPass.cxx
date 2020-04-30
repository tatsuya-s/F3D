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
  os << indent << "FirstPassFBO:";
  if (this->FirstPassFBO != nullptr)
  {
    this->FirstPassFBO->PrintSelf(os, indent);
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
  os << indent << "BlurredPass1:";
  if (this->BlurredPass1 != nullptr)
  {
    this->BlurredPass1->PrintSelf(os, indent);
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
void vtkF3DBlurBackgroundPass::ComputeKernel()
{
  constexpr float euler = 2.71828182845904523536;

  this->Kernel.clear();

  float kernelSum = 0.f;

  while (this->Kernel.size() < 30)
  {
    float x = static_cast<float>(this->Kernel.size());
    float power = -0.5 * x * x / (this->Sigma * this->Sigma);
    float currentValue = std::pow(euler, power) / (this->Sigma * std::sqrt(2.0 * vtkMath::Pi()));
    if (this->Kernel.size() > 0)
    {
      if (currentValue / this->Kernel[0] < 0.001)
      {
        break;
      }
      kernelSum += 2.f * currentValue;
    }
    else
    {
      kernelSum += currentValue;
    }
    this->Kernel.push_back(currentValue);
  }

  // normalize
  std::transform(this->Kernel.begin(), this->Kernel.end(), this->Kernel.begin(),
    [kernelSum](float v) { return v /= kernelSum; });

  cout << "kernel size: " << this->Kernel.size() << endl;
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
              "uniform vec2 direction;\n"
              "//VTK::FSQ::Decl";

    vtkShaderProgram::Substitute(FSSource, "//VTK::FSQ::Decl", ssDecl.str());

    this->ComputeKernel();

    std::stringstream ssImpl;
    ssImpl << "  ivec2 size = textureSize(texColor, 0);\n"
              "  vec4 center = texture(texColor, texCoord);\n"
              "  if (texture(texDepth, texCoord).r < 0.999)\n"
              "  {\n"
              "    gl_FragData[0] = center;\n"
              "  }\n"
              "  else\n"
              "  {\n"
              "    vec2 op, om;\n"
              "    vec4 cp, cm;\n"
              "    float dp, dm;\n"
              "    vec4 col = "
           << this->Kernel[0] << " * center;\n";

    for (int i = 1; i < this->Kernel.size(); i++)
    {
      ssImpl << "    op = texCoord + " << i << " * direction / size;\n"
             << "    om = texCoord - " << i << " * direction / size;\n"
             << "    cp = texture(texColor, op);\n"
             << "    dp = texture(texDepth, op).r;\n"
             << "    cm = texture(texColor, om);\n"
             << "    dm = texture(texDepth, om).r;\n"
             << "    if (dp > 0.999)\n"
             << "    {\n"
             << "      col += " << this->Kernel[i] << " * cp;\n"
             << "    }\n"
             << "    else\n"
             << "    {\n"
             << "      col += " << this->Kernel[i] << " * center;\n"
             << "    }\n"
             << "    if (dm > 0.999)\n"
             << "    {\n"
             << "      col += " << this->Kernel[i] << " * cm;\n"
             << "    }\n"
             << "    else\n"
             << "    {\n"
             << "      col += " << this->Kernel[i] << " * center;\n"
             << "    }\n";
    }

    ssImpl << "    gl_FragData[0] = vec4(col.rgb, center.a);\n"
              "  }\n"
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
    this->ColorTexture->Allocate2D(w, h, 4, VTK_FLOAT);
  }

  if (this->BlurredPass1 == nullptr)
  {
    this->BlurredPass1 = vtkTextureObject::New();
    this->BlurredPass1->SetContext(renWin);
    this->BlurredPass1->SetFormat(GL_RGBA);
    this->BlurredPass1->SetInternalFormat(GL_RGBA32F);
    this->BlurredPass1->SetDataType(GL_FLOAT);
    this->BlurredPass1->SetMinificationFilter(vtkTextureObject::Linear);
    this->BlurredPass1->SetMagnificationFilter(vtkTextureObject::Linear);
    this->BlurredPass1->Allocate2D(w, h, 4, VTK_FLOAT);
  }

  if (this->DepthTexture == nullptr)
  {
    this->DepthTexture = vtkTextureObject::New();
    this->DepthTexture->SetContext(renWin);
    this->DepthTexture->AllocateDepth(w, h, vtkTextureObject::Float32);
  }

  this->ColorTexture->Resize(w, h);
  this->BlurredPass1->Resize(w, h);
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

  if (this->FirstPassFBO == nullptr)
  {
    this->FirstPassFBO = vtkOpenGLFramebufferObject::New();
    this->FirstPassFBO->SetContext(renWin);
    renWin->GetState()->PushFramebufferBindings();
    this->FirstPassFBO->Bind();
    this->FirstPassFBO->AddColorAttachment(0, this->BlurredPass1);
    this->FirstPassFBO->ActivateDrawBuffers(1);
    renWin->GetState()->PopFramebufferBindings();
  }

  ostate->vtkglViewport(x, y, w, h);
  ostate->vtkglScissor(x, y, w, h);

  this->RenderDelegate(s, w, h);

  ostate->vtkglDisable(GL_BLEND);
  ostate->vtkglDisable(GL_DEPTH_TEST);

  this->BuildBlurShader(renWin);

  float dirx[2] = { 1.f, 0.f };
  float diry[2] = { 0.f, 1.f };

  // first blur pass
  renWin->GetState()->PushFramebufferBindings();
  this->FirstPassFBO->Bind();
  this->FirstPassFBO->StartNonOrtho(w, h);

  this->ColorTexture->Activate();
  this->DepthTexture->Activate();
  this->QuadHelper->Program->SetUniformi("texColor", this->ColorTexture->GetTextureUnit());
  this->QuadHelper->Program->SetUniformi("texDepth", this->DepthTexture->GetTextureUnit());
  this->QuadHelper->Program->SetUniform2f("direction", dirx);

  this->QuadHelper->Render();

  this->ColorTexture->Deactivate();

  renWin->GetState()->PopFramebufferBindings();

  // second blur pass
  this->BlurredPass1->Activate();
  this->QuadHelper->Program->SetUniformi("texColor", this->BlurredPass1->GetTextureUnit());
  this->QuadHelper->Program->SetUniform2f("direction", diry);

  this->QuadHelper->Render();

  this->DepthTexture->Deactivate();
  this->BlurredPass1->Deactivate();

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
  if (this->FirstPassFBO)
  {
    this->FirstPassFBO->Delete();
    this->FirstPassFBO = nullptr;
  }
  if (this->ColorTexture)
  {
    this->ColorTexture->Delete();
    this->ColorTexture = nullptr;
  }
  if (this->BlurredPass1)
  {
    this->BlurredPass1->Delete();
    this->BlurredPass1 = nullptr;
  }
  if (this->DepthTexture)
  {
    this->DepthTexture->Delete();
    this->DepthTexture = nullptr;
  }
}
