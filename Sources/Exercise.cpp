#include "pch.h"

#include <Kore/System.h>
#include <Kore/IO/FileReader.h>
#include <Kore/Math/Core.h>
#include <Kore/System.h>
#include <Kore/Input/Keyboard.h>
#include <Kore/Input/Mouse.h>
#include <Kore/Graphics1/Image.h>
#include <Kore/Graphics4/Graphics.h>
#include <Kore/Graphics4/PipelineState.h>
#include <Kore/Log.h>

#include "Memory.h"
#include "ObjLoader.h"

using namespace Kore;

class MeshObject {
public:
	MeshObject(const char* meshFile, const char* textureFile, const Graphics4::VertexStructure& structure, float scale = 1.0f) {
		mesh = loadObj(meshFile);
		image = new Graphics4::Texture(textureFile, true);

		vertexBuffer = new Graphics4::VertexBuffer(mesh->numVertices, structure, 0);
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

		indexBuffer = new Graphics4::IndexBuffer(mesh->numIndices);
		int* indices = indexBuffer->lock();
		for (int i = 0; i < mesh->numIndices; i++) {
			indices[i] = mesh->indices[i];
		}
		indexBuffer->unlock();

		M = mat4::Identity();
	}

	void render(Graphics4::TextureUnit tex) {
		//image->_set(tex);
		Graphics4::setTexture(tex, image);
		//vertexBuffer->_set();
		Graphics4::setVertexBuffer(*vertexBuffer);
		//indexBuffer->_set();
		Graphics4::setIndexBuffer(*indexBuffer);
		Graphics4::drawIndexedVertices();
	}

	mat4 M;
private:
	Graphics4::VertexBuffer* vertexBuffer;
	Graphics4::IndexBuffer* indexBuffer;
	Mesh* mesh;
	Graphics4::Texture* image;
};

namespace {
	const int width = 1024;
	const int height = 768;
	double startTime;
	Graphics4::Shader* vertexShader;
	Graphics4::Shader* fragmentShader;
	Graphics4::PipelineState* pipeline;

	// null terminated array of MeshObject pointers
	MeshObject* objects[] = { nullptr, nullptr, nullptr, nullptr, nullptr };

	// The view projection matrix aka the camera
	mat4 P;
	mat4 V;

	// uniform locations - add more as you see fit
	Graphics4::TextureUnit tex;
	Graphics4::ConstantLocation pLocation;
	Graphics4::ConstantLocation vLocation;
	Graphics4::ConstantLocation mLocation;
	Graphics4::ConstantLocation lightLocation;
	Graphics4::ConstantLocation eyeLocation;
	Graphics4::ConstantLocation specLocation;
	Graphics4::ConstantLocation roughnessLocation;
	Graphics4::ConstantLocation modeLocation;

	vec3 eye;
	vec3 globe = vec3(0, 0, 0);
	vec3 light = vec3(0, 1.5f, -3.0f);

	
	bool left, right, up, down, forward, backward;
	int mode = 0;
	float roughness = 0.9f;
	float specular = 0.1f;
	bool toggle = false;
	
	void update() {
		float t = (float)(System::time() - startTime);

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
		
		Graphics4::begin();
		Graphics4::clear(Graphics4::ClearColorFlag | Graphics4::ClearDepthFlag, 0xff000000, 1000.0f);
		
		Graphics4::setPipeline(pipeline);

		// Set your uniforms for the light vector, the roughness and all further constants you encounter in the BRDF terms.
		// The BRDF itself should be implemented in the fragment shader.
		Graphics4::setFloat3(lightLocation, light);
		Graphics4::setFloat3(eyeLocation, eye);
		Graphics4::setFloat(specLocation, specular);
		Graphics4::setFloat(roughnessLocation, roughness);
		Graphics4::setInt(modeLocation, mode);

		// set the camera
		// vec3(0, 2, -3), vec3(0, 2, 0)
		V = mat4::lookAt(eye, vec3(eye.x(), eye.y(), eye.z() + 3), vec3(0, 1, 0));
		//V = mat4::lookAt(eye, globe, vec3(0, 1, 0)); //rotation test, can be deleted
		P = mat4::Perspective(Kore::pi * 2 / 3, (float)width / (float)height, 0.1f, 100);
		Graphics4::setMatrix(vLocation, V);
		Graphics4::setMatrix(pLocation, P);

		// iterate the MeshObjects
		MeshObject** current = &objects[0];
		while (*current != nullptr) {
			// set the model matrix
			Graphics4::setMatrix(mLocation, (*current)->M);

			(*current)->render(tex);
			++current;
		}

		Graphics4::end();
		Graphics4::swapBuffers();
	}

	void keyDown(KeyCode code) {
		if (code == KeyLeft) {
			left = true;
		}
		else if (code == KeyRight) {
			right = true;
		}
		else if (code == KeyUp) {
			forward = true;
		}
		else if (code == KeyDown) {
			backward = true;
		}
		else if (code == KeyW) {
			up = true;
		}
		else if (code == KeyS) {
			down = true;
		}
		else if (code == KeyB) {
			mode = 0;
			log(Info, "Complete BRDF");
		}
		else if (code == KeyF) {
			mode = 1;
			log(Info, "Schlick's Fresnel approximation");
		}
		else if (code == KeyD) {
			mode = 2;
			log(Info, "Trowbridge-Reitz normal distribution term");
		}
		else if (code == KeyG) {
			mode = 3;
			log(Info, "Cook and Torrance's geometry factor");
		}
		else if (code == KeyT) {
			toggle = !toggle;
		}
		else if (code == KeyR) {
			if (toggle) {
				roughness = Kore::max(roughness - 0.1f, 0.0f);
			}
			else {
				roughness = Kore::min(roughness + 0.1f, 1.0f);
			}
			log(Info, "Roughness: %f", roughness);
		}
		else if (code == KeyE) {
			if (toggle) {
				specular = Kore::max(specular - 0.1f, 0.0f);
			}
			else {
				specular = Kore::min(specular + 0.1f, 1.0f);
			}
			log(Info, "Specular: %f", specular);
		}
		else if (code == KeySpace) {
			log(Info, "hi");
		}
	}
	
	void keyUp(KeyCode code) {
		if (code == KeyLeft) {
			left = false;
		}
		else if (code == KeyRight) {
			right = false;
		}
		else if (code == KeyUp) {
			forward = false;
		}
		else if (code == KeyDown) {
			backward = false;
		}
		else if (code == KeyW) {
			up = false;
		}
		else if (code == KeyS) {
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
		Memory::init();

		FileReader vs("shader.vert");
		FileReader fs("shader.frag");
		vertexShader = new Graphics4::Shader(vs.readAll(), vs.size(), Graphics4::VertexShader);
		fragmentShader = new Graphics4::Shader(fs.readAll(), fs.size(), Graphics4::FragmentShader);

		// This defines the structure of your Vertex Buffer
		Graphics4::VertexStructure structure;
		structure.add("pos", Graphics4::Float3VertexData);
		structure.add("tex", Graphics4::Float2VertexData);
		structure.add("nor", Graphics4::Float3VertexData);

		pipeline = new Graphics4::PipelineState;
		pipeline->inputLayout[0] = &structure;
		pipeline->inputLayout[1] = nullptr;
		pipeline->vertexShader = vertexShader;
		pipeline->fragmentShader = fragmentShader;
		pipeline->depthMode = Graphics4::ZCompareLess;
		pipeline->depthWrite = true;
		pipeline->cullMode = Graphics4::CounterClockwise;
		pipeline->compile();

		tex = pipeline->getTextureUnit("tex");
		pLocation = pipeline->getConstantLocation("P");
		vLocation = pipeline->getConstantLocation("V");
		mLocation = pipeline->getConstantLocation("M");
		lightLocation = pipeline->getConstantLocation("light");
		eyeLocation = pipeline->getConstantLocation("eye");
		specLocation = pipeline->getConstantLocation("spec");
		roughnessLocation = pipeline->getConstantLocation("roughness");
		modeLocation = pipeline->getConstantLocation("mode");
		objects[0] = new MeshObject("ball.obj", "ball_tex.png", structure);
		objects[0]->M = mat4::Translation(globe.x(), globe.y(), globe.z()) * mat4::RotationY(180.0f);
		objects[1] = new MeshObject("ball.obj", "light_tex.png", structure, 0.3f);
		objects[1]->M = mat4::Translation(light.x(), light.y(), light.z());

		Graphics4::setTextureAddressing(tex, Graphics4::U, Graphics4::Repeat);
		Graphics4::setTextureAddressing(tex, Graphics4::V, Graphics4::Repeat);
		
		log(Info, "Showing complete BRDF");
		log(Info, "Roughness: %f", roughness);
		log(Info, "Specular: %f", specular);

		eye = vec3(0, 2, -3);
	}
}

int kore(int argc, char** argv) {
	Kore::System::init("Exercise 7", width, height);
	
	init();

	Kore::System::setCallback(update);

	startTime = System::time();
	
	Keyboard::the()->KeyDown = keyDown;
	Keyboard::the()->KeyUp = keyUp;
	Mouse::the()->Move = mouseMove;
	Mouse::the()->Press = mousePress;
	Mouse::the()->Release = mouseRelease;

	Kore::System::start();
	
	return 0;
}
