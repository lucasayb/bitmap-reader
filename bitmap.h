#include <iostream>
#include <fstream>
#include <cstdint>
#include <cstdio>

#pragma pack(push, 1)
struct BMPFileHeader {
    uint16_t file_type; // File type always BM which is 0x4D42
    uint32_t file_size; // Size of the file (in bytes)
	uint16_t reserved1; // Reserved, always 0
	uint16_t reserved2; // Reserved, always 0
	uint32_t offset_data; // Start position of pixel data (bytes from the beginning of the file)
};

// DIB Header
struct BMPInfoHeader {
	uint32_t size; // Size of this header (in bytes)
	int32_t width; // width of bitmap in pixels
    int32_t height{ 0 }; // height of bitmap in pixels
	//       (if positive, bottom-up, with origin in lower left corner)
    //       (if negative, top-down, with origin in upper left corner)
	uint16_t planes; // No. of planes for the target device, this is always 1
	uint16_t bit_count; // No. of bits per pixel
	uint32_t compression; // 0 or 3 - uncompressed. THIS PROGRAM CONSIDERS ONLY UNCOMPRESSED BMP images
	uint32_t size_image; // 0 for uncompressed images
	int32_t x_pixels_per_meter;
	int32_t y_pixels_per_meter;
	uint32_t colors_used; // No. color indexes in the color table. Use 0 for the max number of colors allowed by bit_count
	uint32_t colors_important; // No. of colors used for displaying the bitmap. If 0 all colors are required
};

struct BMPColorPalette {
	uint8_t blue;
	uint8_t green;
	uint8_t red;
	uint8_t reserved;
};

struct BMPColorHeader {
    uint32_t red_mask{ 0x00ff0000 };         // Bit mask for the red channel
    uint32_t green_mask{ 0x0000ff00 };       // Bit mask for the green channel
    uint32_t blue_mask{ 0x000000ff };        // Bit mask for the blue channel
    uint32_t alpha_mask{ 0xff000000 };       // Bit mask for the alpha channel
    uint32_t color_space_type{ 0x73524742 }; // Default "sRGB" (0x73524742)
    uint32_t unused[16]{ 0 };                // Unused data for sRGB color space
};

#pragma pack(pop)

struct BMP {
    BMPFileHeader file_header;
	BMPInfoHeader info_header;
	BMPColorHeader color_header;
	std::vector<uint8_t> data;
	std::vector<BMPColorPalette> palette;

    BMP(const char *fname) {
        read(fname);
    }

    void read(const char *fname) {
        std::ifstream inp{fname, std::ios_base::binary};
        if (inp) {
			printf("sizeof(BMPFileHeader) = %zu\n", sizeof(BMPFileHeader));
			printf("sizeof(BMPInfoHeader) = %zu\n", sizeof(BMPInfoHeader));

            inp.read((char*)&file_header, sizeof(file_header));
			
			if (file_header.file_type != 0x4D42) {
				throw std::runtime_error("Error! Unrecognized file format.");
			}
			inp.read((char*)&info_header, sizeof(info_header));
			
			// NOTE: 32bpp support is limited to BMPs that include V4/V5 masks (tutorial-style).
			// Some valid 32bpp BI_RGB BMPs (DIB size = 40) will be rejected for now.
			if (info_header.bit_count == 32) {
				if (info_header.size >= (sizeof(BMPInfoHeader) + sizeof(BMPColorHeader))) {
					// Check if the pixel data is stored as BGRA and if the color space type is sRGB
					inp.read((char*)&color_header, sizeof(color_header));
					check_color_header(color_header);
				} else {
					std::cerr << "Warning! The file \"" << fname << "\" does not seem to contain bit mask information\n";
					throw std::runtime_error("Error! Unrecognized file format.");
				}
			}

			if (info_header.bit_count == 8) {
				read8bitColorPalette(inp);
			}
			
			// Jump to the pixel data location
			inp.seekg(file_header.offset_data, inp.beg);
			
			if (info_header.height < 0) {
				throw std::runtime_error("The program can treat only BMP images with the origin in the bottom left corner!");
			}
			
			data.resize(info_header.width * info_header.height * info_header.bit_count / 8);
			
			if (info_header.bit_count == 32) {
				read32bit(inp);
			}

			if (info_header.bit_count == 8) {
				read8bitPixels(inp);
			}
			
			fileInfoTable();
        } else {
        	throw std::runtime_error("Unable to open the input image file.");
        }
    }

private:

	void read8bitColorPalette(std::ifstream& inp) {
		uint32_t palette_bytes = file_header.offset_data - info_header.size - sizeof(BMPFileHeader);
		if (palette_bytes % sizeof(BMPColorPalette) != 0) {
			throw std::runtime_error("Invalid palette size");
		}
		size_t palette_entries = palette_bytes / sizeof(BMPColorPalette);
		printf("%u", palette_bytes);
		inp.seekg(sizeof(BMPFileHeader) + info_header.size, std::ios::beg);

		palette.resize(palette_entries);

		inp.read((char*)(palette.data()), palette_bytes);
	}
	
	void read8bitPixels(std::ifstream& inp) {
		const int width = info_header.width;
		const int height = std::abs(info_header.height);

		const int row_bytes = width;
		const int padding = (4 - (row_bytes % 4)) % 4;
		
		data.resize(static_cast<size_t>(width) * height * 4);

		inp.seekg(file_header.offset_data, std::ios::beg);

		std::vector<uint8_t> row(static_cast<size_t>(row_bytes));

		const bool bottomUp = (info_header.height > 0);

		for (int y = 0; y < height; y++) {
			inp.read(reinterpret_cast<char*>(row.data()), row_bytes);
			inp.ignore(padding);

			int out_y = bottomUp ? (height - 1 - y) : y;

			for (int x = 0; x < width; x++) {
				uint8_t idx = row[x];
				auto &p = palette[idx];

				size_t out = (static_cast<size_t>(out_y) * width + x) * 4;
				data[out + 0] = p.blue;
				data[out + 1] = p.green;
				data[out + 2] = p.red;
				data[out+3] = 255;

			}
		}

	}

	void read32bit(std::ifstream& inp) {
		if (info_header.width % 4 == 0) {
			inp.read((char*)data.data(), data.size());
			file_header.file_size += static_cast<uint32_t>(data.size());
		} else {
			row_stride = info_header.width * info_header.bit_count / 8;
			uint32_t new_stride = make_stride_aligned(4);
			std::vector<uint8_t> padding_row(new_stride - row_stride);
			
			for (int y = 0; y < info_header.height; ++y) {
				inp.read((char*)(data.data() + row_stride * y), row_stride);
				inp.read((char*)padding_row.data(), padding_row.size());
			}
			file_header.file_size += static_cast<uint32_t>(data.size()) + info_header.height * static_cast<uint32_t>(padding_row.size());
		}
	}
	

	uint32_t row_stride { 0 };
	void fileInfoTable() {
		std::cout << "bpp=" << info_header.bit_count
          << " dib=" << info_header.size
          << " off=" << file_header.offset_data
          << " palette_entries=" << palette.size()
          << "\n";


		printf("DIB header size: %u (0x%08X)\n", info_header.size, info_header.size);
		printf("Planes: %u (0x%04X)\n", info_header.planes, info_header.planes);
		printf("Bit count: %u (0x%04X)\n", info_header.bit_count, info_header.bit_count);
		
		printf("File type (HEX): ");
		printf("0x%04X\n", file_header.file_type);
        printf("File type (CHAR): ");
        printf("%c %c\n", file_header.file_type & 0xFF, (file_header.file_type >> 8) & 0xFF);
		printf("\n");
		printf("File size (HEX): ");
		printf("0x%08X\n", (file_header.file_size)); // We don't add the mask 0xFFFFFFFF here because the we are already expecting 32bits in %08X
		printf("File size (BYTES): ");
		printf("%u Bytes\n", file_header.file_size);
		printf("Bit count (HEX): ");
		printf("0x%04X\n", info_header.bit_count);
		printf("Bit count: ");
		printf("%u\n", info_header.bit_count);
		printf("Height: ");
		printf("%u\n", info_header.height);
	}
	
	void check_color_header(BMPColorHeader &color_header) {
		BMPColorHeader expected_color_header;
		
		if (expected_color_header.red_mask != color_header.red_mask || 
			expected_color_header.blue_mask != color_header.blue_mask ||
			expected_color_header.green_mask != color_header.green_mask ||
			expected_color_header.alpha_mask != color_header.alpha_mask) {
				throw std::runtime_error("Unexpected color mask format! This program expects the pixel data to be in the BGRA format");
		}
		
		if (expected_color_header.color_space_type != color_header.color_space_type) {
			throw std::runtime_error("Unexpected color space type! The programa expects sRGB values");
		}
	}
	
	uint32_t make_stride_aligned(uint32_t align_stride) {
		uint32_t new_stride = row_stride;
		while (new_stride % align_stride != 0) {
			new_stride++;
		}
		return new_stride;
	}
};