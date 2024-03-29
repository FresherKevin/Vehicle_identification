// vibe1.cpp: 定义控制台应用程序的入口点。
//
#include "stdafx.h"
#include <iostream>
#include <cv.hpp>
#include <highgui.hpp>
#include "vibe-background-sequential.h"
#include <time.h>
#include <ml.hpp>

using namespace cv;
using namespace std;
using namespace cv::ml;

void processVideo(char* videoFilename);
int main(int argc, char* argv[])
{
	string path = "src2.mp4";
	/* Create GUI windows. */
	//load svm model;
	//namedWindow("segErode");
	namedWindow("Origin");
	namedWindow("Segmentation by ViBe");
	processVideo((char*)path.c_str());

	/* Destroy GUI windows. */
	destroyAllWindows();
	return EXIT_SUCCESS;
}
void processVideo(char* videoFilename)
{
	Ptr<SVM> svm = StatModel::load<SVM>("vehicleType.xml");
	/* Create the capture object. */
	VideoCapture capture(videoFilename);
	//VideoCapture capture2("src2.mp4");
	if (!capture.isOpened()) {
		/* Error in opening the video input. */
		cerr << "Unable to open video file: " << videoFilename << endl;
		exit(EXIT_FAILURE);
	}
	/* Variables. */
	static int frameNumber = 1; /* The current frame number */
	Mat frame;                  /* Current Mat for vibe*/
	Mat origin;					/*origin frame */
	Mat segmentationMap;        /* Will contain the segmentation map. This is the binary output map. */
	int keyboard = 0;           /* Input from keyboard. Used to stop the program. Enter 'q' to quit. */

								/* Model for ViBe. */
	vibeModel_Sequential_t *model = NULL; /* Model used by ViBe. */
	clock_t start, finish;
	double totaltime;
	start = clock();

	int ex = static_cast<int>(capture.get(CV_CAP_PROP_FOURCC));     // Get Codec Type- Int form														   // Transform from int to char via Bitwise operators
	char EXT[] = { ex & 0XFF , (ex & 0XFF00) >> 8,(ex & 0XFF0000) >> 16,(ex & 0XFF000000) >> 24, 0 };
	Size s = Size((int)capture.get(CV_CAP_PROP_FRAME_WIDTH), (int)capture.get(CV_CAP_PROP_FRAME_HEIGHT));
	VideoWriter output_dst1("src.mp4",ex, capture.get(CV_CAP_PROP_FPS),s, false);
	//cout << "Input codec type: " << EXT << endl;
	//VideoWriter output_dst2("dst2.avi", ex, capture.get(CV_CAP_PROP_FPS), s, false);

	//Read input data. ESC or 'q' for quitting. */
	while ((char)keyboard != 'q' && (char)keyboard != 27) {
		/* Read the current frame. */
		if (!capture.read(frame)) {
			cerr << "Unable to read next frame." << endl;
			break;
		}
		/* Applying ViBe.
		* If you want to use the grayscale version of ViBe (which is much faster!):
		* (1) remplace C3R by C1R in this file.
		* (2) uncomment the next line (cvtColor).
		*/
		double temp = (double)150 / (double)frame.cols;
		origin = frame.clone();
		resize(frame, frame, Size(), temp, temp);
		cvtColor(frame, frame, CV_BGR2GRAY);

		if (frameNumber == 100) {//前100帧更新率大
			libvibeModel_Sequential_SetUpdateFactor(model, 15);
		}
		if (frameNumber == 1) {
			segmentationMap = Mat(frame.rows, frame.cols, CV_8UC1);
			model = (vibeModel_Sequential_t*)libvibeModel_Sequential_New();
			libvibeModel_Sequential_AllocInit_8u_C1R(model, frame.data, frame.cols, frame.rows);
		}

		/* ViBe: Segmentation and updating. */
		libvibeModel_Sequential_Segmentation_8u_C1R(model, frame.data, segmentationMap.data);
		libvibeModel_Sequential_Update_8u_C1R(model, frame.data, segmentationMap.data);

		/* Post-processes the segmentation map. This step is not compulsory.
		Note that we strongly recommend to use post-processing filters, as they
		always smooth the segmentation map. For example, the post-processing filter
		used for the Change Detection dataset (see http://www.changedetection.net/ )
		is a 5x5 median filter. */
		medianBlur(segmentationMap, segmentationMap, 5); /* 3x3 median filtering */
		Mat element = getStructuringElement(MORPH_RECT, Size(3, 3));
		dilate(segmentationMap, segmentationMap, element);


		vector<vector<Point>> contours;
		vector<Vec4i> hierarchy;

		findContours(segmentationMap, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);//CV_RETR_EXTERNAL只检测外部轮廓，可根据自身需求进行调整
		CvFont *defaultFont = NULL;
		CvFont *font;
		font = defaultFont = new CvFont;
		cvInitFont(font, CV_FONT_HERSHEY_DUPLEX, 0.5, 0.5, 0, 1);

		for (int i = 0; i<contours.size(); i++) {
			cv::drawContours(segmentationMap, contours, i, Scalar(0)/*, 1, 8, hierarchy*/);//描绘字符的外轮廓
			Rect rect = boundingRect(contours[i]);//检测外轮廓
			rect.height = (int)((double)rect.height / temp);
			rect.width = (int)((double)rect.width / temp);
			rect.x = (int)((double)rect.x / temp);
			rect.y = (int)((double)rect.y / temp);
			//if (rect.height < 60 || rect.width < 60)continue;
			Mat cutedArea = origin(rect);//切割目标区域
			resize(cutedArea, cutedArea, cvSize(64, 64));

			HOGDescriptor * hogscriptor = new HOGDescriptor(cvSize(64, 64), cvSize(16, 16), cvSize(8, 8), cvSize(8, 8), 9);
			vector<float> descriptors;  //存放提取HOG特征向量的结果数组 
			hogscriptor->compute(cutedArea, descriptors, Size(8, 8), Size(0, 0)); //对读入的样本图片提取HOG特征向量 
			rectangle(origin, rect, Scalar(0, 0, 255), 1);//对外轮廓加矩形框

			Mat SVMtrainMat = Mat::zeros(1, descriptors.size(), CV_32FC1);

			int n = 0;
			for (vector<float>::iterator iter = descriptors.begin(); iter != descriptors.end(); iter++)
			{
				SVMtrainMat.at<float>(0, n) = *iter;
				n++;
			}
			int ret = svm->predict(SVMtrainMat);
			string type;
			switch (ret)
			{
			case 0: {
				type = "car";
				break;
			}
			case 1: {
				type = "van";
				break;
			}
			case 2: {
				type = "bus";
				break;
			}
			default:
				break;
			}
			putText(origin, type, cv::Point(rect.x, rect.y + 4), FONT_HERSHEY_COMPLEX, 1, cv::Scalar(255, 255, 0));
		}
		/* Shows the current frame and the segmentation map. */
		imshow("Origin", origin);
		imshow("Segmentation by ViBe", segmentationMap);
		//添加桢到dst1.avi
		output_dst1.write(origin);
		//添加桢到dst2.avi
		//output_dst2.write(segmentationMap);
		++frameNumber;
		/* Gets the input from the keyboard. */
		keyboard = waitKey(1);
	}
	/* Delete capture object. */
	capture.release();
	/* Frees the model. */
	libvibeModel_Sequential_Free(model);
	finish = clock();
	totaltime = (double)(finish - start) / CLOCKS_PER_SEC;
	cout << "\n此程序的运行时间为" << totaltime << "秒！" << endl;
	system("pause");
}
