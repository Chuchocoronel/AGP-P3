///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
#ifdef TEXTURED_GEOMETRY

#if defined(VERTEX) ///////////////////////////////////////////////////

layout(location=0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;

out vec3 FragPos;
out vec2 vTexCoord;
out vec3 Normal;

out vec3 FFragPos;
out vec3 FNormal;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 model;
void main(){
	vTexCoord=aTexCoord;
	vec4 worldPoss = view * model * vec4(aPosition, 1.0);
	vec4 worldPos =  model * vec4(aPosition, 1.0);
	FFragPos = worldPoss.xyz;
	FragPos = worldPos.xyz;

	mat3 normalMatrixs = transpose(inverse(mat3(view*model)));
	mat3 normalMatrix = transpose(inverse(mat3(model)));
	FNormal=normalMatrixs* aNormal;
    Normal = normalMatrix * aNormal;
	gl_Position = projection* worldPoss;
}
#elif defined(FRAGMENT) ///////////////////////////////////////////////
uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;
in vec2 vTexCoord;

in vec3 FragPos;
in vec3 Normal;
in vec3 FFragPos;
in vec3 FNormal;
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;
layout (location = 3) out vec4 gDepth;
layout (location = 4) out vec3 ggPosition;
layout (location = 5) out vec3 ggNormal;
void main(){
	gPosition = FragPos;
	gNormal = normalize(Normal);
	gAlbedoSpec.rgb = texture(texture_diffuse1, vTexCoord).rgb;
	gAlbedoSpec.a = texture(texture_specular1, vTexCoord).r;
	ggPosition = FFragPos;
	ggNormal = normalize(FNormal);
	
}

#endif
#endif


// NOTE: You can write several shaders in the same file if you want as
// long as you embrace them within an #ifdef block (as you can see above).
// The third parameter of the LoadProgram function in engine.cpp allows
// chosing the shader you want to load by name.