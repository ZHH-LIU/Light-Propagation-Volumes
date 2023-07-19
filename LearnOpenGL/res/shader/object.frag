#version 450 core
uniform sampler3D texR;
uniform sampler3D texG;
uniform sampler3D texB;
uniform sampler3D texOcclusion;

out vec4 fragColor;

in VS_OUT{
	vec3 fragPos;
	vec3 normal;
	vec2 texCoord;
	vec4 fragPosLightSpace;
} fs_in;

uniform vec3 viewPos;
uniform sampler2D gPositionDepth;
uniform vec3 minPos;
uniform vec3 maxPos;
uniform int Step;

struct Material {
	//vec3 ambient;
	sampler2D diffuse;
	sampler2D specular;
	float shininess;
};
uniform Material material;

struct DirLight {
	vec3 direction;

	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
};
uniform DirLight dirlight;

const float PI= 3.14159265359;

vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewPos, vec3 fragPos);
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir);
mat3 Zrotate(float angle);
vec4 RotateSHCoefficients(vec4 unrotatedCoeffs, float theta, float phi);
int Factorial(int n);
float P(int l, int m, float x);
float K(int l, int m);
float SH(int l, int m, float theta, float phi);
vec3 getLight(vec3 direction, vec3 worldPos);

vec3 posTransToNdc(vec3 pos)
{
	vec3 result = pos-(maxPos+minPos)/2;
	result /=(maxPos-minPos);
	result+=vec3(0.5);
	return result;
}

vec4 cosineCoeffs= vec4(
					0.886218,
					0.0,
					1.02328,
					0.0);

void main()
{
	//direct
	vec3 direct  = calcDirLight(dirlight, fs_in.normal, viewPos, fs_in.fragPos);

	//indirect
	vec3 Id = getLight(-fs_in.normal,fs_in.fragPos);

	vec3 Is=vec3(0.0);
	vec3 viewDir = -fs_in.fragPos+viewPos;
	vec3 reflectDir = normalize(reflect(viewDir,fs_in.normal));
	float texelSize = (maxPos-minPos).x/Step;
	for(int i=1;i<=4;i++)
	{
		Is+=getLight(reflectDir,fs_in.fragPos-reflectDir*texelSize*i);
	}
	//Ls/=4.0;

	vec3 I =Is+Id;

	float len = texelSize*0.5;
	vec3 L=I*len*len;

	vec3 ambient = L* vec3(texture(material.diffuse, fs_in.texCoord));

	//GI
	vec3 GI = ambient + direct;

	fragColor=vec4(GI,1.0);

}

vec3 getLight(vec3 direction, vec3 worldPos)
{
	float theta =acos(direction.z);
	float phi = atan(direction.y,direction.x)+PI;
	vec4 ylm = vec4(SH(0,0,theta,phi),SH(1,-1,theta,phi),SH(1,0,theta,phi),SH(1,1,theta,phi));

	vec3 Pos = posTransToNdc(worldPos);
	vec4 cR = textureLod(texR,Pos,0);
	vec4 cG = textureLod(texG,Pos,0);
	vec4 cB = textureLod(texB,Pos,0);

	vec3 L = max(vec3(dot(cR,ylm),dot(cG,ylm),dot(cB,ylm)),0.0);
	return L;
}

vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewPos, vec3 fragPos)
{
	//diffuse
	vec3 lightDir = normalize(-light.direction);
	float diff = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = light.diffuse * diff * vec3(texture(material.diffuse, fs_in.texCoord));
	//specular
	vec3 viewDir = normalize(viewPos - fragPos);
	vec3 halfwayDir = normalize(viewDir + lightDir);
	float spec = pow(max(dot(halfwayDir, normal), 0.0), material.shininess);
	vec3 specular = light.specular * spec * vec3(texture(material.specular, fs_in.texCoord));

	float shadow = ShadowCalculation(fs_in.fragPosLightSpace, normal, lightDir);

	return  (1.0 - shadow) * (diffuse + specular);
}

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
	vec3 projCoord = fragPosLightSpace.xyz / fragPosLightSpace.w;
	projCoord = projCoord * 0.5 + 0.5;
	float currentDepth = projCoord.z;
	float shadow = 0.0f;

	float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);

	//PCF
	vec2 texelSize = 1.0 / textureSize(gPositionDepth, 0);
	for (int i = 0; i != 3; i++)
	{
		for (int j = 0; j != 3; j++)
		{
			float pcfDepth = texture(gPositionDepth, projCoord.xy + vec2(i, j) * texelSize).a;

			shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;;
		}
	}
	
	shadow /= 9.0;

	//Ô¶´¦
	if (projCoord.z > 1.0f)
		shadow = 0.0f;

	return shadow;
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

int Factorial(int n)
{
    if (n <= 1)
        return 1;

    int k = n;
    int fact = n;
    while (--k > 1)
    {
        fact *= k;
    }
    return fact;
}

float P(int l, int m, float x)
{
    //l=m
    float Pmm = 1.0;

    if (m > 0)
    {
        float item = sqrt(1.0 - x * x);
        for (int i = 1; i <= m; i++)
        {
            Pmm *= item * (2.0 * i - 1.0) * (-1.0);
        }
    }

    if (l == m)
        return Pmm;

    //l=m+1
    float Pmp1m = (2.0 * m + 1.0) * x * Pmm;
    if (l == m + 1)
        return Pmp1m;

    //l>m+1
    float Plm = 1.0;
    for (int i = m + 2; i <= l; i++)
    {
        Plm = x * (2.0 * i - 1.0) * Pmp1m - (i + m - 1.0) * Pmm;
        Plm /= (i - m);
        Pmm = Pmp1m;
        Pmp1m = Plm;
    }
    return Plm;
}

float K(int l, int m)
{
    float item = (2.0 * l + 1.0) / (4.0 * PI) * Factorial(l - m) / Factorial(l + m);
    return sqrt(item);
}

float SH(int l, int m, float theta, float phi)
{
    if (m == 0)
        return K(l, m) * P(l, m, cos(theta));

    if (m > 0)
        return K(l, m) * P(l, m, cos(theta)) * sqrt(2.0) * cos(m * phi);

    return K(l, -m) * P(l, -m, cos(theta)) * sqrt(2.0) * sin(-m * phi);
}