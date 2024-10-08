#include <vsg/io/VSG.h>
static auto fbxshader_frag = []() {std::istringstream str(
R"(#vsga 0.5.4
Root id=1 vsg::ShaderStage
{
  userObjects 0
  stage 16
  entryPointName "main"
  module id=2 vsg::ShaderModule
  {
    userObjects 0
    hints id=0
    source "#version 450
#pragma import_defines ( VSG_NORMAL, VSG_COLOR, VSG_TEXCOORD0, VSG_LIGHTING, VSG_DIFFUSE_MAP, VSG_NORMAL_MAP )
#extension GL_ARB_separate_shader_objects : enable
#ifdef VSG_DIFFUSE_MAP
layout(binding = 0) uniform sampler2D diffuseMap;
#endif
#ifdef VSG_NORMAL_MAP
layout(binding = 5) uniform sampler2D normalMap;
#endif

#ifdef VSG_NORMAL
layout(location = 1) in vec3 normalDir;
#endif
#ifdef VSG_COLOR
layout(location = 3) in vec4 vertColor;
#endif
#ifdef VSG_TEXCOORD0
layout(location = 4) in vec2 texCoord0;
#endif
#ifdef VSG_LIGHTING
layout(location = 5) in vec3 viewDir;
layout(location = 6) in vec3 lightDir;
#endif
layout(location = 0) out vec4 outColor;

void main()
{
#ifdef VSG_DIFFUSE_MAP
    vec4 base = texture(diffuseMap, texCoord0.st);
#else
    vec4 base = vec4(1.0,1.0,1.0,1.0);
#endif

#ifdef VSG_COLOR
    base = base * vertColor;
#endif

    float ambientOcclusion = 1.0;
    vec3 specularColor = vec3(0.2,0.2,0.2);

#ifdef VSG_LIGHTING
    #ifdef VSG_NORMAL_MAP
        vec3 nDir = texture(normalMap, texCoord0.st).xyz * 2.0 - 1.0;
        nDir.g = -nDir.g;
    #else
        vec3 nDir = normalDir;
    #endif

    vec3 nd = normalize(nDir);
    vec3 ld = normalize(lightDir);
    vec3 vd = normalize(viewDir);
    vec4 color = vec4(0.01, 0.01, 0.01, 1.0);
    color += /*osg_Material.ambient*/ vec4(0.1, 0.1, 0.1, 0.0);
    float diff = max(dot(ld, nd), 0.0);
    color += /*osg_Material.diffuse*/ vec4(0.8, 0.8, 0.8, 0.0) * diff;
    color *= ambientOcclusion;
    color *= base;

    if (diff > 0.0)
    {
        vec3 halfDir = normalize(ld + vd);
        color.rgb += base.a * specularColor *
            pow(max(dot(halfDir, nd), 0.0), 16.0/*osg_Material.shininess*/);
    }
#else
    vec4 color = base;
#endif

    outColor = color;
    if (outColor.a==0.0) discard;
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
