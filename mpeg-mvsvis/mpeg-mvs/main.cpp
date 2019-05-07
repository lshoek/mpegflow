/*
 * http://ffmpeg.org/doxygen/trunk/index.html
 *
 * Main components
 *
 * Format (Container) - a wrapper, providing sync, metadata and muxing for the streams.
 * Stream - a continuous stream (audio or video) of data over time.
 * Codec - defines how data are enCOded (from Frame to Packet)
 *        and DECoded (from Packet to Frame).
 * Packet - are the data (kind of slices of the stream data) to be decoded as raw frames.
 * Frame - a decoded raw frame (to be encoded or filtered).
 */

#pragma once

extern "C"
{
	#include "libavcodec\avcodec.h"
	#include "libavformat\avformat.h"
	#include <libavutil\motion_vector.h>
}
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string>
#include <vector>
#include <algorithm>
#include "FrameInfo.h"

void ffmpeg_init();
void ffmpeg_print_error(int err);
void log(const char *fmt, ...);

AVFrame *pFrame;
AVFormatContext* pFormatContext;
AVCodecContext* pVideoCodecContext;
AVStream* videoStream;

const size_t MAX_GRID_SIZE = 512;
static bool ARG_OUTPUT_OCCUPANCY, ARG_OUTPUT_RAW_MOTION_VECTORS, ARG_FORCE_GRID_8, ARG_QUIET, ARG_HELP;
int video_stream_index;

size_t frameWidth, frameHeight;
std::string inFile = "../resources/videos/sintel.avi";
std::string outFile = "../resources/mvs/mvs.txt";

bool process_frame(AVPacket *packet)
{
	av_frame_unref(pFrame);

	int got_frame = 0;
	int ret = avcodec_decode_video2(videoStream->codec, pFrame, &got_frame, packet);
	if (ret < 0) return false;

	ret = FFMIN(ret, packet->size); /* guard against bogus return values */
	packet->data += ret;
	packet->size -= ret;
	return got_frame > 0;
}

bool read_packets()
{
	static bool initialized = false;
	static AVPacket packet, pktCopy;

	while (true)
	{
		if (initialized)
		{
			if (process_frame(&pktCopy))
				return true;
			else
			{
				av_packet_unref(&packet);
				initialized = false;
			}
		}

		int ret = av_read_frame(pFormatContext, &packet);
		if (ret != 0) break;

		initialized = true;
		pktCopy = packet;
		if (packet.stream_index != video_stream_index)
		{
			av_packet_unref(&packet);
			initialized = false;
			continue;
		}
	}
	return process_frame(&packet);
}

bool read_frame(int64_t& pts, char& pictType, std::vector<AVMotionVector>& motion_vectors)
{
	if (!read_packets()) return false;

	pictType = av_get_picture_type_char(pFrame->pict_type);

	// fragile, consult fresh f_select.c and ffprobe.c when updating ffmpeg
	pts = pFrame->pkt_pts != AV_NOPTS_VALUE ? pFrame->pkt_pts : (pFrame->pkt_dts != AV_NOPTS_VALUE ? pFrame->pkt_dts : pts + 1);
	
	bool noMotionVectors = av_frame_get_side_data(pFrame, AV_FRAME_DATA_MOTION_VECTORS) == NULL;
	if (!noMotionVectors)
	{
		// reading motion vectors, see ff_print_debug_info2 in ffmpeg's libavcodec/mpegvideo.c for reference and a fresh doc/examples/extract_mvs.c
		AVFrameSideData* sd = av_frame_get_side_data(pFrame, AV_FRAME_DATA_MOTION_VECTORS);
		AVMotionVector* mvs = (AVMotionVector*)sd->data;
		int mvcount = sd->size / sizeof(AVMotionVector);

		//append results
		motion_vectors = std::vector<AVMotionVector>(mvs, mvs + mvcount);
	}
	else
	{
		motion_vectors = std::vector<AVMotionVector>();
	}
	return true;
}

void output_vectors_raw(int frameIndex, int64_t pts, char pictType, std::vector<AVMotionVector>& motionVectors)
{
	printf("# pts=%lld frame_index=%d pict_type=%c output_type=raw shape=%zux4\n", (long long)pts, frameIndex, pictType, motionVectors.size());
	for (int i = 0; i < motionVectors.size(); i++)
	{
		AVMotionVector& mv = motionVectors[i];
		int mvdx = mv.dst_x - mv.src_x;
		int mvdy = mv.dst_y - mv.src_y;

		printf("%d\t%d\t%d\t%d\n", mv.dst_x, mv.dst_y, mvdx, mvdy);
	}
}

void output_vectors_std(int frameIndex, int64_t pts, char pictType, std::vector<AVMotionVector>& motionVectors)
{
	std::vector<FrameInfo> prev;

	size_t gridStep = ARG_FORCE_GRID_8 ? 8 : 16;
	std::pair<size_t, size_t> shape = 
		std::make_pair(std::min(frameHeight / gridStep, MAX_GRID_SIZE),
		std::min(frameWidth / gridStep, MAX_GRID_SIZE));

	if (!prev.empty() && pts != prev.back().Pts + 1)
	{
		for (int64_t dummy_pts = prev.back().Pts + 1; dummy_pts < pts; dummy_pts++)
		{
			FrameInfo dummy;
			dummy.GridStep = gridStep;
			dummy.Shape = shape;
			//dummy.FrameIndex = -1;
			//dummy.Pts = dummy_pts;
			//dummy.Origin = "dummy";
			//dummy.PictType = '?';
			prev.push_back(dummy);
		}
	}

	FrameInfo cur;
	cur.FrameIndex = frameIndex;
	cur.Pts = pts;
	cur.Origin = "video";
	cur.PictType = pictType;
	cur.GridStep = gridStep;
	cur.Shape = shape;
	cur.OutputOccupancy = ARG_OUTPUT_OCCUPANCY;

	for (int i = 0; i < motionVectors.size(); i++)
	{
		AVMotionVector& mv = motionVectors[i];
		int mvdx = mv.dst_x - mv.src_x;
		int mvdy = mv.dst_y - mv.src_y;

		size_t i_clipped = std::max(size_t(0), std::min(mv.dst_y / cur.GridStep, cur.Shape.first - 1));
		size_t j_clipped = std::max(size_t(0), std::min(mv.dst_x / cur.GridStep, cur.Shape.second - 1));

		cur.Empty = false;
		cur.dx[i_clipped][j_clipped] = mvdx;
		cur.dy[i_clipped][j_clipped] = mvdy;
		cur.occupancy[i_clipped][j_clipped] = true;
	}

	if (cur.GridStep == 8)
		cur.FillInSomeMissingVectorsInGrid8();

	if (frameIndex == -1)
	{
		for (int i = 0; i < prev.size(); i++)
			prev[i].PrintIfNotPrinted();
	}
	else if (!motionVectors.empty())
	{
		if (prev.size() == 2 && prev.front().Empty == false)
		{
			prev.back().InterpolateFlow(prev.front(), cur);
			prev.back().PrintIfNotPrinted();
		}
		else
		{
			for (int i = 0; i < prev.size(); i++)
				prev[i].PrintIfNotPrinted();
		}
		prev.clear();
		cur.PrintIfNotPrinted();
	}
	prev.push_back(cur);
}

void ffmpeg_init()
{
	av_register_all();

	pFrame = av_frame_alloc();
	pFormatContext = avformat_alloc_context();
	video_stream_index = -1;

	log("initializing all the containers, codecs and protocols.");

	int err = 0;
	if ((err = avformat_open_input(&pFormatContext, inFile.c_str(), NULL, NULL)) != 0)
	{
		ffmpeg_print_error(err);
		throw std::runtime_error("Couldn't open file. Possibly it doesn't exist.");
	}
	// read file header and say what format (container) it is, and some other information related to the format itself.
	log("format %s, duration %lld us, bit_rate %lld", pFormatContext->iformat->name, pFormatContext->duration, pFormatContext->bit_rate);

	// read packets from format to get stream info. this function populates pFormatContext->streams (size equals pFormatContext->nb_streams)
	// function arguments: AVFormatContext, options contains options for codec corresponding to i-th stream.
	if ((err = avformat_find_stream_info(pFormatContext, NULL)) < 0)
	{
		ffmpeg_print_error(err);
		throw std::runtime_error("Stream information not found.");
	}

	// this component knows how to enCOde and DECode the stream
	AVCodec *pCodec = NULL;

	// loop though all the streams and print its main information
	for (int i = 0; i < pFormatContext->nb_streams; i++)
	{
		log("finding the proper decoder (CODEC)");
		AVCodecContext *enc = pFormatContext->streams[i]->codec;
		if (AVMEDIA_TYPE_VIDEO == enc->codec_type && video_stream_index < 0)
		{
			pVideoCodecContext = enc;
			AVCodec *pCodec = avcodec_find_decoder(enc->codec_id);
			AVDictionary *options = NULL;

			// export motion vector information to dictionary
			av_dict_set(&options, "flags2", "+export_mvs", 0);

			if (!pCodec || avcodec_open2(enc, pCodec, &options) < 0)
				throw std::runtime_error("Codec not found or cannot open codec.");

			video_stream_index = i;
			videoStream = pFormatContext->streams[i];
			frameWidth = enc->width;
			frameHeight = enc->height;
			break;
		}
	}
	if (video_stream_index == -1)
		throw std::runtime_error("Video stream not found.");
}

void log(const char *fmt, ...)
{
	va_list args;
	fprintf(stderr, "log: ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

// copied from cmdutils.c, originally called print_error
void ffmpeg_print_error(int err)
{
	char errbuf[128];
	const char *errbuf_ptr = errbuf;

	if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
		errbuf_ptr = strerror(AVUNERROR(err));
	av_log(NULL, AV_LOG_ERROR, "ffmpeg_print_error: %s\n", errbuf_ptr);
}

int main(int argc, const char *argv[])
{
	int response = 0;
	int packets_to_process = 8;

	char pictType;
	int64_t pts, prev_pts = -1;
	std::vector<AVMotionVector> motionVectors;

	ARG_OUTPUT_OCCUPANCY = false;
	ARG_OUTPUT_RAW_MOTION_VECTORS = false;
	ARG_FORCE_GRID_8 = true;
	ARG_QUIET = false;
	ARG_HELP = false;

	freopen(outFile.c_str(), "w", stdout);

	log("extracting motion vectors from video...");
	ffmpeg_init();

	AVFrame *pFrame = av_frame_alloc();
	AVPacket *pPacket = av_packet_alloc();

	log("writing motion vectors to stream...");
	for (int frameIndex = 1; read_frame(pts, pictType, motionVectors); frameIndex++)
	{
		if (pts <= prev_pts && prev_pts != -1)
			fprintf(stderr, "Skipping frame %d (frame with pts %d already processed).\n", int(frameIndex), int(pts));

		if (ARG_OUTPUT_RAW_MOTION_VECTORS)
			output_vectors_raw(frameIndex, pts, pictType, motionVectors);
		else
			output_vectors_std(frameIndex, pts, pictType, motionVectors);

		prev_pts = pts;
	}
	log("writing motion vectors to %s...", outFile.c_str());
	if (!ARG_OUTPUT_RAW_MOTION_VECTORS)
		output_vectors_std(-1, pts, pictType, motionVectors);

	avformat_close_input(&pFormatContext);
	avformat_free_context(pFormatContext);
	av_packet_free(&pPacket);
	av_frame_free(&pFrame);
	avcodec_free_context(&pVideoCodecContext);

	log("finished");
	fclose(stdout);
	return 0;
}
