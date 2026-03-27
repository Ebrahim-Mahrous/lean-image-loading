#ifndef _IMAGE_H
#define _IMAGE_H
#include <stdint.h>

typedef struct Image {
	uint64_t width;
	uint64_t height;
	uint64_t bytesPerPixel;
	uint8_t* data; // size of data = width * height * bytesPerPixel (bytes)
} Image;

int32_t iLoadImage(Image* image, const char* fileName);
void iFreeImage(Image* image);

#endif // !_IMAGE_H
