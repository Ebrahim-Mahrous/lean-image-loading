#define _CRT_SECURE_NO_WARNINGS
#include "image_debug.h"
#include "image.h"
#include "png.h"
#include <stdio.h>
#include <stdlib.h>

static int32_t iLoadImagePNG(Image* image, const uint8_t* data, uint64_t size) {
    // This is a static API, it doesn't need debug level NULL checking.
    int32_t error = 0;
    uint64_t imageSizeBytes = 0;
    PNG png = { 0 };
    CHECK(InitPNG(&png, data, size), error);
  
    image->width = png.ihdr.imageWidth;
    image->height = png.ihdr.imageHeight;
    image->bytesPerPixel = BytesPerColorTypePNG(png.ihdr.colorType);
    imageSizeBytes = image->width * image->height * image->bytesPerPixel;
    if (!imageSizeBytes) {
        FreePNG(&png);
        return -10;
    }
    image->data = (uint8_t*)calloc(1, imageSizeBytes);
    if (!image->data) {
        return -2;
    }
    if ((error = LoadPNG(&png, image->data, imageSizeBytes)) < 0) {
        free(image->data);
        FreePNG(&png);
        return error;
    }
    FreePNG(&png);
    return 1;
}

int32_t iLoadImage(Image* image, const char* fileName)
{
    DEBUG(image);
    DEBUG(fileName);

    uint64_t size = 0;
    uint8_t* data = NULL;
    FILE* file = NULL;

    file = fopen(fileName, "rb");
    if (!file) {
        return -2;
    }
    
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (!size) {
        return -3;
    }

    data = (uint8_t*)malloc(size);
    if (!data) {
        return -2;
    }

    if (fread(data, 1, size, file) < size) {
        free(data);
        fclose(file);
        return -2;
    }

    int32_t error = 0;
    if (IsPNG(data, size) > 0) {
        if ((error = iLoadImagePNG(image, data, size)) < 0) {
            free(data);
            fclose(file);
            return error;
        }
    }
    //else if (0) {} 

    free(data);
    fclose(file);
    return 1;
}
#ifdef _WIN32
#pragma optimize( "", off )
void iFreeImage(Image* image)
{
    if (!image) return; // NOP
    if (!image->data) return;
    image->width = 0;
    image->height = 0;
    image->bytesPerPixel = 0;
    free(image->data);
}
#pragma optimize( "", on )
#else
void iFreeImage(Image* image)
{
    if (!image) return; // NOP
    if (!image->data) return;
    image->width = 0;
    image->height = 0;
    image->bytesPerPixel = 0;
    free(image->data);
}
#endif