#include "pch.h"
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <string>
#include <sstream>
#include <iomanip>
#include <utility>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

using namespace std;
using namespace cv;

bool OCCUPANCY = false;

string VIDEO_PATH = "../resources/videos/sintel.avi";
string MVS_PATH = "../resources/mvs/mvs.txt";
string DUMP_DIR = "../resources/output";
string WND_NAME = "mvs-vis";

double alpha = 0.85;				// default 0.75
double ksize = 9;					// default 11
double mulstrokeWeight = 2.0;		// default 1.25

Mat black;

FILE* file = NULL;

void vis_flow(pair<Mat, int> flow, Mat srcFrame, Mat &dstFrame)
{
	Mat flowComponents[3];
	split(flow.first, flowComponents);
	int rows = flowComponents[0].rows;
	int cols = flowComponents[0].cols;

	double beta = (1.0 - alpha);
	addWeighted(dstFrame, alpha, black, beta, 0.0, dstFrame);

	for (int i = 0; i < rows; i++)
	{
		for (int j = 0; j < cols; j++)
		{
			int dx = flowComponents[0].at<int>(i, j);
			int dy = flowComponents[1].at<int>(i, j);
			int occupancy = flowComponents[2].at<int>(i, j);

			int stride = srcFrame.rows / rows;
			Vec3b sample = srcFrame.at<Vec3b>(Point(j*stride, i*stride)); //.mul(Vec3b(0.0, 0.0, 255.0));

			Point start(double(j) / cols * dstFrame.cols + dstFrame.cols / cols / 2, double(i) / rows * dstFrame.rows + dstFrame.rows / rows / 2);
			Point end(start.x + dx, start.y + dy);

			double mag = sqrt(dx*dx + dy*dy)/20.0;
			double alpha = min(1.0, mag/4)*255.0;
			Vec4b col = Vec4b(sample[0], sample[1], sample[2], alpha); // .mul(Vec4b(255.0, 255.0, 255.0, 1.0));

			int strokeWeight = (int)max(round(mag*mulstrokeWeight), 1.0);
			if (OCCUPANCY) strokeWeight = max(occupancy, 1) * 2;

			line(dstFrame, start, end, col, strokeWeight);
		}
	}
	GaussianBlur(dstFrame, dstFrame, Size(ksize, ksize), 2);
}

pair<Mat, int> read_flow()
{
	int rows, cols, frameIndex;
	bool ok = fscanf_s(file, "# pts=%*d frame_index=%d pict_type=%*c output_type=%*s shape=%dx%d origin=%*s\n", &frameIndex, &rows, &cols) == 3;
	int d = (OCCUPANCY) ? 3 : 2;

	if (!(ok &&  rows % d == 0))
	{
		return make_pair(Mat(), -1);
	}
	rows /= d;

	Mat_<int> dx(rows, cols), dy(rows, cols), occupancy(rows, cols);
	occupancy = 1;
	Mat flowComponents[] = { dx, dy, occupancy };
	for (int k = 0; k < d; k++)
		for (int i = 0; i < rows; i++)
			for (int j = 0; j < cols; j++)
				assert(fscanf_s(file, "%d ", &flowComponents[k].at<int>(i, j)) == 1);

	Mat flow;
	merge(flowComponents, 3, flow);

	return make_pair(flow, frameIndex);
}

void writeFrameToFile(Mat frame, int frameIndex)
{
	stringstream s;
	s << DUMP_DIR << "/" << setfill('0') << setw(6) << frameIndex << ".png";
	imwrite(s.str(), frame);
}

int display(Mat img, int delay)
{
	imshow(WND_NAME, img);
	int c = waitKey(delay);
	if (c >= 0) { return -1; }
	return 0;
}

int main(int argc, const char* argv[])
{
	fopen_s(&file, MVS_PATH.c_str(), "r");

	pair<Mat, int> flow = read_flow();

	VideoCapture in(VIDEO_PATH);
	Mat srcFrame, dstFrame, outFrame;

	assert(in.read(srcFrame));
	dstFrame = Mat(Size(srcFrame.cols, srcFrame.rows), CV_8UC4, Scalar(0, 0, 0, 0));
	black = Mat(Size(srcFrame.cols, srcFrame.rows), dstFrame.type(), Scalar(0, 0, 0, 1.0));
	outFrame = Mat(Size(srcFrame.cols, srcFrame.rows), CV_8UC3, Scalar(0, 0, 0));

	fprintf(stderr, "generating motion vectors...\n");

	int fskipped = 0;
	for (int opencvFrameIndex = 1; in.read(srcFrame); opencvFrameIndex++)
	{
		if (opencvFrameIndex == flow.second)
		{
			fprintf(stderr, ".");
			vis_flow(flow, srcFrame, dstFrame);

			display(dstFrame, 1);
			cvtColor(dstFrame, outFrame, cv::COLOR_BGRA2BGR);
			writeFrameToFile(outFrame, flow.second-fskipped);
			flow = read_flow();
		}
		else
		{
			fskipped++;
			fprintf(stderr, "x");
		}
	}
	fprintf(stderr, "\nfinished.\n");
	waitKey(0);
}
