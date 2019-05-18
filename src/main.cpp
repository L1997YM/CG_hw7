#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include "stb_image.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <time.h>
#include "camera.h"


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
unsigned int loadTexture(const char *path);
void RenderScene(const unsigned int  &shaderID);
void RenderCube();

const char* glsl_version = "#version 330";
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
Camera camera(glm::vec3(0.0f, 1.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

glm::vec3 lightPos(-2.0f, 4.0f, -1.0f);
unsigned int woodTexture;
unsigned int planeVAO;

const char *depthVertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"uniform mat4 model;\n"
"uniform mat4 lightSpaceMatrix;\n"
"void main()\n"
"{\n"
"	gl_Position = lightSpaceMatrix * model * vec4(aPos,1.0);\n"
"}\0";

const char *depthFragmentShaderSource = "#version 330 core\n"
"void main()\n"
"{\n"
"// gl_FragDepth = gl_FragCoord.z;\n"
"}\0";

const char *vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"layout (location = 1) in vec3 aNormal;\n"
"layout (location = 2) in vec2 aTexCoords;\n"
"out vec2 TexCoords;\n"
"out VS_OUT {\n"
"   vec3 FragPos;\n"
"   vec3 Normal;\n"
"   vec2 TexCoords;\n"
"   vec4 FragPosLightSpace;\n"
"} vs_out;"
"uniform mat4 projection;\n"
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 lightSpaceMatrix;\n"
"void main()\n"
"{\n"
"   vs_out.FragPos = vec3(model * vec4(aPos, 1.0));\n"
"   vs_out.Normal = transpose(inverse(mat3(model))) * aNormal;\n"
"   vs_out.TexCoords = aTexCoords;\n"
"   vs_out.FragPosLightSpace = lightSpaceMatrix * vec4(vs_out.FragPos, 1.0);\n"
"   gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
"}\0";

const char *fragmentShaderSource = "#version 330 core\n"
"out vec4 FragColor;\n"
"in VS_OUT{\n"
"	vec3 FragPos;\n"
"	vec3 Normal;\n"
"	vec2 TexCoords;\n"
"	vec4 FragPosLightSpace;\n"
"} fs_in;\n"
"uniform sampler2D diffuseTexture;\n"
"uniform sampler2D shadowMap;\n"
"uniform vec3 lightPos;\n"
"uniform vec3 viewPos;\n"
"float ShadowCalculation(vec4 fragPosLightSpace)\n"
"{\n"
"	//将光空间坐标转换为标准化设备坐标，再变换到[0,1]范围\n"
"	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;\n"
"	projCoords = projCoords * 0.5 + 0.5;\n"
"	//光透视视角下的最近深度\n"
"	float closestDepth = texture(shadowMap, projCoords.xy).r;\n"
"	//当前片元在光透视视角下的深度\n"
"	float currentDepth = projCoords.z;\n"
"	vec3 normal = normalize(fs_in.Normal);\n"
"	vec3 lightDir = normalize(lightPos - fs_in.FragPos);\n"
"	float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);\n"
"	float shadow = 0.0;\n"
"	vec2 texelSize = 1.0 / textureSize(shadowMap, 0);\n"
"	for (int x = -1; x <= 1; ++x)\n"
"	{\n"
"		for (int y = -1; y <= 1; ++y)\n"
"		{\n"
"			float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;\n"
"			shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;\n"
"		}\n"
"	}\n"
"	shadow /= 9.0;\n"
"	if (projCoords.z > 1.0)\n"
"		shadow = 0.0;\n"
"	return shadow;\n"
"}\n"
"void main()\n"
"{\n"
"	vec3 color = texture(diffuseTexture, fs_in.TexCoords).rgb;\n"
"	vec3 normal = normalize(fs_in.Normal);\n"
"	vec3 lightColor = vec3(0.3);\n"
"	vec3 ambient = 0.3 * color;\n"
"	vec3 lightDir = normalize(lightPos - fs_in.FragPos);\n"
"	float diff = max(dot(lightDir, normal), 0.0);\n"
"	vec3 diffuse = diff * lightColor;\n"
"	vec3 viewDir = normalize(viewPos - fs_in.FragPos);\n"
"	vec3 reflectDir = reflect(-lightDir, normal);\n"
"	float spec = 0.0;\n"
"	vec3 halfwayDir = normalize(lightDir + viewDir);\n"
"	spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);\n"
"	vec3 specular = spec * lightColor;\n"
"	float shadow = ShadowCalculation(fs_in.FragPosLightSpace);\n"
"	vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;\n"
"	FragColor = vec4(lighting, 1.0);\n"
"}\0";

int main()
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);			//告诉GLFW我们要使用的OpenGL的版本号
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);		//告诉GLFW我们使用的是核心模式
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);	//参数说明：1、窗口的宽；2、窗口的高；3、窗口名称
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);		//每当窗口调整大小时调用
	//glfwSetCursorPosCallback(window, mouse_callback);
	//glfwSetScrollCallback(window, scroll_callback);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))		//初始化GLAD
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}
	// 开启深度测试
	glEnable(GL_DEPTH_TEST);
	//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	/*---------------------------------------------编译着色器------------------------------------------------*/
	unsigned int depthVertexShader = glCreateShader(GL_VERTEX_SHADER);		//创建着色器，参数为类型，顶点着色器
	glShaderSource(depthVertexShader, 1, &depthVertexShaderSource, NULL);		//添加着色器源码
	glCompileShader(depthVertexShader);		//编译着色器
	int success;
	char infoLog[512];
	glGetShaderiv(depthVertexShader, GL_COMPILE_STATUS, &success);		//检查是否编译成功
	if (!success)
	{
		glGetShaderInfoLog(depthVertexShader, 512, NULL, infoLog);		//获取错误信息
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}

	/*--------------------------------------------片段着色器-------------------------------------------------*/
	unsigned int depthFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(depthFragmentShader, 1, &depthFragmentShaderSource, NULL);
	glCompileShader(depthFragmentShader);
	glGetShaderiv(depthFragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(depthFragmentShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
	}

	/*--------------------------------------------着色器程序-------------------------------------------------*/
	unsigned int depthShaderProgram = glCreateProgram();		//创建一个程序并返回ID引用
	glAttachShader(depthShaderProgram, depthVertexShader);		//将着色器附加到程序对象上
	glAttachShader(depthShaderProgram, depthFragmentShader);
	glLinkProgram(depthShaderProgram);
	glGetProgramiv(depthShaderProgram, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(depthShaderProgram, 512, NULL, infoLog);
	}
	glDeleteShader(depthVertexShader);		//删除着色器对象 
	glDeleteShader(depthFragmentShader);

	/*---------------------------------------------编译着色器------------------------------------------------*/
	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);		//创建着色器，参数为类型，顶点着色器
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);		//添加着色器源码
	glCompileShader(vertexShader);		//编译着色器
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);		//检查是否编译成功
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);		//获取错误信息
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}

	/*--------------------------------------------片段着色器-------------------------------------------------*/
	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
	}

	/*--------------------------------------------着色器程序-------------------------------------------------*/
	unsigned int phongShaderProgram = glCreateProgram();		//创建一个程序并返回ID引用
	glAttachShader(phongShaderProgram, vertexShader);		//将着色器附加到程序对象上
	glAttachShader(phongShaderProgram, fragmentShader);
	glLinkProgram(phongShaderProgram);
	glGetProgramiv(phongShaderProgram, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(phongShaderProgram, 512, NULL, infoLog);
	}
	glDeleteShader(vertexShader);		//删除着色器对象 
	glDeleteShader(fragmentShader);

	//设置ImGui上下文
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//设置ImGui样式
	ImGui::StyleColorsDark();
	//平台/渲染器绑定
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	// 加载纹理
	unsigned int woodTexture = loadTexture(std::string("wood.jpg").c_str());

	//为渲染的深度贴图创建一个帧缓冲对象
	unsigned int depthMapFBO;
	glGenFramebuffers(1, &depthMapFBO);
	
	//创建一个2D纹理，提供给帧缓冲的深度缓冲使用
	const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
	unsigned int depthMap;
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	//处理采样过多问题
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	//把生成的深度纹理作为帧缓冲的深度缓冲
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glUseProgram(phongShaderProgram);
	unsigned int diffuseTextureLoc = glGetUniformLocation(phongShaderProgram, "diffuseTexture");
	glUniform1i(diffuseTextureLoc, 0);
	unsigned int shadowMapLoc = glGetUniformLocation(phongShaderProgram, "shadowMap");
	glUniform1i(shadowMapLoc, 1);
	
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

	bool isPerspective = false;

	while (!glfwWindowShouldClose(window))		//让GLFW退出前一直保持运行
	{
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		processInput(window);

		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 lightProjection;
		glm::mat4 lightSpaceMatrix;
		float near_plane = 1.0f, far_plane = 7.5f;
		if (isPerspective)
		{
			lightProjection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, near_plane, far_plane);
		}
		else
		{
			lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
		}
		glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		//光空间的变化矩阵，将世界空间坐标变换到光源处所见到的空间
		lightSpaceMatrix = lightProjection * lightView;

		//给深度缓冲着色器传lightSpaceMatrix
		glUseProgram(depthShaderProgram);
		unsigned int lightSpaceMatrixLoc = glGetUniformLocation(depthShaderProgram, "lightSpaceMatrix");
		glUniformMatrix4fv(lightSpaceMatrixLoc, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
		
		//渲染深度贴图
		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, woodTexture);
		RenderScene(depthShaderProgram);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		//渲染阴影
		glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(phongShaderProgram);
		glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
		glm::mat4 view = camera.GetViewMatrix();
		unsigned int phongProjectionLoc = glGetUniformLocation(phongShaderProgram, "projection");
		glUniformMatrix4fv(phongProjectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
		unsigned int phongViewLoc = glGetUniformLocation(phongShaderProgram, "view");
		glUniformMatrix4fv(phongViewLoc, 1, GL_FALSE, glm::value_ptr(view));
		unsigned int viewPosLoc = glGetUniformLocation(phongShaderProgram, "viewPos");
		glUniform3fv(viewPosLoc, 1, &camera.Position[0]);
		unsigned int lightPosLoc = glGetUniformLocation(phongShaderProgram, "lightPos");
		glUniform3fv(lightPosLoc, 1, &lightPos[0]);
		lightSpaceMatrixLoc = glGetUniformLocation(phongShaderProgram, "lightSpaceMatrix");
		glUniformMatrix4fv(lightSpaceMatrixLoc, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, woodTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		RenderScene(phongShaderProgram);

		//启动ImGui
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("checkbox");
		ImGui::Checkbox("isPerspective", &isPerspective);
		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);		//交换颜色缓冲
		glfwPollEvents();		//检查有没有触发什么事件
	}
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();		//释放删除之前分配的所有资源
	return 0;
}


void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement(xoffset, yoffset);
}

void processInput(GLFWwindow *window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(RIGHT, deltaTime);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll(yoffset);
}


void RenderScene(const unsigned int  &shaderID)
{
	unsigned int planeVBO;
	unsigned int planeVAO;
	float planeVertices[] = {
		// Positions          // Normals         // Texture Coords
		25.0f, -0.5f, 25.0f, 0.0f, 1.0f, 0.0f, 25.0f, 0.0f,
		-25.0f, -0.5f, -25.0f, 0.0f, 1.0f, 0.0f, 0.0f, 25.0f,
		-25.0f, -0.5f, 25.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,

		25.0f, -0.5f, 25.0f, 0.0f, 1.0f, 0.0f, 25.0f, 0.0f,
		25.0f, -0.5f, -25.0f, 0.0f, 1.0f, 0.0f, 25.0f, 25.0f,
		-25.0f, -0.5f, -25.0f, 0.0f, 1.0f, 0.0f, 0.0f, 25.0f
	};

	/*----------------------------------------------创建VBO--------------------------------------------------*/
	glGenBuffers(1, &planeVBO);		//创建缓冲对象，参数1：需要创建的缓存数量；参数2：储存缓冲ID的地址
	glBindBuffer(GL_ARRAY_BUFFER, planeVBO);		//将缓冲对象绑定到相应的缓冲上，参数1：缓冲类型（顶点数组数据or索引数组数据）
	glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);		//将vertices复制到当前绑定缓冲，GL_STATIC_DRAW表示数据几乎不会改变

	/*---------------------------------------------创建VAO----------------------------------------------------*/
	glGenVertexArrays(1, &planeVAO);
	glBindVertexArray(planeVAO);

	/*-------------------------------------------链接顶点属性-------------------------------------------------*/
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);		//参数1：要配置的顶点属性；参数2：顶点属性的大小
																						//参数3：数据的类型；参数4：步长；参数5：偏移量
	glEnableVertexAttribArray(0);		//启用顶点属性
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glBindVertexArray(0);

	glm::mat4 model = glm::mat4(1.0f);
	unsigned int modelLoc = glGetUniformLocation(shaderID, "model");
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	glBindVertexArray(planeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(1.0f, 1.0f, 0.5f));
	model = glm::scale(model, glm::vec3(0.5));
	modelLoc = glGetUniformLocation(shaderID, "model");
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	RenderCube();
	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(2.0f, -0.25f, 1.0f));
	model = glm::scale(model, glm::vec3(0.5));
	modelLoc = glGetUniformLocation(shaderID, "model");
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	RenderCube();
	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(-1.0f, -0.2f, 2.0f));
	model = glm::rotate(model, 60.0f, glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
	model = glm::scale(model, glm::vec3(0.5));
	modelLoc = glGetUniformLocation(shaderID, "model");
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	RenderCube();
	glDeleteVertexArrays(1, &planeVAO);
	glDeleteBuffers(1, &planeVBO);
}

unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void RenderCube()
{
	if (cubeVAO == 0)
	{
		float vertices[] = {
			// Back face
			-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // Bottom-left
			0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, // top-right
			0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
			0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,  // top-right
			-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,  // bottom-left
			-0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,// top-left
			// Front face
			-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left
			0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,  // bottom-right
			0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  // top-right
			0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, // top-right
			-0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,  // top-left
			-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  // bottom-left
			// Left face
			-0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, // top-right
			-0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // top-left
			-0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,  // bottom-left
			-0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-left
			-0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  // bottom-right
			-0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, // top-right
			// Right face
			0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, // top-left
			0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-right
			0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // top-right         
			0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,  // bottom-right
			0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,  // top-left
			0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // bottom-left     
			// Bottom face
			-0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, // top-right
			0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, // top-left
			0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,// bottom-left
			0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, // bottom-left
			-0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, // bottom-right
			-0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, // top-right
			// Top face
			-0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,// top-left
			0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // bottom-right
			0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, // top-right     
			0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // bottom-right
			-0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,// top-left
			-0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f // bottom-left
		};
		/*----------------------------------------------创建VBO--------------------------------------------------*/
		glGenBuffers(1, &cubeVBO);		//创建缓冲对象，参数1：需要创建的缓存数量；参数2：储存缓冲ID的地址
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);		//将缓冲对象绑定到相应的缓冲上，参数1：缓冲类型（顶点数组数据or索引数组数据）
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);		//将vertices复制到当前绑定缓冲，GL_STATIC_DRAW表示数据几乎不会改变

		/*---------------------------------------------创建VAO----------------------------------------------------*/
		glGenVertexArrays(1, &cubeVAO);
		glBindVertexArray(cubeVAO);

		/*-------------------------------------------链接顶点属性-------------------------------------------------*/
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);		//参数1：要配置的顶点属性；参数2：顶点属性的大小
																							//参数3：数据的类型；参数4：步长；参数5：偏移量
		glEnableVertexAttribArray(0);		//启用顶点属性
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glBindVertexArray(0);
	}
	glBindVertexArray(cubeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

unsigned int loadTexture(char const * path)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
	if (data)
	{
		GLenum format;
		if (nrComponents == 1)
			format = GL_RED;
		else if (nrComponents == 3)
			format = GL_RGB;
		else if (nrComponents == 4)
			format = GL_RGBA;

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT); // for this tutorial: use GL_CLAMP_TO_EDGE to prevent semi-transparent borders. Due to interpolation it takes texels from next repeat 
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
	}
	else
	{
		std::cout << "Texture failed to load at path: " << path << std::endl;
		stbi_image_free(data);
	}
	return textureID;
}


