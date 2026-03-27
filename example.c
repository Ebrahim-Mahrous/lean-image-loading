#include "image/image.h"
#include <stdio.h>

int main(int argc, char* argv[]) {
	// example loading image

	int32_t error = 0;
	Image image = { 0 };

	error = iLoadImage(&image, "example1.png");
	if (error < 0) {
		return error;
	}
	iFreeImage(&image);
	
	return 0;
}