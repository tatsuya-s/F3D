/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkPlanarReflectionPass.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkPlanarReflectionPass
 * @brief   Implement a planar reflection pass.
 *
 * @sa
 * vtkRenderPass
 */

#ifndef vtkPlanarReflectionPass_h
#define vtkPlanarReflectionPass_h

#include "vtkOpenGLRenderPass.h"
#include "vtkRenderingOpenGL2Module.h" // For export macro

#include "vtkSmartPointer.h"
#include "vtkPlane.h"

#include <vector> // For vector

class vtkMatrix4x4;
class vtkOpenGLFramebufferObject;
class vtkOpenGLQuadHelper;
class vtkTextureObject;
class vtkRenderPass;
class vtkOpenGLRenderWindow;

class VTKRENDERINGOPENGL2_EXPORT vtkPlanarReflectionPass : public vtkOpenGLRenderPass
{
public:
  static vtkPlanarReflectionPass* New();
  vtkTypeMacro(vtkPlanarReflectionPass, vtkOpenGLRenderPass);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Perform rendering according to a render state.
   */
  void Render(const vtkRenderState* s) override;

  /**
   * Release graphics resources and ask components to release their own resources.
   */
  void ReleaseGraphicsResources(vtkWindow* w) override;

  vtkSetSmartPointerMacro(OpaquePass, vtkRenderPass);
  vtkGetSmartPointerMacro(OpaquePass, vtkRenderPass);

  vtkSetSmartPointerMacro(Plane, vtkPlane);
  vtkGetSmartPointerMacro(Plane, vtkPlane);

  vtkSetSmartPointerMacro(ColorTexture, vtkTextureObject);
  vtkGetSmartPointerMacro(ColorTexture, vtkTextureObject);

  vtkSetSmartPointerMacro(DepthTexture, vtkTextureObject);
  vtkGetSmartPointerMacro(DepthTexture, vtkTextureObject);


protected:
  vtkPlanarReflectionPass() = default;
  ~vtkPlanarReflectionPass() override = default;

  void InitializeGraphicsResources(vtkOpenGLRenderWindow* renWin, int w, int h);

  void ComputeMirrorTransform();

  vtkSmartPointer<vtkTextureObject> ColorTexture;
  vtkSmartPointer<vtkTextureObject> DepthTexture;

  vtkOpenGLFramebufferObject* FrameBufferObject = nullptr;

  vtkSmartPointer<vtkRenderPass> OpaquePass;
  vtkSmartPointer<vtkPlane> Plane;

  vtkNew<vtkMatrix4x4> MirrorTransform;

private:
  vtkPlanarReflectionPass(const vtkPlanarReflectionPass&) = delete;
  void operator=(const vtkPlanarReflectionPass&) = delete;
};

#endif
