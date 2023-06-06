///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
#ifdef TEXTURED_EMPTYOBJ

#if defined(VERTEX) ///////////////////////////////////////////////////

layout(location=0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;

out vec3 FragPos;
out vec2 vTexCoord;
out vec3 Normal;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 model;
void main(){
	vTexCoord=aTexCoord;
	vec4 worldPoss = view * model * vec4(aPosition, 1.0);
	FragPos = worldPoss.xyz;

	mat3 normalMatrixs = transpose(inverse(mat3(view*model)));
    Normal = normalMatrixs * aNormal;
	gl_Position = projection* worldPoss;
}
#elif defined(FRAGMENT) ///////////////////////////////////////////////

in vec2 vTexCoord;

in vec3 FragPos;
in vec3 Normal;
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;
layout (location = 3) out vec4 gDepth;
uniform int lightAffected;
uniform vec3 ColorToPass;
void main(){
	gPosition = FragPos;
	gNormal = normalize(Normal);
	gAlbedoSpec = vec4(1);

	if(lightAffected == 0){
		gNormal= vec3(-1);
		gAlbedoSpec = vec4(ColorToPass,1.0);
		
	}
	
	
}

#endif
#endif


// NOTE: You can write several shaders in the same file if you want as
// long as you embrace them within an #ifdef block (as you can see above).
// The third parameter of the LoadProgram function in engine.cpp allows
// chosing the shader you want to load by name.
