#include "bitmap.h"

int main(int argc, char* argv[])
{ 
	if (argc < 2) {
		printf("Uso: %s <arquivo.bmp>\n", argv[0]);
		return 1;
	}
	
    BMP bmp(argv[1]);
	return 0;
}