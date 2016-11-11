#include "pch.h"

#include <Kore/System.h>
#include <Kore/IO/FileReader.h>
#include <Kore/Math/Core.h>
#include <Kore/System.h>
#include <Kore/Input/Keyboard.h>
#include <Kore/Input/Mouse.h>
#include <Kore/Audio/Mixer.h>
#include <Kore/Graphics/Image.h>
#include <Kore/Graphics/Graphics.h>
#include "ObjLoader.h"
#include <algorithm>
#include <iostream>

using namespace Kore;

class MeshObject {
public:
	MeshObject(const char* meshFile, const char* textureFile, const VertexStructure& structure, float scale = 1.0f) {
		mesh = loadObj(meshFile);
		image = new Texture(textureFile, true);

		vertexBuffer = new VertexBuffer(mesh->numVertices, structure, 0);
		float* vertices = vertexBuffer->lock();
		for (int i = 0; i < mesh->numVertices; ++i) {
			vertices[i * 8 + 0] = mesh->vertices[i * 8 + 0] * scale;
			vertices[i * 8 + 1] = mesh->vertices[i * 8 + 1] * scale;
			vertices[i * 8 + 2] = mesh->vertices[i * 8 + 2] * scale;
			vertices[i * 8 + 3] = mesh->vertices[i * 8 + 3];
			vertices[i * 8 + 4] = 1.0f - mesh->vertices[i * 8 + 4];
			vertices[i * 8 + 5] = mesh->vertices[i * 8 + 5];
			vertices[i * 8 + 6] = mesh->vertices[i * 8 + 6];
			vertices[i * 8 + 7] = mesh->vertices[i * 8 + 7];
		}
		vertexBuffer->unlock();

		indexBuffer = new IndexBuffer(mesh->numFaces * 3);
		int* indices = indexBuffer->lock();
		for (int i = 0; i < mesh->numFaces * 3; i++) {
			indices[i] = mesh->indices[i];
		}
		indexBuffer->unlock();

		M = mat4::Identity();
	}

	void render(TextureUnit tex) {
		//image->_set(tex);
		Graphics::setTexture(tex, image);
		//vertexBuffer->_set();
		Graphics::setVertexBuffer(*vertexBuffer);
		//indexBuffer->_set();
		Graphics::setIndexBuffer(*indexBuffer);
		Graphics::drawIndexedVertices();
	}

	mat4 M;
private:
	VertexBuffer* vertexBuffer;
	IndexBuffer* indexBuffer;
	Mesh* mesh;
	Texture* image;
};

namespace {
	const int width = 1024;
	const int height = 768;
	double startTime;
	Shader* vertexShader;
	Shader* fragmentShader;
	Program* program;

	// null terminated array of MeshObject pointers
	MeshObject* objects[] = { nullptr, nullptr, nullptr, nullptr, nullptr };

	// The view projection matrix aka the camera
	mat4 P;
	mat4 V;

	// uniform locations - add more as you see fit
	TextureUnit tex;
	ConstantLocation pLocation;
	ConstantLocation vLocation;
	ConstantLocation mLocation;
	ConstantLocation lightLocation;
	ConstantLocation eyeLocation;
	ConstantLocation specLocation;
	ConstantLocation roughnessLocation;
	ConstantLocation modeLocation;

	vec3 eye;
	vec3 globe = vec3(0, 1.95f, -2.5f);
	//vec3 globe = vec3(0.7f, 1.2f, -0.2f);
	vec3 light = vec3(0, 1.95f, -3.0f);
	
	bool left, right, up, down, forward, backward;
	int mode = 0;
	float roughness = 0.9f;
	float specular = 0.1f;
	bool toggle = false;
	
	void update() {
		float t = (float)(System::time() - startTime);
		Kore::Audio::update();

		const float speed = 0.05f;
		if (left) {
			eye.x() -= speed;
		}
		if (right) {
			eye.x() += speed;
		}
		if (forward) {
			eye.z() += speed;
		}
		if (backward) {
			eye.z() -= speed;
		}
		if (up) {
			eye.y() += speed;
		}
		if (down) {
			eye.y() -= speed;
		}
		
		Graphics::begin();
		Graphics::clear(Graphics::ClearColorFlag | Graphics::ClearDepthFlag, 0xff000000, 1000.0f);
		
		program->set();

		/*
		Set your uniforms for the light vector, the roughness and all further constants you encounter in the BRDF terms.
		The BRDF itself should be implemented in the fragment shader.
		*/
		Graphics::setFloat3(lightLocation, light);
		Graphics::setFloat3(eyeLocation, eye);
		Graphics::setFloat(specLocation, specular);
		Graphics::setFloat(roughnessLocation, roughness);
		Graphics::setInt(modeLocation, mode);

		// set the camera
		// vec3(0, 2, -3), vec3(0, 2, 0)
		V = mat4::lookAt(eye, vec3(eye.x(), eye.y(), eye.z() + 3), vec3(0, 1, 0));
		//V = mat4::lookAt(eye, globe, vec3(0, 1, 0)); //rotation test, can be deleted
		P = mat4::Perspective(Kore::pi * 2 / 3, (float)width / (float)height, 0.1f, 100);
		Graphics::setMatrix(vLocation, V);
		Graphics::setMatrix(pLocation, P);

		// iterate the MeshObjects
		MeshObject** current = &objects[0];
		while (*current != nullptr) {
			// set the model matrix
			Graphics::setMatrix(mLocation, (*current)->M);

			(*current)->render(tex);
			++current;
		}

		Graphics::end();
		Graphics::swapBuffers();
	}

	void keyDown(KeyCode code, wchar_t character) {
		if (code == Key_Left) {
			left = true;
		}
		else if (code == Key_Right) {
			right = true;
		}
		else if (code == Key_Up) {
			forward = true;
		}
		else if (code == Key_Down) {
			backward = true;
		}
		else if (code == Key_W) {
			up = true;
		}
		else if (code == Key_S) {
			down = true;
		}
		else if (code == Key_B) {
			mode = 0;
			std::cout << "Complete BRDF" << std::endl;
		}
		else if (code == Key_F) {
			mode = 1;
			std::cout << "Schlick's Fresnel approximation" << std::endl;
		}
		else if (code == Key_D) {
			mode = 2;
			std::cout << "Trowbridge-Reitz normal distribution term" << std::endl;
		}
		else if (code == Key_G) {
			mode = 3;
			std::cout << "Cook and Torrance's geometry factor" << std::endl;
		}
		else if (code == Key_T) {
			toggle = !toggle;
		}
		else if (code == Key_R) {
			if (toggle) {
				roughness = std::max(roughness - 0.1f, 0.0f);
			}
			else {
				roughness = std::min(roughness + 0.1f, 1.0f);
			}
			std::cout << "Roughness: " << roughness << std::endl;
		}
		else if (code == Key_E) {
			if (toggle) {
				specular = std::max(specular - 0.1f, 0.0f);
			}
			else {
				specular = std::min(specular + 0.1f, 1.0f);
			}
			std::cout << "Specular: " << specular << std::endl;
		}
		else if (code == Key_Space) {
			std::cout << "hi" << std::endl;
		}
	}
	
	void keyUp(KeyCode code, wchar_t character) {
		if (code == Key_Left) {
			left = false;
		}
		else if (code == Key_Right) {
			right = false;
		}
		else if (code == Key_Up) {
			forward = false;
		}
		else if (code == Key_Down) {
			backward = false;
		}
		else if (code == Key_W) {
			up = false;
		}
		else if (code == Key_S) {
			down = false;
		}
	}
	
	void mouseMove(int window, int x, int y, int movementX, int movementY) {

	}
	
	void mousePress(int window, int button, int x, int y) {

	}

	void mouseRelease(int window, int button, int x, int y) {

	}

	void init() {
		FileReader vs("shader.vert");
		FileReader fs("shader.frag");
		vertexShader = new Shader(vs.readAll(), vs.size(), VertexShader);
		fragmentShader = new Shader(fs.readAll(), fs.size(), FragmentShader);

		// This defines the structure of your Vertex Buffer
		VertexStructure structure;
		structure.add("pos", Float3VertexData);
		structure.add("tex", Float2VertexData);
		structure.add("nor", Float3VertexData);

		program = new Program;
		program->setVertexShader(vertexShader);
		program->setFragmentShader(fragmentShader);
		program->link(structure);

		tex = program->getTextureUnit("tex");
		pLocation = program->getConstantLocation("P");
		vLocation = program->getConstantLocation("V");
		mLocation = program->getConstantLocation("M");
		lightLocation = program->getConstantLocation("light");
		eyeLocation = program->getConstantLocation("eye");
		specLocation = program->getConstantLocation("spec");
		roughnessLocation = program->getConstantLocation("roughness");
		modeLocation = program->getConstantLocation("mode");
		objects[0] = new MeshObject("ball.obj", "ball_tex.png", structure);
		objects[0]->M = mat4::Translation(globe.x(), globe.y(), globe.z()) * mat4::RotationY(180.0f);
		objects[1] = new MeshObject("ball.obj", "light_tex.png", structure, 0.3f);
		objects[1]->M = mat4::Translation(light.x(), light.y(), light.z());

		Graphics::setRenderState(DepthTest, true);
		Graphics::setRenderState(DepthTestCompare, ZCompareLess);

		Graphics::setTextureAddressing(tex, Kore::U, Repeat);
		Graphics::setTextureAddressing(tex, Kore::V, Repeat);
		
		std::cout << "Showing complete BRDF" << std::endl;
		std::cout << "Roughness: " << roughness << std::endl;
		std::cout << "Specular: " << specular << std::endl;

		eye = vec3(0, 2, -3);
	}
}

int kore(int argc, char** argv) {
	Kore::System::init("Exercise 7", width, height);
	
	init();

	Kore::System::setCallback(update);

	startTime = System::time();
	Kore::Mixer::init();
	Kore::Audio::init();
	//Kore::Mixer::play(new SoundStream("back.ogg", true));
	
	Keyboard::the()->KeyDown = keyDown;
	Keyboard::the()->KeyUp = keyUp;
	Mouse::the()->Move = mouseMove;
	Mouse::the()->Press = mousePress;
	Mouse::the()->Release = mouseRelease;

	Kore::System::start();
	
	return 0;
}
