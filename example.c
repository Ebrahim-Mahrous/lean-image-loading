#include "image/image.h"
#include <stdio.h>

int main(int argc, char* argv[]) {
	// example loading image

	int32_t error = 0;
	Image image1 = { 0 };
	Image image2 = { 0 };
	Image image3 = { 0 };
	Image image4 = { 0 };

	error = iLoadImage(&image1, "example1.png");
	if (error < 0) {
		return error;
	}
	error = iLoadImage(&image2, "example2.png");
	if (error < 0) {
		return error;
	}
	error = iLoadImage(&image3, "example3.png");
	if (error < 0) {
		return error;
	}
	error = iLoadImage(&image4, "example4.png");
	if (error < 0) {
		return error;
	}
	iFreeImage(&image1);
	iFreeImage(&image2);
	iFreeImage(&image3);
	iFreeImage(&image4);
	
	return 0;
}