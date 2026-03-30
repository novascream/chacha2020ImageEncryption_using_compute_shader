#ifndef COMPUTE_SHADER_H
#define COMPUTE_SHADER_H

#include<glad/glad.h>
#include<glm/glm.hpp>

#include<string>
#include<fstream>
#include<sstream>
#include<iostream>

class ComputeShader
{
public:
	unsigned int ID;
	ComputeShader(const char* ComputeShaderPath)
	{
		std::string compute_code;
		std::ifstream compute_file;
		int flag;
		char infoLog[1024];


		compute_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		try
		{
			compute_file.open(ComputeShaderPath);
			std::stringstream cShaderStream;
			cShaderStream << compute_file.rdbuf();
			compute_code = cShaderStream.str();
		}
		catch (std::ifstream::failure& e)
		{
			std::cout << "ERROR::FILE NOT READ" << e.what() << std::endl;
		}
		const char* cShaderCode = compute_code.c_str();
		unsigned int compute;
		compute = glCreateShader(GL_COMPUTE_SHADER);
		glShaderSource(compute, 1, &cShaderCode, NULL);
		glCompileShader(compute);
		glGetShaderiv(compute, GL_COMPILE_STATUS, &flag);
		if (!flag)
		{
			glGetShaderInfoLog(compute, 1024, NULL, infoLog);
			std::cout << "ERROR::COMPUTE SHADER";
			std::cout << infoLog << std::endl;
		}
		ID = glCreateProgram();
		glAttachShader(ID, compute);
		glLinkProgram(ID);
		glGetProgramiv(ID, GL_LINK_STATUS, &flag);
		glDeleteShader(compute);

		glDeleteShader(compute);

	}
	void use()
	{
		glUseProgram(ID);
	}
	void setBool(const std::string& name, bool value) const
	{
		glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
	}
	void setInt(const std::string& name, int value) const
	{
		glUniform1i(glGetUniformLocation(ID, name.c_str()), value);	
	}
	void setFloat(const std::string& name, int value) const
	{
		glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
	}
	void setMatrix4f(const std::string& name, glm::mat4 mat) const
	{
		glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()),1,GL_FALSE,&mat[0][0]);
	}


};

#endif