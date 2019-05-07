#include "FrameInfo.h"

FrameInfo::FrameInfo()
{
	dx = std::vector<std::vector<int>>(255, std::vector<int>(255));
	dy = std::vector<std::vector<int>>(255, std::vector<int>(255));
	occupancy = std::vector<std::vector<uint8_t>>(255, std::vector<uint8_t>(255));
}

void FrameInfo::InterpolateFlow(FrameInfo& prev, FrameInfo& next)
{
	for (int i = 0; i < Shape.first; i++)
	{
		for (int j = 0; j < Shape.second; j++)
		{
			dx[i][j] = (prev.dx[i][j] + next.dx[i][j]) / 2;
			dy[i][j] = (prev.dy[i][j] + next.dy[i][j]) / 2;
		}
	}
	Empty = false;
	Origin = "interpolated";
}

void FrameInfo::FillInSomeMissingVectorsInGrid8()
{
	for (int k = 0; k < 2; k++)
	{
		for (int i = 1; i < Shape.first - 1; i++)
		{
			for (int j = 1; j < Shape.second - 1; j++)
			{
				if (occupancy[i][j] == 0)
				{
					if (occupancy[i][j - 1] != 0 && occupancy[i][j + 1] != 0)
					{
						dx[i][j] = (dx[i][j - 1] + dx[i][j + 1]) / 2;
						dy[i][j] = (dy[i][j - 1] + dy[i][j + 1]) / 2;
						occupancy[i][j] = 2;
					}
					else if (occupancy[i - 1][j] != 0 && occupancy[i + 1][j] != 0)
					{
						dx[i][j] = (dx[i - 1][j] + dx[i + 1][j]) / 2;
						dy[i][j] = (dy[i - 1][j] + dy[i + 1][j]) / 2;
						occupancy[i][j] = 2;
					}
				}
			}
		}
	}
}

void FrameInfo::PrintIfNotPrinted()
{
	static int64_t FirstPts = -1;

	if (Printed)
		return;

	if (FirstPts == -1)
		FirstPts = Pts;

	printf("# pts=%lld frame_index=%d pict_type=%c output_type=arranged shape=%zux%zu origin=%s\n", (long long)Pts - FirstPts, FrameIndex, PictType, (OutputOccupancy ? 3 : 2) * Shape.first, Shape.second, Origin);
	for (int i = 0; i < Shape.first; i++)
	{
		for (int j = 0; j < Shape.second; j++)
		{
			printf("%d\t", dx[i][j]);
		}
		printf("\n");
	}
	for (int i = 0; i < Shape.first; i++)
	{
		for (int j = 0; j < Shape.second; j++)
		{
			printf("%d\t", dy[i][j]);
		}
		printf("\n");
	}

	if (OutputOccupancy)
	{
		for (int i = 0; i < Shape.first; i++)
		{
			for (int j = 0; j < Shape.second; j++)
			{
				printf("%d\t", occupancy[i][j]);
			}
			printf("\n");
		}
	}

	Printed = true;
}
