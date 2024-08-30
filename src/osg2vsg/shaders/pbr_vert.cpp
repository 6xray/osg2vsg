#include <vsg/io/VSG.h>
static auto fbxshader_vert = []() {std::istringstream str(
R"(#vsga 0.5.4
Root id=1 vsg::ShaderStage
{
  userObjects 0
  stage 1
  entryPointName "main"
  module id=2 vsg::ShaderModule
  {
    userObjects 0
    hints id=0
    source "#version 450
#pragma import_defines ( VSG_NORMAL, VSG_COLOR, VSG_TEXCOORD0, VSG_LIGHTING )
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants 
{
    mat4 projection;
    mat4 modelview;
    //mat3 normal;
} pc;

layout(location = 0) in vec3 osg_Vertex;

#ifdef VSG_NORMAL
layout(location = 1) in vec3 osg_Normal;
layout(location = 1) out vec3 normalDir;
#endif

#ifdef VSG_COLOR
layout(location = 3) in vec4 osg_Color;
layout(location = 3) out vec4 vertexColor;
#endif

#ifdef VSG_TEXCOORD0
layout(location = 4) in vec2 osg_MultiTexCoord0;
layout(location = 4) out vec2 texCoord0;
#endif

#ifdef VSG_LIGHTING
layout(location = 2) out vec3 eyePos;
layout(location = 5) out vec3 viewDir;
layout(location = 6) out vec3 lightDir;
#endif

out gl_PerVertex{ vec4 gl_Position; };

void main()
{
	vec4 vertex = vec4(osg_Vertex, 1.0);
	
	gl_Position = (pc.projection * pc.modelview) * vertex;
	
#ifdef VSG_LIGHTING
	eyePos = (pc.modelview * vertex).xyz;
	viewDir = -(pc.modelview * vertex).xyz;
	
	// TODO : use real light source positions
	vec4 lightPos = /*osg_LightSource.position*/ vec4(0.0, 10.0, 50.0, 0.0);
    viewDir = -vec3((pc.modelview) * vec4(osg_Vertex, 1.0));
    if (lightPos.w == 0.0)
        lightDir = lightPos.xyz;
    else
        lightDir = lightPos.xyz + viewDir;
#endif
	
#ifdef VSG_NORMAL
	vec4 normal = vec4(osg_Normal, 0.0);
	normalDir = (pc.modelview * normal).xyz;
#endif
	
#ifdef VSG_COLOR
	vertexColor = osg_Color;
#endif

#ifdef VSG_TEXCOORD0
	texCoord0 = osg_MultiTexCoord0.st;
#endif
}
"
    code 0
    
  }
  NumSpecializationConstants 0
}
)");
vsg::VSG io;
return io.read_cast<vsg::ShaderStage>(str);
};
