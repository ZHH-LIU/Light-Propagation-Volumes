#version 450 core
layout(binding = 6, r32ui) uniform volatile coherent uimage3D texBlock;
uniform sampler2D gPositionDepth;
uniform sampler2D gNormal;

uniform vec3 minPos;
uniform vec3 maxPos;
uniform int Step;
uniform float A;

const float PI= 3.14159265359;

mat3 Zrotate(float angle);
vec4 RotateSHCoefficients(vec4 unrotatedCoeffs, float theta, float phi);
ivec3 posTrans(vec3 pos);
void imageAtomicRGBA8Avg(ivec3 coords, vec4 val);

vec4 cosineCoeffs= vec4(
					0.886218,
					0.0,
					1.02328,
					0.0);

void main()
{
	vec3 worldPos = texelFetch(gPositionDepth,ivec2(gl_FragCoord.xy),0).xyz;
	vec3 normal = normalize(texelFetch(gNormal,ivec2(gl_FragCoord.xy),0).xyz);

	float voxSize = (maxPos-minPos).x/Step;
	ivec3 screenPos = posTrans(worldPos);

	float theta = acos(normal.z);
	float phi = atan(normal.y,normal.x)+PI;

	float blockRatio = A/(voxSize*voxSize);
	vec4 B = blockRatio * RotateSHCoefficients(cosineCoeffs, theta, phi);
	imageAtomicRGBA8Avg(screenPos,B);
}

ivec3 posTrans(vec3 pos)
{
	vec3 result = pos-(maxPos+minPos)/2;
	result /=(maxPos-minPos);
	result*=Step;
	result+=vec3(Step/2);
	return ivec3(result);
}

void imageAtomicRGBA8Avg(ivec3 coords, vec4 val)
{
	uint newVal = packUnorm4x8(val);
	uint prevStoredVal = 0;
	uint curStoredVal;
	// Loop as long as destination value gets changed by other threads
	while ((curStoredVal = imageAtomicCompSwap(texBlock, coords, prevStoredVal, newVal)) != prevStoredVal)
	{
		prevStoredVal = curStoredVal;
		vec4 rval = unpackUnorm4x8(curStoredVal);
		rval *= 256.0; // Denormalize
		vec4 curValF = rval + val; // Add new value
		curValF.xyz /= 256.0; // Renormalize
		newVal = packUnorm4x8(curValF);
	}
}

vec4 RotateSHCoefficients(vec4 unrotatedCoeffs, float theta, float phi)
{
	vec4 rotatedCoeffs = vec4(0.0);

	//Band 0 coefficient is unchanged
	rotatedCoeffs.x = unrotatedCoeffs.x;

	//Rotate band 1 coefficients
	mat3 Xrotate90 = mat3(0,-1,0,1,0,0,0,0,1);
	rotatedCoeffs.yzw = Zrotate(phi)*transpose(Xrotate90)*Zrotate(theta)*Xrotate90*unrotatedCoeffs.yzw;

	return rotatedCoeffs;
}

mat3 Zrotate(float angle)
{
	return mat3(cos(angle),0,-sin(angle),0,1,0,sin(angle),0,cos(angle));
}