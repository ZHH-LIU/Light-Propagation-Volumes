#ifndef LPV_H
#define LPV_H

#include "RSM.h"

extern const unsigned int SCR_WIDTH;
extern const unsigned int SCR_HEIGHT;

class LPV
{
public:
	LPV(unsigned int step, DirRSM* rsm, glm::vec3 _min, glm::vec3 _max)
		: Step(step), ourRSM(rsm)
	{
		Min = _min;
		Max = _max;
		GetFrameVAO();
		GetImage3D();
	}
	void DrawRSM(const std::vector<Object>& objects)
	{
		ourRSM->DrawRSM(objects);
	}
	void LightInsert();
	void BlockInsert();
	void DrawVoxel(unsigned int FBO, Object& object, int mip, int m, const glm::mat4& view, const glm::mat4& projection);
	void DrawBlockVoxel(unsigned int FBO, Object& object, int mip, const glm::mat4& view, const glm::mat4& projection);
	void Propagation(int numIter);
	void ClearLightVolume();
	void DrawObjects(vector<Object>objects, unsigned int FBO, glm::vec3 viewPos, glm::mat4 view, glm::mat4 projection);
private:
	void GetFrameVAO();
	void GetImage3D();
	glm::vec3 getVoxelPosition(unsigned int n, int step, int mip);
	unsigned int frameVAO, frameVBO;
	unsigned int lightVolume[3], lightVolumeProp[3], lightVolumeProp1[3], BlockVolme;
	unsigned int* lightVolumeNow;
	unsigned int Step;
	glm::vec3 Min, Max;
	DirRSM* ourRSM;
	Shader lightInsertShader=Shader("res/shader/lightInsert.vert", "res/shader/lightInsert.frag");
	Shader BlockInsertShader = Shader("res/shader/blockInsert.vert", "res/shader/blockInsert.frag");
	Shader drawShader = Shader("res/shader/cube.vert", "res/shader/cube.frag");
	Shader propShader = Shader("res/shader/prop.comp");
	Shader clearShader = Shader("res/shader/clear.comp");
	Shader objectShader = Shader("res/shader/object.vert", "res/shader/object.frag");
};

void LPV::GetFrameVAO()
{
	float frameVertices[] =
	{
		-1.0, -1.0,
		 1.0, -1.0,
		 1.0,  1.0,
		 1.0,  1.0,
		-1.0,  1.0,
		-1.0, -1.0,
	};

	glGenBuffers(1, &frameVBO);
	glGenVertexArrays(1, &frameVAO);

	glBindVertexArray(frameVAO);
	glBindBuffer(GL_ARRAY_BUFFER, frameVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(frameVertices), frameVertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
}

void LPV::LightInsert()
{
	ClearLightVolume();

	glViewport(0, 0, ourRSM->WIDTH(), ourRSM->HEIGHT());
	lightInsertShader.use();

	lightInsertShader.setInt("Step", Step);
	lightInsertShader.setVec3("minPos", Min);
	lightInsertShader.setVec3("maxPos", Max);

	for (int i = 0; i != 3; i++)
	{
		glBindImageTexture(i, lightVolume[i], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
	}

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, ourRSM->RSM_PositionDepth);
	lightInsertShader.setInt("gPositionDepth", 1);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, ourRSM->RSM_Normal);
	lightInsertShader.setInt("gNormal", 2);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, ourRSM->RSM_Flux);
	lightInsertShader.setInt("gFlux", 3);

	glBindVertexArray(frameVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	for (int i = 0; i != 3; i++)
	{
		glBindTexture(GL_TEXTURE_3D, lightVolume[i]);
		glGenerateMipmap(GL_TEXTURE_3D);
	}
	lightVolumeNow = &lightVolume[0];
}

void LPV::BlockInsert()
{
	glViewport(0, 0, ourRSM->WIDTH(), ourRSM->HEIGHT());
	BlockInsertShader.use();

	glm::vec3 delta = (Max - Min) / (2.0f * Step);
	BlockInsertShader.setInt("Step", Step+1);
	BlockInsertShader.setVec3("minPos", Min-delta);
	BlockInsertShader.setVec3("maxPos", Max+delta);

	float A = ourRSM->Area()/(ourRSM->HEIGHT() * ourRSM->WIDTH());
	BlockInsertShader.setFloat("A", A);

	glBindImageTexture(6, BlockVolme, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ourRSM->RSM_PositionDepth);
	BlockInsertShader.setInt("gPositionDepth", 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, ourRSM->RSM_Normal);
	BlockInsertShader.setInt("gNormal", 1);

	glBindVertexArray(frameVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	glBindTexture(GL_TEXTURE_3D, BlockVolme);
	glGenerateMipmap(GL_TEXTURE_3D);

}

void LPV::Propagation(int numIter)
{
	propShader.use();

	propShader.setInt("Step", Step);
	propShader.setVec3("minPos", Min);
	propShader.setVec3("maxPos", Max);

	bool first = true;
	for (int k = 0; k != numIter; k++)
	{
		for (int i = 0; i != 3; i++)
		{
			if (first)
			{
				glBindImageTexture(i, lightVolume[i], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
				glBindImageTexture(3 + i, lightVolumeProp[i], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);

				lightVolumeNow = &lightVolumeProp[0];
			}
			else
			{
				if (k % 2 == 0)
				{
					glBindImageTexture(i, lightVolumeProp1[i], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
					glBindImageTexture(3 + i, lightVolumeProp[i], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);

					lightVolumeNow = &lightVolumeProp[0];
				}
				else
				{
					glBindImageTexture(i, lightVolumeProp[i], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
					glBindImageTexture(3 + i, lightVolumeProp1[i], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);

					lightVolumeNow = &lightVolumeProp1[0];
				}
			}
		}
		glBindImageTexture(6, BlockVolme, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);

		propShader.setBool("first", first);
		glDispatchCompute(Step / 8, Step / 8, Step / 8);
		//glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		for (int i = 0; i != 3; i++)
		{
			glBindTexture(GL_TEXTURE_3D, lightVolumeProp[i]);
			glGenerateMipmap(GL_TEXTURE_3D);
			glBindTexture(GL_TEXTURE_3D, lightVolumeProp1[i]);
			glGenerateMipmap(GL_TEXTURE_3D);
		}
		first = false;
	}
	glFinish();
}

void LPV::ClearLightVolume()
{
	clearShader.use();

	for (int i = 0; i != 3; i++)
	{
		glBindImageTexture(i, lightVolumeProp1[i], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
		glBindImageTexture(3 + i, lightVolumeProp[i], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
	}
	glBindImageTexture(6, BlockVolme, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);

	glDispatchCompute(Step / 8, Step / 8, Step / 8);
	glFinish();
}

void LPV::GetImage3D()
{
	glGenTextures(3, lightVolume);
	for (int i = 0; i != 3; i++)
	{
		glBindTexture(GL_TEXTURE_3D, lightVolume[i]);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
		GLfloat borderColor[] = { 0.0,0.0,0.0,0.0 };
		glTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, borderColor);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, Step, Step, Step, 0, GL_RGBA, GL_FLOAT, nullptr);
		glBindImageTexture(i, lightVolume[i], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
	}

	glGenTextures(3, lightVolumeProp);
	for (int i = 0; i != 3; i++)
	{
		glBindTexture(GL_TEXTURE_3D, lightVolumeProp[i]);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		GLfloat borderColor[] = { 0.0,0.0,0.0,0.0 };
		glTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, borderColor);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, Step, Step, Step, 0, GL_RGBA, GL_FLOAT, nullptr);
		glBindImageTexture(i+3, lightVolumeProp[i], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
	}

	glGenTextures(3, lightVolumeProp1);
	for (int i = 0; i != 3; i++)
	{
		glBindTexture(GL_TEXTURE_3D, lightVolumeProp1[i]);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		GLfloat borderColor[] = { 0.0,0.0,0.0,0.0 };
		glTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, borderColor);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, Step, Step, Step, 0, GL_RGBA, GL_FLOAT, nullptr);
		glBindImageTexture(i + 3, lightVolumeProp1[i], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
	}

	glGenTextures(1, &BlockVolme);
	glBindTexture(GL_TEXTURE_3D, BlockVolme);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	GLfloat borderColor[] = { 0.0,0.0,0.0,0.0 };
	glTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, (Step+1), (Step + 1), (Step + 1), 0, GL_RGBA, GL_FLOAT, nullptr);
	glBindImageTexture(6, BlockVolme, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
	
	glBindTexture(GL_TEXTURE_3D, 0);
}

void LPV::DrawVoxel(unsigned int FBO, Object& object, int mip, int m, const glm::mat4& view, const glm::mat4& projection)
{
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glEnable(GL_DEPTH_TEST);

	glDisable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glFrontFace(GL_CCW);

	glDisable(GL_STENCIL_TEST);

	glDisable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

	int step = Step / glm::pow(2, mip);
	int length = step * step * step;
	float* Datar = new float[length * 4];
	float* Datag = new float[length * 4];
	float* Datab = new float[length * 4];

	glGetTextureImage(lightVolumeNow[0], mip, GL_RGBA, GL_FLOAT, length * 4 * sizeof(float), Datar);
	glGetTextureImage(lightVolumeNow[1], mip, GL_RGBA, GL_FLOAT, length * 4 * sizeof(float), Datag);
	glGetTextureImage(lightVolumeNow[2], mip, GL_RGBA, GL_FLOAT, length * 4 * sizeof(float), Datab);
	//std::cout << lightVolumeNow[0] << " " << lightVolume[0] << " " << lightVolumeProp[0] << std::endl;
	std::vector<glm::vec3> Voxel_Positions;
	std::vector<glm::vec3> Voxel_Colors;

	if (Datar != nullptr && Datag != nullptr && Datab != nullptr)
	{
		for (unsigned int i = 0; i < length * 4; i += 4)
		{
			if (Datar[i+m] == 0 && Datag[i+m]==0 && Datab[i+m]==0) continue;
			glm::vec3 c = glm::vec3(Datar[i+m], Datag[i+m], Datab[i+m]);
			glm::vec3 p = getVoxelPosition(i / 4, Step, mip);
			Voxel_Colors.push_back(c);
			Voxel_Positions.push_back(p);
		}
	}
	else std::cout << "¶ÁÈ¡Ê§°Ü" << std::endl;
	delete[] Datar;
	delete[] Datag;
	delete[] Datab;
	drawShader.use();

	drawShader.setMat4("view", glm::value_ptr(view));
	drawShader.setMat4("projection", glm::value_ptr(projection));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, lightVolume[0]);
	drawShader.setInt("tex", 0);
	//drawShader.setInt("tex", 0);
	drawShader.setInt("Step", Step);
	drawShader.setVec3("minPos", Min);
	drawShader.setVec3("maxPos", Max);
	drawShader.setInt("mip", mip);

	glm::vec3 range = Max - Min;
	float _step = range.x / Step;
	//std::cout << Voxel_Positions.size()<<std::endl;
	for (int i = 0; i != Voxel_Colors.size(); i++)
	{
		object.SetModel(Voxel_Positions[i] - 0.5f * glm::vec3(_step * 0.5 * glm::pow(2, mip)), _step * 0.5 * glm::pow(2, mip));
		drawShader.setMat4("model", glm::value_ptr(object.model));

		drawShader.setVec3("color", Voxel_Colors[i]);

		glBindVertexArray(object.VAO);
		glDrawElements(GL_TRIANGLES, object.Count, GL_UNSIGNED_INT, 0);
	}
}

void LPV::DrawBlockVoxel(unsigned int FBO, Object& object, int mip, const glm::mat4& view, const glm::mat4& projection)
{
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glEnable(GL_DEPTH_TEST);

	glDisable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glFrontFace(GL_CCW);

	glDisable(GL_STENCIL_TEST);

	glDisable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

	int step = (Step+1) / glm::pow(2, mip);
	int length = step * step * step;
	float* Data = new float[length * 4];
	glGetTextureImage(BlockVolme, mip, GL_RGBA, GL_FLOAT, length * 4 * sizeof(float), Data);
	std::vector<glm::vec3> Voxel_Positions;
	std::vector<glm::vec3> Voxel_Colors;
	if (Data != nullptr)
	{
		for (unsigned int i = 0; i < length * 4; i += 4)
		{
			if (Data[i] == 0) continue;
			glm::vec3 c = glm::vec3(Data[i], Data[i + 1], Data[i + 2]);
			glm::vec3 p = getVoxelPosition(i / 4, Step+1, mip);
			Voxel_Colors.push_back(c);
			Voxel_Positions.push_back(p);
		}
	}
	else std::cout << "¶ÁÈ¡Ê§°Ü" << std::endl;
	delete[] Data;
	drawShader.use();

	drawShader.setMat4("view", glm::value_ptr(view));
	drawShader.setMat4("projection", glm::value_ptr(projection));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, BlockVolme);
	drawShader.setInt("tex", 0);
	//drawShader.setInt("tex", 0);
	drawShader.setInt("Step", Step+1);

	glm::vec3 delta = (Max - Min) / (2.0f * Step);
	drawShader.setVec3("minPos", Min-delta);
	drawShader.setVec3("maxPos", Max+delta);
	drawShader.setInt("mip", mip);

	glm::vec3 range = Max - Min ;
	float _step = range.x / Step;
	//std::cout << Voxel_Positions.size()<<std::endl;
	for (int i = 0; i != Voxel_Colors.size(); i++)
	{
		object.SetModel(Voxel_Positions[i] - 0.5f * glm::vec3(_step * 0.5 * glm::pow(2, mip)), _step * 0.5 * glm::pow(2, mip));
		drawShader.setMat4("model", glm::value_ptr(object.model));

		drawShader.setVec3("color", Voxel_Colors[i]);

		 
		glBindVertexArray(object.VAO);
		glDrawElements(GL_TRIANGLES, object.Count, GL_UNSIGNED_INT, 0);
	}
}

inline glm::vec3 LPV::getVoxelPosition(unsigned int n, int step, int mip)
{
	step = step / glm::pow(2, mip);
	int step2 = step * step;
	int pz = (n - (n % step2)) / step2;
	n %= step2;
	int py = (n - (n % step)) / step;
	n %= step;
	int px = n;

	glm::vec3 pos = glm::vec3(px, py, pz);
	pos -= glm::vec3(step / 2);
	pos /= step;
	pos *= Max - Min;
	pos += (Max + Min) / 2.0f;
	return pos;
}

void LPV::DrawObjects(vector<Object>objects, unsigned int FBO, glm::vec3 viewPos, glm::mat4 view, glm::mat4 projection)
{
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
	glEnable(GL_DEPTH_TEST);

	glDisable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glFrontFace(GL_CCW);

	glDisable(GL_STENCIL_TEST);

	glDisable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);

	//glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	objectShader.use();

	objectShader.setVec3("dirlight.ambient", ourRSM->light.Ambient);
	objectShader.setVec3("dirlight.diffuse", ourRSM->light.Diffuse);
	objectShader.setVec3("dirlight.specular", ourRSM->light.Specular);
	objectShader.setVec3("dirlight.direction", ourRSM->light.Direction);

	objectShader.setFloat("material.shininess", 32.0f);
	objectShader.setVec3("viewPos", viewPos);

	objectShader.setMat4("lightSpaceMatrix", glm::value_ptr(ourRSM->lightSpaceMatrix));
	objectShader.setMat4("view", glm::value_ptr(view));
	objectShader.setMat4("projection", glm::value_ptr(projection));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ourRSM->RSM_PositionDepth);
	objectShader.setInt("gPositionDepth", 0);

	for (int i = 0; i != 3; i++)
	{
		glBindTexture(GL_TEXTURE_3D, lightVolumeNow[i]);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		glActiveTexture(GL_TEXTURE1+i);
		glBindTexture(GL_TEXTURE_3D, lightVolumeNow[i]);
		string name;
		if (i == 0)
			name = "texR";
		else if (i == 1)
			name = "texG";
		else
			name = "texB";
		objectShader.setInt(name.c_str(), i);
	}

	objectShader.setInt("Step", Step);
	objectShader.setVec3("minPos", Min);
	objectShader.setVec3("maxPos", Max);

	for (unsigned int i = 0; i != objects.size(); i++)
	{
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, objects[i].texture_diffuse);
		objectShader.setInt("material.diffuse", 3);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, objects[i].texture_specular);
		objectShader.setInt("material.specular", 4);

		objectShader.setMat4("model", glm::value_ptr(objects[i].model));

		glBindVertexArray(objects[i].VAO);
		glDrawElements(GL_TRIANGLES, objects[i].Count, GL_UNSIGNED_INT, 0);
	}
}
#endif
