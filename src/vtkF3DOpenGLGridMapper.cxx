#include "vtkF3DOpenGLGridMapper.h"

#include <vtkFloatArray.h>
#include <vtkInformation.h>
#include <vtkObjectFactory.h>
#include <vtkOpenGLError.h>
#include <vtkOpenGLRenderPass.h>
#include <vtkOpenGLRenderWindow.h>
#include <vtkOpenGLVertexArrayObject.h>
#include <vtkOpenGLVertexBufferObjectGroup.h>
#include <vtkRenderer.h>
#include <vtkShaderProgram.h>

vtkStandardNewMacro(vtkF3DOpenGLGridMapper);

//----------------------------------------------------------------------------
vtkF3DOpenGLGridMapper::vtkF3DOpenGLGridMapper()
{
  this->SetNumberOfInputPorts(0);
  this->StaticOn();
}

//----------------------------------------------------------------------------
void vtkF3DOpenGLGridMapper::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkF3DOpenGLGridMapper::ReplaceShaderValues(
  std::map<vtkShader::Type, vtkShader*> shaders, vtkRenderer* ren, vtkActor* actor)
{
  this->ReplaceShaderRenderPass(shaders, ren, actor, true);

  std::string VSSource = shaders[vtkShader::Vertex]->GetSource();
  std::string FSSource = shaders[vtkShader::Fragment]->GetSource();

  vtkShaderProgram::Substitute(
    VSSource, "//VTK::PositionVC::Dec", "out vec4 positionMCVSOutput;\n");

  vtkShaderProgram::Substitute(VSSource, "//VTK::PositionVC::Impl",
    "positionMCVSOutput = vec4(vertexMC.x, 0.0, vertexMC.y, 1.0);\n"
    "gl_Position = MCDCMatrix * positionMCVSOutput;\n");

  vtkShaderProgram::Substitute(FSSource, "//VTK::PositionVC::Dec",
    "in vec4 positionMCVSOutput;\n"
    "uniform float fadeDist;\n"
    "uniform float unitSquare;\n"
    "uniform sampler2D reflectionColorTex;\n"
    "uniform sampler2D reflectionDepthTex;\n");

  // fwidth must be computed for all fragments to avoid artifacts with early returns
  vtkShaderProgram::Substitute(FSSource, "  //VTK::UniformFlow::Impl",
    "  vec2 coord = positionMCVSOutput.xz / (unitSquare * positionMCVSOutput.w);\n"
    "  vec2 grid = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);\n");

  vtkShaderProgram::Substitute(FSSource, "  //VTK::Color::Impl",
    "  float line = min(grid.x, grid.y);\n"
    "  float dist2 = unitSquare * unitSquare * (coord.x * coord.x + coord.y * coord.y);\n"
    "  float opacity = (1.0 - min(line, 1.0)) * (1.0 - dist2 / (fadeDist * fadeDist));\n"
    "  vec3 color = diffuseColorUniform;\n"
    "  if (abs(coord.x) < 0.1 && grid.y != line) color = vec3(0.0, 0.0, 1.0);\n"
    "  if (abs(coord.y) < 0.1 && grid.x != line) color = vec3(1.0, 0.0, 0.0);\n"
    "  vec2 texCoord = gl_FragCoord.xy / textureSize(reflectionColorTex, 0);\n"
    "  vec4 background = vec4(0.0);\n"
    "  if (gl_FrontFacing)\n"
    "  {\n"
    "    background = texture(reflectionColorTex, vec2(1.0 - texCoord.x, texCoord.y));\n"
    "    float depth = texture(reflectionDepthTex, vec2(1.0 - texCoord.x, texCoord.y)).r;\n"
    "    float depthDiff = 100.0*(gl_DepthRange.far - gl_DepthRange.near)*abs(gl_FragCoord.z - depth);\n"
    "    //background.rgb = vec3(depthDiff);\n"
    "    background.a *= 0.4;\n"
    "  }\n"
    "  // alpha blending\n"
    "  float outOpacity = opacity + background.a * (1.0 - opacity);\n"
    "  vec3 outColor = (color * opacity + background.rgb * background.a * (1.0 - opacity)) / outOpacity;\n"
    "  gl_FragData[0] = vec4(outColor, outOpacity);\n");

  shaders[vtkShader::Vertex]->SetSource(VSSource);
  shaders[vtkShader::Fragment]->SetSource(FSSource);

  // add camera uniforms declaration
  this->ReplaceShaderPositionVC(shaders, ren, actor);

  // add color uniforms declaration
  this->ReplaceShaderColor(shaders, ren, actor);

  // for depth peeling
  this->ReplaceShaderRenderPass(shaders, ren, actor, false);
}

//----------------------------------------------------------------------------
void vtkF3DOpenGLGridMapper::SetMapperShaderParameters(
  vtkOpenGLHelper& cellBO, vtkRenderer* ren, vtkActor* actor)
{
  if (this->VBOs->GetMTime() > cellBO.AttributeUpdateTime ||
    cellBO.ShaderSourceTime > cellBO.AttributeUpdateTime)
  {
    cellBO.VAO->Bind();
    this->VBOs->AddAllAttributesToVAO(cellBO.Program, cellBO.VAO);

    cellBO.AttributeUpdateTime.Modified();
  }

  // Handle render pass setup:
  vtkInformation* info = actor->GetPropertyKeys();
  if (info && info->Has(vtkOpenGLRenderPass::RenderPasses()))
  {
    int numRenderPasses = info->Length(vtkOpenGLRenderPass::RenderPasses());
    for (int i = 0; i < numRenderPasses; ++i)
    {
      vtkObjectBase* rpBase = info->Get(vtkOpenGLRenderPass::RenderPasses(), i);
      vtkOpenGLRenderPass* rp = vtkOpenGLRenderPass::SafeDownCast(rpBase);
      if (rp && !rp->SetShaderParameters(cellBO.Program, this, actor, cellBO.VAO))
      {
        vtkErrorMacro(
          "RenderPass::SetShaderParameters failed for renderpass: " << rp->GetClassName());
      }
    }
  }

  cellBO.Program->SetUniformf("fadeDist", this->FadeDistance);
  cellBO.Program->SetUniformf("unitSquare", this->UnitSquare);

  this->ReflectionColorTexture->Activate();
  this->ReflectionDepthTexture->Activate();
  cellBO.Program->SetUniformi("reflectionColorTex", this->ReflectionColorTexture->GetTextureUnit());
  cellBO.Program->SetUniformi("reflectionDepthTex", this->ReflectionDepthTexture->GetTextureUnit());

}

//----------------------------------------------------------------------------
void vtkF3DOpenGLGridMapper::BuildBufferObjects(vtkRenderer* ren, vtkActor* act)
{
  vtkNew<vtkFloatArray> infinitePlane;
  infinitePlane->SetNumberOfComponents(2);
  infinitePlane->SetNumberOfTuples(4);

  float d = static_cast<float>(this->FadeDistance);
  float corner1[] = { -d, -d };
  float corner2[] = { -d, d };
  float corner3[] = { d, -d };
  float corner4[] = { d, d };

  infinitePlane->SetTuple(0, corner1);
  infinitePlane->SetTuple(1, corner2);
  infinitePlane->SetTuple(2, corner3);
  infinitePlane->SetTuple(3, corner4);

  vtkOpenGLRenderWindow* renWin = vtkOpenGLRenderWindow::SafeDownCast(ren->GetRenderWindow());
  vtkOpenGLVertexBufferObjectCache* cache = renWin->GetVBOCache();

  this->VBOs->CacheDataArray("vertexMC", infinitePlane, cache, VTK_FLOAT);
  this->VBOs->BuildAllVBOs(cache);

  vtkOpenGLCheckErrorMacro("failed after BuildBufferObjects");

  this->VBOBuildTime.Modified();
}

//-----------------------------------------------------------------------------
double* vtkF3DOpenGLGridMapper::GetBounds()
{
  this->Bounds[0] = this->Bounds[2] = this->Bounds[4] = -this->FadeDistance;
  this->Bounds[1] = this->Bounds[3] = this->Bounds[5] = this->FadeDistance;
  return this->Bounds;
}

//-----------------------------------------------------------------------------
void vtkF3DOpenGLGridMapper::RenderPiece(vtkRenderer* ren, vtkActor* actor)
{
  // Update/build/etc the shader.
  this->UpdateBufferObjects(ren, actor);
  this->UpdateShaders(this->Primitives[PrimitivePoints], ren, actor);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  this->ReflectionColorTexture->Deactivate();
  this->ReflectionDepthTexture->Deactivate();
}

//-----------------------------------------------------------------------------
bool vtkF3DOpenGLGridMapper::GetNeedToRebuildShaders(
  vtkOpenGLHelper& cellBO, vtkRenderer* ren, vtkActor* act)
{
  vtkMTimeType renderPassMTime = this->GetRenderPassStageMTime(act);
  return cellBO.Program == nullptr || cellBO.ShaderSourceTime < renderPassMTime;
}
