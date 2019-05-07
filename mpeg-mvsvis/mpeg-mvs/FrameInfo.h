#pragma once
#include <inttypes.h>
#include <utility>
#include <vector>

class FrameInfo
{
public:
	FrameInfo();
	void FrameInfo::InterpolateFlow(FrameInfo& prev, FrameInfo& next);
	void FrameInfo::FillInSomeMissingVectorsInGrid8();
	void FrameInfo::PrintIfNotPrinted();

	size_t GridStep;
	std::pair<size_t, size_t> Shape;

	std::vector<std::vector<int>> dx;
	std::vector<std::vector<int>> dy;
	std::vector<std::vector<uint8_t>> occupancy;

	int64_t Pts = -1;
	int FrameIndex = -1;
	char PictType = '?';
	const char* Origin = "";
	bool Empty = true;
	bool Printed = false;
	bool OutputOccupancy = false;
};
