// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "support/ImageSafety.h"
#include <cstdlib>
#include <iostream>
#include <vector>

int main()
{
	int checks = 0;
	auto check = [&](bool value) { ++checks; if (!value) { std::cerr << "image check " << checks << " failed\n"; std::exit(1); } };
	std::vector<unsigned char> png = {137,80,78,71,13,10,26,10, 0,0,0,13, 'I','H','D','R',
		0,0,4,0, 0,0,3,0, 8,2,0,0,0, 0,0,0,0};
	auto inspect = [&](const auto& bytes) { return Upload::InspectPreviewImage(bytes.data(), bytes.size()); };
	auto dimensions = inspect(png);
	check(dimensions && dimensions->width == 1024 && dimensions->height == 768);
	for (size_t length = 0; length < png.size(); ++length)
		check(!Upload::InspectPreviewImage(png.data(), length));
	auto malicious = png;
	malicious[16] = 0xff;
	check(!inspect(malicious));
	malicious = png; malicious[18] = 0; check(!inspect(malicious));
	malicious = png; malicious[18] = 0x40; malicious[22] = 0x40; check(!inspect(malicious));
	malicious = png; malicious[11] = 12; check(!inspect(malicious));
	std::vector<unsigned char> jpeg = {0xff,0xd8, 0xff,0xe0, 0,4, 0,0,
		0xff,0xc0, 0,11, 8, 3,0, 4,0, 1, 1,0x11,0};
	dimensions = inspect(jpeg);
	check(dimensions && dimensions->width == 1024 && dimensions->height == 768);
	jpeg[9] = 0xc2; check(inspect(jpeg).has_value());
	for (size_t length = 0; length < jpeg.size(); ++length)
		check(!Upload::InspectPreviewImage(jpeg.data(), length));
	malicious = jpeg; malicious[14] = 0; malicious[13] = 0; check(!inspect(malicious));
	malicious = jpeg; malicious[15] = 0xff; check(!inspect(malicious));
	malicious = jpeg; malicious[5] = 1; check(!inspect(malicious));
	malicious = jpeg; malicious[12] = 16; check(!inspect(malicious));
	malicious = jpeg; malicious[9] = 0xc3; check(!inspect(malicious));
	malicious = jpeg; malicious[9] = 0xda; check(!inspect(malicious));
	malicious = jpeg; malicious[17] = 4; check(!inspect(malicious));
	check(!Upload::InspectPreviewImage(nullptr, 9999));
	const unsigned char gif[] = {'G','I','F','8','9','a',1,0,1,0};
	check(!Upload::InspectPreviewImage(gif, sizeof(gif)));
	std::cout << checks << " image resource-preflight checks passed\n";
}
