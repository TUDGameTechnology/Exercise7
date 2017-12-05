#pragma once

// Clears the screen to a single color
void clear(float red, float green, float blue);

// Loads and returns an image using Memory::allocate.
// Also sets the width and height parameters to the image dimensions.
Kore::u8* loadImage(const char* filename, int* width, int* height);

// Draws 32bit RGBA pixels (for examples the ones loaded via loadImage)
void drawImage(Kore::u8* image, int imageWidth, int imageHeight, int x, int y);

// Draws a beautiful, red triangle
void drawTriangle(float x1, float y1, float z1, float u1, float v1, float x2, float y2, float z2, float u2, float v2, float x3, float y3, float z3, float u3, float v3);

// Reads a pixel at position (x,y) and returns RGB values
void readPixel(Kore::u8* image, int imageWidth, int imageHeight, int x, int y, float& red, float& green, float& blue);
