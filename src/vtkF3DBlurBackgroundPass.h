/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkF3DBlurBackgroundPass.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkF3DBlurBackgroundPass
 * @brief   Implement a blur background pass.
 *
 * @sa
 * vtkRenderPass
 */

#ifndef vtkF3DBlurBackgroundPass_h
#define vtkF3DBlurBackgroundPass_h

#include "vtkImageProcessingPass.h"
#include "vtkRenderingOpenGL2Module.h" // For export macro

#include <vector> // For vector

class vtkMatrix4x4;
class vtkOpenGLFramebufferObject;
class vtkOpenGLQuadHelper;
class vtkOpenGLRenderWindow;
class vtkTextureObject;

class VTKRENDERINGOPENGL2_EXPORT vtkF3DBlurBackgroundPass : public vtkImageProcessingPass
{
public:
  static vtkF3DBlurBackgroundPass* New();
  vtkTypeMacro(vtkF3DBlurBackgroundPass, vtkImageProcessingPass);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Perform rendering according to a render state.
   */
  void Render(const vtkRenderState* s) override;

  /**
   * Release graphics resources and ask components to release their own resources.
   */
  void ReleaseGraphicsResources(vtkWindow* w) override;

  //@{
  /**
   * Get/Set the Gaussian sigma parameter.
   * Default is 0.8
   */
  vtkGetMacro(Sigma, double);
  vtkSetMacro(Sigma, double);
  //@}

protected:
  vtkF3DBlurBackgroundPass() = default;
  ~vtkF3DBlurBackgroundPass() override = default;

  void ComputeKernel();
  void BuildBlurShader(vtkOpenGLRenderWindow* renWin);

  void RenderDelegate(const vtkRenderState* s, int w, int h);
  void RenderBlurPass(const vtkRenderState* s, int w, int h);

  vtkTextureObject* ColorTexture = nullptr;
  vtkTextureObject* DepthTexture = nullptr;

  vtkTextureObject* BlurredPass1 = nullptr;

  vtkOpenGLFramebufferObject* DelegateFBO = nullptr;
  vtkOpenGLFramebufferObject* FirstPassFBO = nullptr;

  vtkOpenGLQuadHelper* QuadHelper = nullptr;

  double Sigma = 1.8;
  std::vector<float> Kernel;

private:
  vtkF3DBlurBackgroundPass(const vtkF3DBlurBackgroundPass&) = delete;
  void operator=(const vtkF3DBlurBackgroundPass&) = delete;
};

#endif
