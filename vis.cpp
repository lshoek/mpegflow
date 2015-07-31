#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <string>
#include <sstream>
#include <iomanip>
#include <utility>

#include <opencv2/highgui/highgui.hpp>

using namespace std;
using namespace cv;

const char* ARG_VIDEO_PATH = NULL;
const char* ARG_DUMP_DIR = NULL;
bool ARG_HELP;

void drawArrow(Mat img, Point pStart, Point pEnd, double len, double alphaDegrees, Scalar lineColor, Scalar startColor)
{    
	const double PI = acos(-1);
	const int lineThickness = 1;
	const int lineType = CV_AA;

	double angle = atan2((double)(pStart.y - pEnd.y), (double)(pStart.x - pEnd.x));  
	line(img, pStart, pEnd, lineColor, lineThickness, lineType);
	img.at<Vec3b>(pStart) = Vec3b(startColor[0], startColor[1], startColor[2]);
	if(len > 0)
	{
		for(int k = 0; k < 2; k++)
		{
			int sign = k == 1 ? 1 : -1;
			Point arrow(pEnd.x + len * cos(angle + sign * PI * alphaDegrees / 180), pEnd.y + len * sin(angle + sign * PI * alphaDegrees / 180));
			line(img, pEnd, arrow, lineColor, lineThickness, lineType);   
		}
	}
}

void visFlow(pair<Mat, int> flow, Mat frame, const char* dumpDir)
{
	Mat flowComponents[3];
	split(flow.first, flowComponents);
	int rows = flowComponents[0].rows;
	int cols = flowComponents[0].cols;

	Mat img = frame.clone();
	for(int i = 0; i < rows; i++)
	{
		for(int j = 0; j < cols; j++)
		{
			int dx = flowComponents[0].at<int>(i, j);
			int dy = flowComponents[1].at<int>(i, j);
			
			Point start(double(j) / cols * img.cols + img.cols / cols / 2, double(i) / rows * img.rows + img.rows / rows / 2);
			Point end(start.x + dx, start.y + dy);
			
			drawArrow(img, start, end, 2.0, 20.0, CV_RGB(255, 0, 0), CV_RGB(0, 255, 255));
		}
	}
	
	stringstream s;
	s << (dumpDir != NULL ? dumpDir : "") << "/" << setfill('0') << setw(6) << flow.second << ".png";
	
	if(dumpDir != NULL)
	{	
		imwrite(s.str(), img);
	}
	else
	{
		imshow(s.str(), img);
		waitKey();
	}
}

pair<Mat, int> readFlow()
{
	int rows, cols, frameIndex;
	bool ok = scanf("# pts=%*ld frame_index=%d pict_type=%*c output_type=%*s shape=%dx%d origin=%*s\n", &frameIndex, &rows, &cols) == 3 && rows % 3 == 0;
	
	if(!ok)
		return make_pair(Mat(), -1);
	
	rows /= 3;

	Mat_<int> dx(rows, cols), dy(rows, cols), occupancy(rows, cols);
	Mat flowComponents[] = {dx, dy, occupancy};
	for(int k = 0; k < 3; k++)
		for(int i = 0; i < rows; i++)
			for(int j = 0; j < cols; j++)
				assert(scanf("%d ", &flowComponents[k].at<int>(i, j)) == 1);

	Mat flow;
	merge(flowComponents, 3, flow);

	return make_pair(flow, frameIndex);
}

void parse_options(int argc, const char* argv[])
{
	for(int i = 1; i < argc; i++)
	{
		if(strcmp(argv[i], "--dump") == 0 && i + 1 < argc)
		{
			ARG_DUMP_DIR = argv[i + 1];
			i++;
		}
		else if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
		{
			ARG_HELP = true;
		}
		else
			ARG_VIDEO_PATH = argv[i];
	}
	if(ARG_HELP || ARG_VIDEO_PATH == NULL)
	{
		fprintf(stderr, "Usage: cat mpegflow.txt | ./vis [--dump dumpDir] videoPath\n  --help and -h will output this help message.\n  --dump will skip showing visualization on screen and will save the images to dumpDir instead\n");
		exit(1);
	}
}

int main(int argc, const char* argv[])
{
	parse_options(argc, argv);

	pair<Mat, int> flow = readFlow();

	VideoCapture in(ARG_VIDEO_PATH);
	Mat frame;
	for(int opencvFrameIndex = 1; in.read(frame) ; opencvFrameIndex++)
	{
		if(opencvFrameIndex == flow.second)
		{
			visFlow(flow, frame, ARG_DUMP_DIR);
			flow = readFlow();
		}
		else
			fprintf(stderr, "Skipping frame %d.\n", int(opencvFrameIndex));
	}
}