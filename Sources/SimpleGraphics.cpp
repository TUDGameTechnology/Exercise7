#include "pch.h"
#include "SimpleGraphics.h"
#include <Kore/IO/FileReader.h>
#include <Kore/Graphics/Graphics.h>
#include <Kore/Graphics/Shader.h>
#include <Kore/IO/FileReader.h>
#include <limits>

using namespace Kore;

//void shadePixel(int x, int y, float z, float u, float v);

namespace {
	Shader* vertexShader;
	Shader* fragmentShader;
	Program* program;
	TextureUnit tex;
	VertexBuffer* vb;
	IndexBuffer* ib;
	Texture* texture;
	int* image;
}

void startFrame() {
	image = (int*)texture->lock();
}

#define CONVERT_COLORS(red, green, blue) int r = (int)((red) * 255); int g = (int)((green) * 255); int b = (int)((blue) * 255);

void clear(float red, float green, float blue) {
	CONVERT_COLORS(red, green, blue);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
#ifdef OPENGL
			image[y * texture->width + x] = 0xff << 24 | b << 16 | g << 8 | r;
#else
			image[y * texture->width + x] = 0xff << 24 | r << 16 | g << 8 | b;
#endif
		}
	}
}

void setPixel(int x, int y, float red, float green, float blue) {
	if (x < 0 || x >= width || y < 0 || y >= height) return;
	CONVERT_COLORS(red, green, blue);
#ifdef OPENGL
	image[y * texture->width + x] = 0xff << 24 | b << 16 | g << 8 | r;
#else
	image[y * texture->width + x] = 0xff << 24 | r << 16 | g << 8 | b;
#endif
}

Image* loadImage(const char* filename) {
	return new Texture(filename, true);
}

void destroyImage(Kore::Image* image) {
	delete image;
}

void drawImage(Image* image, int x, int y) {
	int ystart = max(0, -y);
	int xstart = max(0, -x);
	int h = min(image->height, height - y);
	int w = min(image->width, width - x);
	for (int yy = ystart; yy < h; ++yy) {
		for (int xx = xstart; xx < w; ++xx) {
			int col = image->at(xx, yy);
#ifdef OPENGL
			::image[(y + yy) * texture->width + (x + xx)] = col;
#else
			::image[(y + yy) * texture->width + (x + xx)] = 0xff << 24
				| ((col >> 0) & 0xff) << 16
				| ((col >> 8) & 0xff) << 8
				| ((col >> 16) & 0xff);
#endif
		}
	}
}

void getPixel(Image* image, int x, int y, float& red, float& green, float& blue) {
	int col = image->at(x, y);
	blue = ((col & 0xff0000) >> 16) / 255.0f;
	green = ((col & 0xff00) >> 8) / 255.0f;
	red = (col & 0xff) / 255.0f;
}

namespace {
	struct Edge {
		int x1, y1, x2, y2;
		float z1, z2;
		float u1, v1, u2, v2;

		Edge(int x1, int y1, float z1, float u1, float v1, int x2, int y2, float z2, float u2, float v2) {
			if (y1 < y2) {
				this->x1 = x1;
				this->y1 = y1;
				this->z1 = z1;
				this->u1 = u1;
				this->v1 = v1;
				this->x2 = x2;
				this->y2 = y2;
				this->z2 = z2;
				this->u2 = u2;
				this->v2 = v2;
			}
			else {
				this->x1 = x2;
				this->y1 = y2;
				this->z1 = z2;
				this->u1 = u2;
				this->v1 = v2;
				this->x2 = x1;
				this->y2 = y1;
				this->z2 = z1;
				this->u2 = u1;
				this->v2 = v1;
			}
		}
	};

	struct Span {
		int x1, x2;
		float z1, z2;
		float u1, u2;
		float v1, v2;

		Span(int x1, int x2, float z1, float z2, float u1, float u2, float v1, float v2) {
			if (x1 < x2) {
				this->x1 = x1;
				this->x2 = x2;
				this->z1 = z1;
				this->z2 = z2;
				this->u1 = u1;
				this->v1 = v1;
				this->u2 = u2;
				this->v2 = v2;
			}
			else {
				this->x1 = x2;
				this->x2 = x1;
				this->z1 = z2;
				this->z2 = z1;
				this->u1 = u2;
				this->v1 = v2;
				this->u2 = u1;
				this->v2 = v1;
			}
		}
	};

	void drawSpan(const Span& span, int y) {
		int xdiff = span.x2 - span.x1;
		if (xdiff == 0) return;

		float zdiff = span.z2 - span.z1;
		float udiff = span.u2 - span.u1;
		float vdiff = span.v2 - span.v1;

		float factor = 0.0f;
		float factorStep = 1.0f / xdiff;

		int xMin = max(0, span.x1);
		int xMax = min(span.x2, width);

		factor += factorStep * -min(0, span.x1);

		for (int x = xMin; x < xMax; ++x) {
			float z = span.z1 + zdiff * factor;
			float u = span.u1 + udiff * factor;
			float v = span.v1 + vdiff * factor;
			//shadePixel(x, y, z, u, 1 - v);
			factor += factorStep;
		}
	}

	void drawSpansBetweenEdges(const Edge& e1, const Edge& e2) {
		float e1ydiff = (float)(e1.y2 - e1.y1);
		if (e1ydiff == 0.0f) return;

		float e2ydiff = (float)(e2.y2 - e2.y1);
		if (e2ydiff == 0.0f) return;

		float e1xdiff = (float)(e1.x2 - e1.x1);
		float e2xdiff = (float)(e2.x2 - e2.x1);
		float z1diff = e1.z2 - e1.z1;
		float z2diff = e2.z2 - e2.z1;
		float e1udiff = e1.u2 - e1.u1;
		float e1vdiff = e1.v2 - e1.v1;
		float e2udiff = e2.u2 - e2.u1;
		float e2vdiff = e2.v2 - e2.v1;

		float factor1 = (float)(e2.y1 - e1.y1) / e1ydiff;
		float factorStep1 = 1.0f / e1ydiff;
		float factor2 = 0.0f;
		float factorStep2 = 1.0f / e2ydiff;

		int yMin = max(0, e2.y1);
		int yMax = min(e2.y2, height);

		factor1 += factorStep1 * -min(0, e2.y1);
		factor2 += factorStep2 * -min(0, e2.y1);

		for (int y = yMin; y < yMax; ++y) {
			Span span(e1.x1 + (int)(e1xdiff * factor1), e2.x1 + (int)(e2xdiff * factor2),
				e1.z1 + z1diff * factor1, e2.z1 + z2diff * factor2,
				e1.u1 + e1udiff * factor1, e2.u1 + e2udiff * factor2,
				e1.v1 + e1vdiff * factor1, e2.v1 + e2vdiff * factor2);
			drawSpan(span, y);
			factor1 += factorStep1;
			factor2 += factorStep2;
		}
	}
}

void drawTriangle(float x1, float y1, float z1, float u1, float v1, float x2, float y2, float z2, float u2, float v2, float x3, float y3, float z3, float u3, float v3) {
	Edge edges[3] = {
		Edge((int)Kore::round(x1), (int)Kore::round(y1), z1, u1, v1, (int)Kore::round(x2), (int)Kore::round(y2), z2, u2, v2),
		Edge((int)Kore::round(x2), (int)Kore::round(y2), z2, u2, v2, (int)Kore::round(x3), (int)Kore::round(y3), z3, u3, v3),
		Edge((int)Kore::round(x3), (int)Kore::round(y3), z3, u3, v3, (int)Kore::round(x1), (int)Kore::round(y1), z1, u1, v1)
	};

	int maxLength = 0;
	int longEdge = 0;

	for (int i = 0; i < 3; ++i) {
		int length = edges[i].y2 - edges[i].y1;
		if (length > maxLength) {
			maxLength = length;
			longEdge = i;
		}
	}

	int shortEdge1 = (longEdge + 1) % 3;
	int shortEdge2 = (longEdge + 2) % 3;

	drawSpansBetweenEdges(edges[longEdge], edges[shortEdge1]);
	drawSpansBetweenEdges(edges[longEdge], edges[shortEdge2]);
}

void endFrame() {
	texture->unlock();

	Graphics::begin();
	Graphics::clear(Graphics::ClearColorFlag, 0xff000000);

	program->set();
	texture->_set(tex);
	vb->_set();
	ib->_set();
	Graphics::drawIndexedVertices();

	Graphics::end();
	Graphics::swapBuffers();
}

void initGraphics() {
	FileReader vs("shader.vert");
	FileReader fs("shader.frag");
	vertexShader = new Shader(vs.readAll(), vs.size(), VertexShader);
	fragmentShader = new Shader(fs.readAll(), fs.size(), FragmentShader);
	VertexStructure structure;
	structure.add("pos", Float3VertexData);
	structure.add("tex", Float2VertexData);
	program = new Program;
	program->setVertexShader(vertexShader);
	program->setFragmentShader(fragmentShader);
	program->link(structure);

	tex = program->getTextureUnit("tex");

	texture = new Texture(width, height, Image::RGBA32, false);
	image = (int*)texture->lock();
	for (int y = 0; y < texture->height; ++y) {
		for (int x = 0; x < texture->width; ++x) {
			image[y * texture->width + x] = 0;
		}
	}
	texture->unlock();

	vb = new VertexBuffer(4, structure, 0);
	float* v = vb->lock();
	{
		int i = 0;
		float w = (float)texture->width / (float)texture->texWidth;
		float h = (float)texture->height / (float)texture->texHeight;
		v[i++] = -1; v[i++] = 1; v[i++] = 0.5; v[i++] = 0; v[i++] = 0;
		v[i++] = 1;  v[i++] = 1; v[i++] = 0.5; v[i++] = w; v[i++] = 0;
		v[i++] = 1; v[i++] = -1;  v[i++] = 0.5; v[i++] = w; v[i++] = h;
		v[i++] = -1; v[i++] = -1;  v[i++] = 0.5; v[i++] = 0; v[i++] = h;
	}
	vb->unlock();

	ib = new IndexBuffer(6);
	int* ii = ib->lock();
	{
		int i = 0;
		ii[i++] = 0; ii[i++] = 1; ii[i++] = 3;
		ii[i++] = 1; ii[i++] = 2; ii[i++] = 3;
	}
	ib->unlock();
}
