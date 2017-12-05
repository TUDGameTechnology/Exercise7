#include "pch.h"

#include "GraphicsHelper.h"
#include "Memory.h"

#include <Kore/Graphics1/Graphics.h>
#include <Kore/Graphics4/Graphics.h>
#include <Kore/Math/Core.h>
#include <limits>
#include <string.h>

using namespace Kore;

void shadePixel(int x, int y, float z, float u, float v) {}

void clear(float red, float green, float blue) {
	for (int y = 0; y < Graphics1::height(); ++y) {
		for (int x = 0; x < Graphics1::width(); ++x) {
			Graphics1::setPixel(x, y, red, green, blue);
		}
	}
}

Kore::u8* loadImage(const char* filename, int* imageWidth, int* imageHeight) {
	Graphics1::Image image(filename, true);
	*imageWidth = image.width;
	*imageHeight = image.height;
	Kore::u8* memory = Memory::allocate<Kore::u8>(image.width * image.height * 4);
	memcpy(memory, image.data, image.dataSize);
	return memory;
}

void readPixel(Kore::u8* image, int imageWidth, int imageHeight, int x, int y, float& red, float& green, float& blue) {
	if (x < 0 || x > imageWidth || y < 0 || y > imageHeight) {
		return;
	}
	int col = *(int*)&(image)[imageWidth * 4 * y + x * 4];
	blue = ((col & 0xff0000) >> 16) / 255.0f;
	green = ((col & 0xff00) >> 8) / 255.0f;
	red = (col & 0xff) / 255.0f;
}

void drawImage(Kore::u8* image, int imageWidth, int imageHeight, int x, int y) {
	int ystart = max(0, -y);
	int xstart = max(0, -x);
	int h = min(imageHeight, Graphics1::height() - y);
	int w = min(imageWidth, Graphics1::width() - x);
	for (int yy = ystart; yy < h; ++yy) {
		for (int xx = xstart; xx < w; ++xx) {
			float r, g, b;
			readPixel(image, imageWidth, imageHeight, xx, yy, r, g, b);
			Graphics1::setPixel(x + xx, y + yy, r, g, b);
		}
	}
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
		int xMax = min(span.x2, Graphics1::width());
		
		factor += factorStep * -min(0, span.x1);
		
		for (int x = xMin; x < xMax; ++x) {
			float z = span.z1 + zdiff * factor;
			float u = span.u1 + udiff * factor;
			float v = span.v1 + vdiff * factor;
			shadePixel(x, y, z, u, 1 - v);
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
		int yMax = min(e2.y2, Graphics1::height());
		
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
