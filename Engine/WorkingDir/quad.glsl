///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
#ifdef TEXTURED_QUAD

#if defined(VERTEX) ///////////////////////////////////////////////////

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 vTexCoord;








void main()
{
    vTexCoord = aTexCoords;
    gl_Position = vec4(aPos, 1.0);
}
#elif defined(FRAGMENT) ///////////////////////////////////////////////

in vec2 vTexCoord;
layout(location=0) out vec4 oColor;
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D gDepth;
uniform int FinalRenderID;

vec4 fin;

struct Light
{
    int type;
    vec3 color;
    vec3 direction;
    vec3 position;
    float intensity;
    float angle;
};
layout(binding = 0, std140) uniform GlobalParams
{
    vec3 uCameraPosition;
    int uLightCount;
    Light uLight[20];
};

vec4 LightRender(vec3 FFragPos,vec3 FNormal,vec3 FDiffuse,float FSpecular)
{
    vec3 lighting  = FDiffuse * 0.1; // hard-coded ambient component
    vec3 viewDir  = normalize(uCameraPosition - FFragPos);
    vec3 halfwayDir;
    for(int i = 0; i < uLightCount; ++i)
    {
        float inten=uLight[i].intensity;
        float attenuation;
         // diffuse
         vec3 lightDir = normalize(uLight[i].position - FFragPos);

         if(uLight[i].type == 0)
         {
            lightDir = -normalize(uLight[i].direction);
            attenuation=uLight[i].intensity;
            viewDir=vec3(0.0);

         }else if(uLight[i].type == 2){
         
            vec3 v2 = normalize(uLight[i].direction);
            float dot_product = dot(lightDir, v2);
            float mag1 = length(lightDir);
            float mag2 = length(v2);
            float ResultAngle = degrees(acos(dot_product / (mag1 * mag2)));
            if(ResultAngle>uLight[i].angle)
            {
                inten=0;
            }
         
         }


         vec3 diffuse = (max(dot(FNormal, lightDir), 0.0) * FDiffuse * uLight[i].color);
         // specular
         halfwayDir = normalize(lightDir + viewDir);  
         float spec = pow(max(dot(FNormal, halfwayDir), 0.0), 16.0);
         vec3 specular = uLight[i].color * spec * FSpecular;
         // attenuation

         
         if(uLight[i].type != 0)
         {
            float distance = length(uLight[i].position - FFragPos);
            attenuation = inten / (1.0 + 0.7 * distance + 1.8 * distance * distance);
         }
         

         diffuse *= attenuation;
         specular *= attenuation;
         lighting += diffuse + specular;        
     }
     return vec4(lighting,1.0);


}

void main(){

    vec3 FragPos = texture(gPosition, vTexCoord).rgb;
    vec3 Normal = texture(gNormal, vTexCoord).rgb;
    vec3 Diffuse = texture(gAlbedoSpec, vTexCoord).rgb;
    float Specular = texture(gAlbedoSpec, vTexCoord).a;
    vec3 depht = texture(gDepth, vTexCoord).rgb;
    if(FinalRenderID == 0){
        fin =vec4(Diffuse,1.0);
    }
    else if(FinalRenderID == 1){
        fin =vec4(Normal,1.0);
    }
    else if(FinalRenderID == 2){
   
        fin =vec4(FragPos,1.0);
    }else if(FinalRenderID == 3){
   
        
        float normalizedDepth =1.0- depht.r;
        

        vec3 grayscaleColor = vec3(normalizedDepth);
        fin =vec4(grayscaleColor,1.0);
    }
    else
    {       
        if(Normal.x != -1){
            fin = LightRender(FragPos,Normal,Diffuse,Specular);
        }else
        {
            fin =vec4(Diffuse,1.0);
        }
        
        
    }
    oColor = fin;
}

#endif
#endif


// NOTE: You can write several shaders in the same file if you want as
// long as you embrace them within an #ifdef block (as you can see above).
// The third parameter of the LoadProgram function in engine.cpp allows
// chosing the shader you want to load by name.
