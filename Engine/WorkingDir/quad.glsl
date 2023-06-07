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
uniform sampler2D ggPosition;
uniform sampler2D ggNormal;
uniform int FinalRenderID;

vec4 fin;


// parameters (you'd probably want to use them as uniforms to more easily tweak the effect)
int kernelSize = 64;
float radius = 0.1;
float bias = 0.025;

// tile noise texture over screen based on screen dimensions divided by noise size
const vec2 noiseScale = vec2(800.0/4.0, 600.0/4.0); 

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
    mat4 projectionMat;
    mat4 projectionMatInv;
    int uLightCount;
    Light uLight[20];
};
layout(binding = 1, std140) uniform GlobalParamss
{
    float left;
    float right;
    float bottom;
    float top;
    float znear;
    float zfar;
    vec3 samples[64];

};
vec3 ReconstructPixelPosition(float depth,mat4 projectionMatrixInv,vec2 v)
{
    float xndc =gl_FragCoord.x / v.x * 2.0 - 1.0;
    float yndc =gl_FragCoord.y / v.y * 2.0 - 1.0;
    float zndc =depth * 2.0 - 1.0;
    vec4 posNDC=vec4(xndc,yndc,zndc,1.0);
    vec4 posView=projectionMatrixInv * posNDC;
    return posView.xyz / posView.w;
}

vec4 ambient(vec3 FFragPos,vec3 FNormal)
{
     vec3 tangent = cross(FNormal,vec3(0,1,0));
     vec3 bitangent = cross(FNormal,tangent);
     mat3 TBN= mat3(tangent,bitangent,FNormal);
     float occlusion=0.0;
     for(int i = 0; i < 64; ++i)
     {
        vec3 offsetView=TBN * samples[i];
        vec3 samplePosView = FFragPos + (offsetView * radius);

        vec4 sampleTexCoord= projectionMat*vec4(samplePosView,1.0);
        sampleTexCoord.xyz /=sampleTexCoord.w;
        sampleTexCoord.xyz =sampleTexCoord.xyz * 0.5 + 0.5;
       
        float sampledDepth=  texture(gDepth, sampleTexCoord.xy).r;
        vec2 g=vec2(800,600);
        vec3 sampledPosView=ReconstructPixelPosition(sampledDepth,projectionMatInv,g);
        //vec3 sampledPosView= Reco(sampledDepth,left,right,bottom,top,znear,zfar,g);
        occlusion +=(samplePosView.z<sampledPosView.z-0.02 ? 1.0 :0.0);
     }
     return vec4(vec3(1.0-occlusion/64.0),1.0);


}
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
     vec3 FFragPos = texture(ggPosition, vTexCoord).rgb;
    vec3 FNormal = texture(ggNormal, vTexCoord).rgb;
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
    else if(FinalRenderID == 4)
    {       
        
        fin=ambient(FFragPos,FNormal);
        
    }else if(FinalRenderID == 5)
    {
        if(Normal.x != -1){
            fin = LightRender(FragPos,Normal,Diffuse,Specular);
        }else
        {
            fin =vec4(Diffuse,1.0);
        }

    }else{
        if(Normal.x != -1){
            fin = LightRender(FragPos,Normal,Diffuse,Specular)*ambient(FFragPos,FNormal);
        }else
        {
            fin =vec4(Diffuse,1.0);
        }
    }
    
    oColor = fin;
}

#endif
#endif
