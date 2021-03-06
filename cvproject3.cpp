//#include "stdafx.h"

#include <opencv2/opencv.hpp>
#include <iostream>
#include <cmath>
#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <Eigen/Dense>
using namespace cv;
using namespace std;

void binary_pic(Mat & img)
{
	for (int i = 0; i < img.rows; i++)
	{
		for (int j = 0; j < img.cols; j++)
		{
			if (!(img.at<uchar>(i, j) < 120 && img.at<uchar>(i, j) > 20))
				img.at<uchar>(i, j) = 0;
			else
				img.at<uchar>(i, j) = 255;
		}
	}
}

//get the rough center of the circle by SimpleBlobDetector
vector<Point2f> get_rough_centers_by_blob(Mat & img, int thresholdStep, int minThreshold,int maxThreshold, int minDistBetweenBlobs,int blobColor,int minArea, int maxArea,int minCircularity, Mat &draw_img)
{
	SimpleBlobDetector::Params params;
	//閾值控制
	params.thresholdStep = 10; //二值化的阈值步长
	params.minThreshold = 50;  //二值化的起始阈值
	params.maxThreshold = 220;  //二值化的终止阈值
	params.minRepeatability = 5;  //重复的最小次数，只有属于灰度图像斑点的那些二值图像斑点数量大于该值时，
									//该灰度图像斑点才被接受
	params.minDistBetweenBlobs = 10*3;  //最小的斑点距离

	//斑点颜色控制
	params.filterByColor = true;
	params.blobColor = 0;    //表示只提取白色斑点；
							//如果该变量为255，表示只提取白色斑点

	//像素面積大小控制
	params.filterByArea = true;
	params.minArea = 600*3;  //斑点的最小面积
	params.maxArea = 8000*3;   //斑点的最大面积
	//形狀（圆）
	params.filterByCircularity = true;
	params.minCircularity = 0.7;   //斑点的最小圆度
	//形狀（凸） 
	params.filterByConvexity = true;
	params.minConvexity = 0.9;  //斑点的最小凸度
	//斑点惯性率
	params.filterByInertia = false;
	params.minInertiaRatio = 0.5;   //斑点的最小惯性率

	Ptr<SimpleBlobDetector> detector = SimpleBlobDetector::create(params);
	vector<KeyPoint> keypoints;
	detector->detect(img, keypoints);
	vector<Point2f> rough_center(int(keypoints.size()));
	for (int i = 0; i < int(keypoints.size()); i++)
	{
		
		rough_center[i].x = keypoints[i].pt.x;
		rough_center[i].y = keypoints[i].pt.y;
	}
	return rough_center;
}
//triangulation 
Point2f pixel2cam(const Point2d& p, const Mat& K)
{
	return Point2f
	(
		(p.x - K.at<double>(0, 2)) / K.at<double>(0, 0),
		(p.y - K.at<double>(1, 2)) / K.at<double>(1, 1)
	);
}
//compute the 3D cordinate of two circle centor
Eigen::Vector3d get_3d_coordinate(const Point2d left, const Point2d right)
{

	Eigen::Matrix3d  Rotation1to2, Rotation2to1;
	Rotation1to2 << 0.999585426780172, -0.012918895796858, -0.025730851134213,
		0.013320644153222, 0.999791075039552, 0.0155037644009375,
		0.0255251838004028, -0.0158400884671288, 0.999548676448180;
	Rotation2to1 = Rotation1to2.inverse();
	Eigen::Vector3d translation1to2, translation2to1;
	translation1to2 << -626.282183743998, 2.75695252294966, 11.6702170037552;
	translation2to1 = -translation1to2;
	Mat left_k = (Mat_<double>(3, 3) << 7.659415501692802e+03, 0, 1.247834598440359e+03, 0, 7.657339635627016e+03, 1.232085832287226e+03, 0, 0, 1);
	Mat right_k = (Mat_<double>(3, 3) << 7.594464903367622e+03, 0, 1.101734656332642e+03, 0, 7.579663788497434e+03, 1.086257026142670e+03, 0, 0, 1);
	Mat T1 = (Mat_<float>(3, 4) <<
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0);
	Mat T2 = (Mat_<float>(3, 4) << Rotation2to1(0, 0), Rotation2to1(0, 1), Rotation2to1(0, 2), translation1to2(0),
		Rotation2to1(1, 0), Rotation2to1(1, 1), Rotation2to1(1, 2), translation1to2(1),
		Rotation2to1(2, 0), Rotation2to1(2, 1), Rotation2to1(2, 2), translation1to2(2)
		);

	Mat pts_4d;
	vector<Point2d> pts_1, pts_2;
	pts_1.push_back(pixel2cam(left, left_k));
	pts_2.push_back(pixel2cam(right, right_k));
	triangulatePoints(T1, T2, pts_1, pts_2, pts_4d);
	Eigen::Vector3d coordinate;
	
	if (pts_4d.cols == 1)
	{

		Mat x = pts_4d.col(0);
		double nn = x.at<double>(3, 0);
		x /= x.at<double>(3, 0);
		coordinate << x.at<double>(0, 0), x.at<double>(1, 0), x.at<double>(2, 0);

	}
	else
		printf("error");
	return coordinate;
	
}
//try to get the rectangle region around the circle
void get_rectangle_rigion(const Mat & img, Point2f center, vector<Point2f> &inner_points, vector<Point2f> &side_points, const Mat &origin_img)
{
	vector<Point2f> upperbound(2), lowerbound(2), leftbound(2), rightbound(2);
	double laplace = 0;
	int threshold = 25;
	int get_upperbound, get_lowerbound, get_leftbound, get_rightbound;
	get_upperbound = get_lowerbound = get_leftbound = get_rightbound = 0;
	while(get_upperbound <2 ) 
	{
		for (int j = 0; j <  100; j++)
		{
			laplace = img.at<uchar>(Point(center.x, center.y - j));
			if (abs(laplace) > threshold)
			{
				if (get_upperbound < 1)
				{
					upperbound[0] = Point2f(center.x, center.y - j);
					get_upperbound++;
					j = j + 5;
					
				}
				else
				{
					upperbound[1] = Point2f(center.x, center.y - j);
					get_upperbound++;
					break;
				}
				  
			}
			laplace = 0;

		}
	}

	while (get_lowerbound < 2)
	{
		for (int j = 0; j < 200; j++)
		{
			laplace = img.at<uchar>(Point(center.x, center.y + j));

			if (abs(laplace) > threshold)
			{
				if (get_lowerbound < 1)
				{
					lowerbound[0] = Point2f(center.x, center.y + j);
					get_lowerbound++;
					j += 5;
				}
				else
				{
					lowerbound[1] = Point2f(center.x, center.y + j);
					get_lowerbound++;
					break;
				}

			}
			laplace = 0;

		}
	}
	bool got_side_bound = false;
	while (get_leftbound <2 )
	{
		for (int j = 0; j < 200; j++)
		{
			laplace = img.at<uchar>(Point(center.x-j, center.y));

			if (abs(laplace) > threshold)
			{
				if (get_leftbound < 1)
				{
					leftbound[0] = Point2f(center.x-j, center.y );
					get_leftbound++;
					j += 5;
				}
				else
				{
					leftbound[1] = Point2f(center.x-j, center.y);
					get_leftbound++;
					break;
				}

			}
			laplace = 0;
		}
	}
	while (get_rightbound < 2)
	{
		for (int j = 0; j < 100; j++)
		{
			laplace = img.at<uchar>(Point(center.x + j, center.y));
			if (abs(laplace) > threshold)
			{
				if (get_rightbound < 1)
				{
					rightbound[0] = Point2f(center.x + j, center.y);
					get_rightbound++;
					j += 5;
				}
				else
				{
					rightbound[1] = Point2f(center.x + j, center.y);
					get_rightbound++;
					break;
				}

			}
			laplace = 0;
		}
	}
	double left_radius = abs(leftbound[0].x - center.x);
	double right_radius = abs(rightbound[0].x - center.x);
	double left_piece = abs(leftbound[1].x - leftbound[0].x);
	double right_piece = abs(rightbound[1].x - rightbound[0].x);
	double horizontal_radius = left_radius >= right_radius ? left_radius : right_radius;
	double horizontal_piece = left_radius <= right_piece ? left_piece : right_piece;

	double upper_radius = abs(upperbound[0].y - center.y);
	double lower_radius = abs(lowerbound[0].y - center.y);
	double upper_piece = abs(upperbound[1].y - upperbound[0].y);
	double lower_piece = abs(lowerbound[1].y - lowerbound[0].y);
	double vertical_radius = upper_radius >= lower_radius ? upper_radius : lower_radius;
	double vertical_piece = upper_piece <= lower_piece ? upper_piece : lower_piece;
	double length = abs(leftbound[0].x-rightbound[0].x)+10;
	double height = abs(upperbound[0].y - lowerbound[0].y)+10;

	vector<Point2f> rectangle(4);
	rectangle[0] = Point2f(center.x - vertical_radius-5, center.y - vertical_radius - 5);
	rectangle[1] = Point2f(2*(vertical_radius+5),2*(vertical_radius + 5));

	double judge;
	for (int i = 0; i < length; i++)
	{
		for (int j = 0; j < height; j++)
		{
			judge = origin_img.at<uchar>(Point(center.x - left_radius+5 + i, center.y - upper_radius  + 5 + j));
			if (judge < 40)
			{
				inner_points.push_back(Point2f(center.x - left_radius  + 5 + i, center.y - upper_radius + 5 + j));
			}
		}
	}
	for (int i = 0; i < length; i++)
	{
		for (int j = 0; j < height; j++)
		{
			judge = img.at<uchar>(Point(center.x - left_radius  + 5 + i, center.y - upper_radius  + 5 + j));
			if (judge > 40)
			{
				side_points.push_back(Point2f(center.x - left_radius  + 5 + i, center.y - upper_radius  + 5 + j));
			}
		}
	}

}
//从圆内一点开始遍历，并通过梯度判断边界情况，得到圆内所有点与圆的边界点，求均值得到新的圆心坐标
void get_circle_features(const Mat & img, Point2f center, vector<Point2f> &inner_points, vector<Point2f> &side_points,const Mat &origin_img)
{
	bool reach_upperbound, reach_lowerbound, reach_leftbound, reach_rightbound, got_left;
	bool reach_upperbound_again, reach_lowerbound_again, reach_sidebound_again;
	reach_upperbound=reach_lowerbound=reach_leftbound=reach_rightbound=got_left=false;
	reach_upperbound_again=reach_lowerbound_again=reach_sidebound_again=false;
	double threshold = 100;

	while (!reach_upperbound)
	{
		for (int i = 0; i < 150; i++)
		{
			got_left = false;
			int upper_laplacian=0;
			reach_leftbound = reach_rightbound = false;
			upper_laplacian = img.at<uchar>(Point(center.x , center.y-i));
			
			if (abs(upper_laplacian) >= threshold)
			{
				side_points.push_back(Point2f(center.x, center.y - i));
				double grad_x = origin_img.at<uchar>(Point(center.x-1, center.y - i-1))*(-1) + origin_img.at<uchar>(Point(center.x-1, center.y - i))*(-2) + origin_img.at<uchar>(Point(center.x-1, center.y - i+1))*(-1)
					+ origin_img.at<uchar>(Point(center.x+1, center.y - i-1)) + origin_img.at<uchar>(Point(center.x+1, center.y - i))*2 + origin_img.at<uchar>(Point(center.x+1, center.y - i+1));
				double symbol = 0;
				double laplacian_2;
				if (-2 < grad_x && grad_x < 2)
				{
					reach_upperbound = true;
					break;
				}
			    else
				{
					symbol = grad_x > 0 ? 1 : -1;
				
					while (!reach_sidebound_again)
					{
						for (int l = 1; l < 50; l++)
						{
							reach_upperbound_again = false;
							laplacian_2 = img.at<uchar>(Point(center.x + symbol * l, center.y - i));
							grad_x = origin_img.at<uchar>(Point(center.x + symbol * l - 1, center.y - i - 1))*(-1) + origin_img.at<uchar>(Point(center.x + symbol * l - 1, center.y - i))*(-2) + origin_img.at<uchar>(Point(center.x + symbol * l - 1, center.y - i + 1))*(-1)
								+ origin_img.at<uchar>(Point(center.x + symbol * l + 1, center.y - i - 1)) + origin_img.at<uchar>(Point(center.x + symbol * l + 1, center.y - i)) * 2 + origin_img.at<uchar>(Point(center.x + symbol * l + 1, center.y - i + 1));
							if (laplacian_2 >= threshold && abs(grad_x)> 50 )
							{
								side_points.push_back(Point(center.x + symbol * l, center.y - i));
								reach_sidebound_again = true;
								break;
							}
							else
							{
								for (int m = 1; m < 50; m++)
								{

									laplacian_2 = img.at<uchar>(Point(center.x + symbol * l, center.y - i - m));
									if (laplacian_2 >= threshold)
									{
										side_points.push_back(Point2f(center.x + symbol * l, center.y - i - m));
										break;
									}
									else
									{
										inner_points.push_back(Point2f(center.x + symbol * l, center.y - i - m));
									}
								}
							}	
						}
					}
					reach_upperbound = true;
					break;
					

				}
				
				
			}
			while (!reach_leftbound)
			{
				for (int j = 0; j < 150; j++)
				{
					int left_laplacian = 0;
					left_laplacian = img.at<uchar>(Point(center.x - j, center.y - i));
					if (abs(left_laplacian) >= threshold)
					{
						reach_leftbound = true;
						got_left = true;
						side_points.push_back(Point2f(center.x - j, center.y - i));
						break;
					}
					else
					{
						inner_points.push_back(Point2f(center.x - j, center.y - i));
					}
				}
			}
			while (got_left && (!reach_rightbound))
			{
				for (int k = 1; k <150; k++)
				{
					int right_laplacian = 0;
					right_laplacian = img.at<uchar>(Point(center.x + k, center.y - i));
					if (abs(right_laplacian) >= threshold)
					{
						side_points.push_back(Point2f(center.x  + k, center.y-i));
						reach_rightbound = true;
						break;
					}
					else
					{
						inner_points.push_back(Point2f(center.x + k, center.y-i));
					}
					
				}
				
			}
		}
		
	}
	reach_sidebound_again = false;
	reach_leftbound = reach_rightbound = false;
	while (!reach_lowerbound)
	{
		for (int i = 0; i < 150; i++)
		{
			reach_leftbound = reach_rightbound = false;
			got_left = false;
			int lower_laplacian = 0;
			lower_laplacian = img.at<uchar>(Point(center.x, center.y + i));
			if (abs(lower_laplacian) >= threshold)
			{
				reach_lowerbound = true;
				side_points.push_back(Point(center.x, center.y + i));

				double grad_x = origin_img.at<uchar>(Point(center.x - 1, center.y + i - 1))*(-1) + origin_img.at<uchar>(Point(center.x - 1, center.y + i))*(-2) + origin_img.at<uchar>(Point(center.x - 1, center.y + i + 1))*(-1)
					+ origin_img.at<uchar>(Point(center.x + 1, center.y + i - 1)) + origin_img.at<uchar>(Point(center.x + 1, center.y + i)) * 2 + origin_img.at<uchar>(Point(center.x + 1, center.y + i + 1));
				int symbol = 0;
				double laplacian_2;
				if (-5< grad_x && grad_x < 5)
				{
					reach_lowerbound = true;
					break;
				}
				else
				{
					symbol = grad_x > 0 ? 1 : -1;

					while (!reach_sidebound_again)
					{
						for (int l = 1; l < 50; l++)
						{
							reach_upperbound_again = false;
							laplacian_2 = img.at<uchar>(Point(center.x + symbol * l, center.y + i));
							grad_x = origin_img.at<uchar>(Point(center.x + symbol * l - 1, center.y + i - 1))*(-1) + origin_img.at<uchar>(Point(center.x + symbol * l - 1, center.y + i))*(-2) + origin_img.at<uchar>(Point(center.x + symbol * l - 1, center.y + i + 1))*(-1)
								+ origin_img.at<uchar>(Point(center.x + symbol * l + 1, center.y + i - 1)) + origin_img.at<uchar>(Point(center.x + symbol * l + 1, center.y + i)) * 2 + origin_img.at<uchar>(Point(center.x + symbol * l + 1, center.y + i + 1));
							if (laplacian_2 >= threshold && abs(grad_x)>50)
							{
								side_points.push_back(Point(center.x + symbol * l, center.y + i));
								reach_sidebound_again = true;
								break;
							}
							else
							{
								for (int m = 1; m < 50; m++)
								{

									laplacian_2 = img.at<uchar>(Point(center.x + symbol * l, center.y + i - m));
									if (laplacian_2 >= threshold)
									{
										side_points.push_back(Point2f(center.x + symbol * l, center.y + i - m));
										break;
									}
									else
									{
										inner_points.push_back(Point2f(center.x + symbol * l, center.y + i - m));
									}
								}
							}
						}
					}
					reach_upperbound = true;
					break;


				}
			}
			while (!reach_leftbound)
			{
				for (int j = 0; j < 150 ; j++)
				{
			
					int left_laplacian = 0;
					left_laplacian = img.at<uchar>(Point(center.x - j, center.y + i));
					if (abs(left_laplacian) >= threshold)
					{
						reach_leftbound = true;
						got_left = true;
						side_points.push_back(Point2f(center.x - j, center.y + i));
						break;
					}
					else
					{
						inner_points.push_back(Point2f(center.x - j, center.y + i));
					}
				}
			}
			while (got_left && (!reach_rightbound))
			{
				for (int k = 1; k < 150 ; k++)
				{
					int right_laplacian = 0;
					right_laplacian = img.at<uchar>(Point(center.x + k, center.y + i));
					if (abs(right_laplacian) >= threshold)
					{
						side_points.push_back(Point2f(center.x + k, center.y + i));
						reach_rightbound = true;
						break;
					}
					else
					{
						inner_points.push_back(Point2f(center.x + k, center.y + i));
					}

				}

			}
		}

	}
}

void edgeEnhance(cv::Mat& srcImg, cv::Mat& dstImg)
{
	if (!dstImg.empty())
	{
		dstImg.release();
	}
	std::vector<cv::Mat> rgb;

	if (srcImg.channels() == 3)        // rgb image
	{
		cv::split(srcImg, rgb);
	}
	else if (srcImg.channels() == 1)  // gray image
	{
		rgb.push_back(srcImg);
	}
	for (size_t i = 0; i < rgb.size(); i++)
	{
		cv::Mat sharpMat8U;
		cv::Mat sharpMat;
		cv::Mat blurMat;
		cv::GaussianBlur(rgb[i], blurMat, cv::Size(3, 3), 0, 0);
		// 计算拉普拉斯
		cv::Laplacian(blurMat, sharpMat, CV_16S);
		// 转换类型
		sharpMat.convertTo(sharpMat8U, CV_8U);
		cv::add(rgb[i], sharpMat8U, rgb[i]);
	}
	cv::merge(rgb, dstImg);
}
//提出重复点
void remove_repeated_component(vector<Point2f> &points)
{
	vector<Point2f>::iterator it, it1;
	for (it = ++points.begin(); it != points.end();)
	{
		it1 = find(points.begin(), it, *it);
		if (it1 != it)
			it = points.erase(it);
		else
			it++;
	}
}
//通过辐射线得到新的圆心
void get_circle_features_by_rays(const Mat & img, Point2f center, vector<Point2f> &inner_points, vector<Point2f> &side_points,const Mat & origin_img)
{
	double theta=0;
	double resolution = 0.1;
	double reach_bound = 0;
	int threshold = 25;
	int y_coordinate;
	while (theta < 45)
	{

		for (int i = 0; i < 200; i++)
		{
			if (tan(theta*CV_PI/180)*i - int(tan(theta*CV_PI/180)*i) > 0.5)
			{
				y_coordinate = center.y - int(tan(theta*CV_PI / 180)*i) - 1;
			}
			else
			{
				y_coordinate = center.y - int(tan(theta*CV_PI / 180)*i) ;
			}
			reach_bound = img.at<uchar>(Point(center.x + i, y_coordinate));
			if (reach_bound > threshold)
			{
				side_points.push_back(Point2f(center.x + i, y_coordinate));
				
				break;
			}
			else
			{
				inner_points.push_back(Point2f(center.x + i, y_coordinate));
			}

			
		}
		for (int i = 0; i < 200; i++)
		{
			if (tan(theta*CV_PI / 180)*i - int(tan(theta*CV_PI / 180)*i) > 0.5)
			{
				y_coordinate = center.y + int(tan(theta*CV_PI / 180)*i) + 1;
			}
			else
			{
				y_coordinate = center.y + int(tan(theta*CV_PI / 180)*i) ;
			}
			reach_bound = img.at<uchar>(Point(center.x - i, y_coordinate));
			if (reach_bound > threshold)
			{
				side_points.push_back(Point2f(center.x - i, y_coordinate));
				break;
			}
			else
			{
					inner_points.push_back(Point2f(center.x - i, y_coordinate));
			}

			
		}
		theta += resolution;
	}
	theta = 45;
	int x_coordinate = 0;
	while (theta >=45 && theta <135)
	{

		for (int i = 0; i < 200; i++)
		{
			if (tan((90-theta)*CV_PI / 180)*i - int(tan((90 - theta)*CV_PI / 180)*i) > 0.5)
			{
				x_coordinate = center.x +int(tan((90 - theta)*CV_PI / 180)*i) + 1;
			}
			else
			{
				x_coordinate = center.x + int(tan((90 - theta)*CV_PI / 180)*i);
			}
			reach_bound = img.at<uchar>(Point(x_coordinate, center.y+i));
			if (reach_bound > threshold)
			{
				side_points.push_back(Point2f(x_coordinate, center.y + i));
				break;
			}
			else
			{
				inner_points.push_back(Point2f(x_coordinate, center.y + i));
			}


		}
		for (int i = 0; i < 200; i++)
		{
			if (tan((90 - theta)*CV_PI / 180)*i - int(tan((90 - theta)*CV_PI / 180)*i) > 0.5)
			{
				x_coordinate = center.x  -int(tan((90 - theta)*CV_PI / 180)*i) - 1;
			}
			else
			{
				x_coordinate = center.x - int(tan((90 - theta)*CV_PI / 180)*i);
			}
			reach_bound = img.at<uchar>(Point(x_coordinate, center.y - i));
			if (reach_bound > threshold)
			{
				side_points.push_back(Point2f(x_coordinate, center.y - i));
				
				break;
			}
			else
			{
				inner_points.push_back(Point2f(x_coordinate, center.y - i));
			}
		}
		theta += resolution;
	}
	theta = 135;

	while (theta >= 135 && theta < 180)
	{

		for (int i = 0; i < 200; i++)
		{
			if (tan(theta*CV_PI / 180)*i - int(tan(theta*CV_PI / 180)*i) > 0.5)
			{
				y_coordinate = center.y - int(tan(theta*CV_PI / 180)*i) - 1;
			}
			else
			{
				y_coordinate = center.y - int(tan(theta*CV_PI / 180)*i);
			}
			reach_bound = img.at<uchar>(Point(center.x + i, y_coordinate));
			if (reach_bound > threshold)
			{
				side_points.push_back(Point2f(center.x + i, y_coordinate));
				
				break;
			}
			else
			{
				inner_points.push_back(Point2f(center.x + i, y_coordinate));
			}


		}
		for (int i = 0; i < 200; i++)
		{
			if (tan(theta*CV_PI / 180)*i - int(tan(theta*CV_PI / 180)*i) > 0.5)
			{
				y_coordinate = center.y + int(tan(theta*CV_PI / 180)*i) + 1;
			}
			else
			{
				y_coordinate = center.y + int(tan(theta*CV_PI / 180)*i);
			}
			reach_bound = img.at<uchar>(Point(center.x - i, y_coordinate));
			if (reach_bound > threshold)
			{
				side_points.push_back(Point2f(center.x - i, y_coordinate));
				
				break;
			}
			else
			{
				inner_points.push_back(Point2f(center.x - i, y_coordinate));
			}


		}
		theta += resolution;
	}
}
int main()

{
	//读取图片
	Mat image_left = imread("left\\1-1.bmp");
	Mat image_right = imread("right\\1-1.bmp");


	Mat  gray_img_left, gray_img_right, binary_img_left, binary_img_right;
	image_left.copyTo(gray_img_left);
	image_right.copyTo(gray_img_right);
	//转为灰度图
	cvtColor(gray_img_left, gray_img_left, CV_RGB2GRAY);
	cvtColor(gray_img_right, gray_img_right, CV_RGB2GRAY);
	gray_img_left.copyTo(binary_img_left);
	gray_img_right.copyTo(binary_img_right);
	
	//高斯金字塔下采样
	Mat down_img_left_1, down_img_left_2, down_img_right_1, down_img_right_2;
	pyrDown(gray_img_left, down_img_left_1, Size(gray_img_left.cols / 2, gray_img_left.rows / 2));
	pyrDown(down_img_left_1, down_img_left_2, Size(down_img_left_1.cols / 2, down_img_left_1.rows / 2));
	
	pyrDown(gray_img_right, down_img_right_1, Size(gray_img_right.cols / 2, gray_img_right.rows / 2));
	pyrDown(down_img_right_1, down_img_right_2, Size(down_img_right_1.cols / 2, down_img_right_1.rows / 2));

	//对下采样图片经行二值化处理
	binary_pic(down_img_left_2);
	binary_pic(down_img_right_2);

	//得到粗略的圆心
	Eigen::Vector2d origin_center_left_1, origin_center_left_2, origin_center_right_1, origin_center_right_2;
	vector<Point2f> rough_centers_left, rough_centers_right;
	Mat blob_img_left, blob_img_right;
	image_left.copyTo(blob_img_left);
	image_right.copyTo(blob_img_right);
		rough_centers_left = get_rough_centers_by_blob(gray_img_left, 10, 50, 220, 30, 0, 600*3, 8000 * 3, 0.7, blob_img_left);
		if (rough_centers_left[0].x < rough_centers_left[1].x)
		{
			origin_center_left_1 << rough_centers_left[0].x, rough_centers_left[0].y;
			origin_center_left_2 << rough_centers_left[1].x, rough_centers_left[1].y;
		}
		else
		{
			origin_center_left_1 << rough_centers_left[1].x, rough_centers_left[1].y;
			origin_center_left_2 << rough_centers_left[0].x, rough_centers_left[0].y;
		}
		
		rough_centers_right = get_rough_centers_by_blob(gray_img_right, 10, 50, 220, 30, 0, 600 * 3, 8000 * 3, 0.7, blob_img_right);
		if (rough_centers_right[0].x < rough_centers_right[1].x)
		{
			origin_center_right_1 << rough_centers_right[0].x, rough_centers_right[0].y;
			origin_center_right_2 << rough_centers_right[1].x, rough_centers_right[1].y;
		}
		else
		{
			origin_center_right_1 << rough_centers_right[1].x, rough_centers_right[1].y;
			origin_center_right_2 << rough_centers_right[0].x, rough_centers_right[0].y;
		}


	
	//重新得到圆心

	Eigen::Matrix<double, 5, 2> new_center_left_1 = get_circle_center(origin_center_left_1, gray_img_left);
	Eigen::Matrix<double, 5, 2> new_center_left_2 = get_circle_center(origin_center_left_2, gray_img_left);
	Eigen::Matrix<double, 5, 2> new_center_right_1 = get_circle_center(origin_center_right_1, gray_img_right);
	Eigen::Matrix<double, 5, 2> new_center_right_2 = get_circle_center(origin_center_right_2, gray_img_right);

	//利用cornerSubPix得到亚像素坐标的新圆心
	vector<Point2f> subPix_center_left_1, subPix_center_left_2, subPix_center_right_1, subPix_center_right_2;
	subPix_center_left_1 = get_subPix_center(new_center_left_1, gray_img_left);
	subPix_center_left_2 = get_subPix_center(new_center_left_2, gray_img_left);
	subPix_center_right_1 = get_subPix_center(new_center_right_1, gray_img_right);
	subPix_center_right_2 = get_subPix_center(new_center_right_2, gray_img_right);



	//输出圆心坐标
	//rough centers
	printf("the rough center by bolb of left 1 is : %f, %f \n\t", rough_centers_left[0].x, rough_centers_left[0].y);
	printf("the rough center by bolb of left 2 is : %f, %f \n\t", rough_centers_left[1].x, rough_centers_left[1].y);
	printf("the rough center by bolb of right 1 is : %f, %f \n\t", rough_centers_right[0].x, rough_centers_right[0].y);
	printf("the rough center by bolb of right 2 is : %f, %f \n\t", rough_centers_right[1].x, rough_centers_right[1].y);
	//acurate centers
	printf("the accurate center of left 1 is : %f, %f \n\t", subPix_center_left_1[0].x, subPix_center_left_1[0].y);
	printf("the accurate center of left 2 is : %f, %f \n\t", subPix_center_left_2[0].x, subPix_center_left_2[0].y);
	printf("the accurate center of right 1 is : %f, %f \n\t", subPix_center_right_1[0].x, subPix_center_right_1[0].y);
	printf("the accurate center of right 2 is : %f, %f \n\t", subPix_center_right_2[0].x, subPix_center_right_2[0].y);



	Point2f B_left, B_right, A_left, A_right;
	
	A_left.x = origin_center_left_1(0);
	A_left.y = origin_center_left_1(1);
	A_right.x = origin_center_right_1(0);
	A_right.y = origin_center_right_1(1);

	B_left.x = origin_center_left_2(0);
	B_left.y = origin_center_left_2(1);
	B_right.x = origin_center_right_2(0);
	B_right.y = origin_center_right_2(1);

	Eigen::Vector3d coordinate_A, coordinate_B;
	coordinate_A = get_3d_coordinate(A_left, A_right);
	coordinate_B = get_3d_coordinate(B_left, B_right);
	double length = (coordinate_A - coordinate_B).norm();
	printf("the 3d coordinate of A is x=%f, y=%f, z=%f \n\t",coordinate_A(0)/1000, coordinate_A(1)/1000, coordinate_A(2)/1000);
	printf("the 3d coordinate of B is x=%f, y=%f, z=%f \n\t", coordinate_B(0)/1000, coordinate_B(1)/1000, coordinate_B(2)/1000);
	printf("the length of A and B is %f\n\t", length);


	vector<Point2f> rectangle_left_A(2), rectangle_left_B(2), rectangle_right_A(2), rectangle_right_B(2);

	//得到圆的边缘和内点
	vector<Point2f> side_points_left_A, inner_points_left_A,side_points_left_B, inner_points_left_B, side_points_right_A, inner_points_right_A, side_points_right_B, inner_points_right_B;
	Mat canny_img_left, canny_img_right;
	int th1, th2;
	th1 = 100; th2 = 50;
	Mat smooth_img_left, smooth_img_right;
	GaussianBlur(gray_img_left, smooth_img_left, Size(3, 3), 0, 0);
	GaussianBlur(gray_img_right, smooth_img_right, Size(3, 3), 0, 0);
	//detailEnhance(gray_img_left, canny_img_left);
	Mat enhanced_left,enhance_right;
	edgeEnhance(gray_img_left, enhanced_left);
	edgeEnhance(gray_img_right, enhance_right);
	Canny(enhanced_left, canny_img_left,th1, th2, 3);
	dilate(canny_img_left, canny_img_left, getStructuringElement(MORPH_ELLIPSE, Size(3, 3)));
	
	//gray_img_right.copyTo(canny_img_right);
	//GaussianBlur(canny_img_right, canny_img_right, Size(3, 3), 0, 0);
	Canny(enhance_right, canny_img_right, th1, th2, 3);
	dilate(canny_img_right, canny_img_right, getStructuringElement(MORPH_ELLIPSE, Size(3, 3)));

	get_circle_features_by_rays(canny_img_left,Point2f(origin_center_left_1(0), origin_center_left_1(1)), inner_points_left_A, side_points_left_A, smooth_img_left);
	get_circle_features_by_rays(canny_img_left, Point2f(origin_center_left_2(0), origin_center_left_2(1)), inner_points_left_B, side_points_left_B, smooth_img_left);
	get_circle_features_by_rays(canny_img_right, Point2f(origin_center_right_1(0), origin_center_right_1(1)), inner_points_right_A, side_points_right_A, smooth_img_right);
	get_circle_features_by_rays(canny_img_right, Point2f(origin_center_right_2(0), origin_center_right_2(1)), inner_points_right_B, side_points_right_B, smooth_img_right);


	RotatedRect box = fitEllipse(inner_points_left_A);
	
	printf("/n/t origin size is %d\n", side_points_left_A.size());
	remove_repeated_component(side_points_left_A);
	remove_repeated_component(side_points_left_B);
	remove_repeated_component(side_points_right_A);
	remove_repeated_component(side_points_right_B);
	/*remove_repeated_component(inner_points_left_A);
	remove_repeated_component(inner_points_left_B);
	remove_repeated_component(inner_points_right_A);
	remove_repeated_component(inner_points_right_B);*/
	printf("/n/t after  size is %d\n", side_points_left_A.size());
	Size winSize = Size(5, 5);
	Size zeroZone = Size(-1, -1);
	TermCriteria criteria = TermCriteria(TermCriteria::EPS + TermCriteria::MAX_ITER, 40, 0.001);
	/*cornerSubPix(gray_img_left, side_points_left_A, winSize, zeroZone, criteria);
	cornerSubPix(gray_img_left, side_points_left_B, winSize, zeroZone, criteria);
	cornerSubPix(gray_img_right, side_points_right_A, winSize, zeroZone, criteria);
	cornerSubPix(gray_img_right, side_points_right_B, winSize, zeroZone, criteria);*/
	//画轮廓
	Point2f left_A_by_contour, left_B_by_contour, right_A_by_contour, right_B_by_contour;
	for (int i = 0; i < side_points_left_A.size(); i++)
	{
		Point origin_center(side_points_left_A[i].x, side_points_left_A[i].y);
		circle(image_left, origin_center, 1, Scalar(0, 0, 255), 1, 8, 0);
		left_A_by_contour.x += side_points_left_A[i].x / side_points_left_A.size();
		left_A_by_contour.y += side_points_left_A[i].y / side_points_left_A.size();
		
	}
	for (int i = 0; i < side_points_left_B.size(); i++)
	{
		Point origin_center2(side_points_left_B[i].x, side_points_left_B[i].y);
		circle(image_left, origin_center2, 1, Scalar(0, 0, 255), 1, 8, 0);
		left_B_by_contour.x += side_points_left_B[i].x / side_points_left_B.size();
		left_B_by_contour.y += side_points_left_B[i].y / side_points_left_B.size();
	}


	for (int i = 0; i < side_points_right_A.size(); i++)
	{
		Point origin_center3(side_points_right_A[i].x, side_points_right_A[i].y);
		circle(image_right, origin_center3, 1, Scalar(0, 0, 255), 1, 8, 0);
		right_A_by_contour.x += side_points_right_A[i].x / side_points_right_A.size();
		right_A_by_contour.y += side_points_right_A[i].y / side_points_right_A.size();
	}

	for (int i = 0; i < side_points_right_B.size(); i++)
	{
		Point origin_center4(side_points_right_B[i].x, side_points_right_B[i].y);
		circle(image_right, origin_center4, 1, Scalar(0, 0, 255), 1, 8, 0);
		right_B_by_contour.x += side_points_right_B[i].x / side_points_right_B.size();
		right_B_by_contour.y += side_points_right_B[i].y / side_points_right_B.size();
	}
	Point2f point0;
	float radius0;
	minEnclosingCircle(inner_points_left_A, left_A_by_contour, radius0);
	minEnclosingCircle(inner_points_left_B, left_B_by_contour, radius0);
	minEnclosingCircle(inner_points_right_A, right_A_by_contour, radius0);
	minEnclosingCircle(inner_points_right_B, right_B_by_contour, radius0);
	Eigen::Vector3d coordinate_A2, coordinate_B2;
	coordinate_A2 = get_3d_coordinate(left_A_by_contour, right_A_by_contour);
	coordinate_B2 = get_3d_coordinate(left_B_by_contour, right_B_by_contour);
	double length2 = (coordinate_A2 - coordinate_B2).norm();
	printf("the 3d coordinate of A by contours is x=%f, y=%f, z=%f \n\t", coordinate_A2(0), coordinate_A2(1), coordinate_A2(2));
	printf("the 3d coordinate of B by contour is x=%f, y=%f, z=%f \n\t", coordinate_B2(0), coordinate_B2(1), coordinate_B2(2));
	printf("the length of by contour A and B is %f\n\t", length2);

	//画内点 
	Point2f left_A_by_inner, left_B_by_inner, right_A_by_inner, right_B_by_inner;
	for (int i = 0; i < inner_points_left_A.size(); i++)
	{
		Point origin_center(inner_points_left_A[i].x, inner_points_left_A[i].y);
		//circle(image_left, origin_center, 2, Scalar(255, 255, 0), 1, 8, 0);
		left_A_by_inner.x += inner_points_left_A[i].x / inner_points_left_A.size();
		left_A_by_inner.y += inner_points_left_A[i].y / inner_points_left_A.size();
	}
	for (int i = 0; i < inner_points_left_B.size(); i++)
	{
		Point origin_center2(inner_points_left_B[i].x, inner_points_left_B[i].y);
		//circle(image_left, origin_center2, 2, Scalar(255, 255, 0), 1, 8, 0);
		left_B_by_inner.x += inner_points_left_B[i].x / inner_points_left_B.size();
		left_B_by_inner.y += inner_points_left_B[i].y / inner_points_left_B.size();
	}


	for (int i = 0; i < inner_points_right_A.size(); i++)
	{
		Point origin_center3(inner_points_right_A[i].x, inner_points_right_A[i].y);
		//circle(image_right, origin_center3, 2, Scalar(255, 255, 0), 1, 8, 0);
		right_A_by_inner.x += inner_points_right_A[i].x / inner_points_right_A.size();
		right_A_by_inner.y += inner_points_right_A[i].y / inner_points_right_A.size();
	}

	for (int i = 0; i < inner_points_right_B.size(); i++)
	{
		Point origin_center4(inner_points_right_B[i].x, inner_points_right_B[i].y);
		//circle(image_right, origin_center4, 2, Scalar(255, 255, 0), 1, 8, 0);
		right_B_by_inner.x += inner_points_right_B[i].x / inner_points_right_B.size();
		right_B_by_inner.y += inner_points_right_B[i].y / inner_points_right_B.size();
	}
	Eigen::Vector3d coordinate_A3, coordinate_B3;
	coordinate_A3 = get_3d_coordinate(left_A_by_inner, right_A_by_inner);
	coordinate_B3 = get_3d_coordinate(left_B_by_inner, right_B_by_inner);
	double length3 = (coordinate_A3 - coordinate_B3).norm();
	printf("the 3d coordinate of A by inner is x=%f, y=%f, z=%f \n\t", coordinate_A3(0), coordinate_A3(1), coordinate_A3(2));
	printf("the 3d coordinate of B by inner is x=%f, y=%f, z=%f \n\t", coordinate_B3(0), coordinate_B3(1), coordinate_B3(2));
	printf("the length of by inner A and B is %f\n\t", length3);



	//运用椭圆拟合的方法得到新的内点
	/*Point2f center_by_fitellipse_left_A, center_by_fitellipse_left_B, center_by_fitellipse_right_A, center_by_fitellipse_right_B;
	center_by_fitellipse_left_A = fitEllipse(inner_points_left_A).center;
	center_by_fitellipse_left_B = fitEllipse(inner_points_left_B).center;
	center_by_fitellipse_right_A = fitEllipse(inner_points_right_A).center;
	center_by_fitellipse_right_B = fitEllipse(inner_points_right_B).center;
	Eigen::Vector3d coordinate_A4, coordinate_B4;
	coordinate_A4 = get_3d_coordinate(center_by_fitellipse_left_A, center_by_fitellipse_right_A);
	coordinate_B4 = get_3d_coordinate(center_by_fitellipse_left_B, center_by_fitellipse_right_B);
	double length4 = (coordinate_A4 - coordinate_B4).norm();
	printf("the 3d coordinate of A by fitellipse is x=%f, y=%f, z=%f \n\t", coordinate_A4(0), coordinate_A4(1), coordinate_A4(2));
	printf("the 3d coordinate of B by fitellipse is x=%f, y=%f, z=%f \n\t", coordinate_B4(0), coordinate_B4(1), coordinate_B4(2));
	printf("the length of by fitellipse A and B is %f\n\t", length4);*/
   

	

	//ellipse(image_left, box, Scalar(0, 0, 255), 3, 8);
	//circle(image_left, point0, radius0, Scalar(0, 0, 255), 3);
	namedWindow("left0", WINDOW_NORMAL);
	imshow("left0", blob_img_left);
	//imwrite("D:\\CvProject3\\blob_img\\left_11.jpg", blob_img_left);

	namedWindow("right0", WINDOW_NORMAL);
	imshow("right0", blob_img_right);
	//imwrite("D:\\CvProject3\\blob_img\\right_11.jpg", blob_img_right);

	namedWindow("left", WINDOW_NORMAL);
	imshow("left", canny_img_left);
	//imwrite("D:\\CvProject3\\canny_img\\left_11.jpg", canny_img_left);

	namedWindow("right", WINDOW_NORMAL);
	imshow("right", canny_img_right);
	//imwrite("D:\\CvProject3\\canny_img\\right_11.jpg", canny_img_right);

	namedWindow("left2", WINDOW_NORMAL);
	imshow("left2", image_left);
	//imwrite("D:\\CvProject3\\only_find_circle_img\\left_11.jpg", image_left);

	namedWindow("right2", WINDOW_NORMAL);
	imshow("right2", image_right);
	//imwrite("D:\\CvProject3\\only_find_circle_img\\right_11.jpg", image_right);

	waitKey(0);
	return 0;

}