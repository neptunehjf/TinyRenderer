#include "tgaimage.h"

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);

using namespace std;

// test1
//void line(int x0, int y0, int x1, int y1, TGAImage& image, TGAColor color)
//{
//  // ��float����
//	for (float t = 0.0; t < 1.0; t += 0.01)
//	{
//		int x = x0 + (x1 - x0) * t;
//		int y = y0 + (y1 - y0) * t;
//		image.set(x, y, color);
//	}
//}

// test2
//void line(int x0, int y0, int x1, int y1, TGAImage& image, TGAColor color)
//{
//	// ����Ҫ��float������int�������죬�˴���x���������ز���
//	for (int x = x0; x <= x1; x++)
//	{
//		float t = (float)(x - x0) / (x1 - x0);
//		int y = y0 + (y1 - y0) * t;
//		image.set(x, y, color);
//	}
//}

// test3
// ���ܷ������ performance1.png
//void line(int x0, int y0, int x1, int y1, TGAImage& image, TGAColor color)
//{
//	// 1 ������������˳���Ӱ��
//	if (x0 > x1)
//	{
//		swap(x0, x1);
//		swap(y0, y1);
//	}
//	// 2 ����б�ʵ�Ӱ��
//	// ��Ϊ����Ƶ������x����ı仯Ƶ�ʾ����ģ�y������仯Ƶ�ʴ���x����ı仯Ƶ�ʵ�ʱ�򣬲���Ƶ�ʿ��ܸ�����y��ı仯Ƶ�ʣ�����ʧ��
//	int dx = abs(x1 - x0);
//	int dy = abs(y1 - y0);
//	if (dx < dy)
//	{
//		// ת�ã�ʼ���ڸ�Ƶ�ķ����ϲ���
//		swap(x0, y0);
//		swap(x1, y1);
//	}
//
//	for (int x = x0; x <= x1; x++)
//	{
//		float t = (float)(x - x0) / (x1 - x0);
//		int y = y0 + (y1 - y0) * t;
//		if (dx < dy)
//			image.set(y, x, color); // ת��ֻ��Ϊ�˲����������ɫ��ʱ�򻹵�ת�û�ȥ
//		else
//			image.set(x, y, color);
//	}
//}

// test4 
// Bresenham�㷨��ֱ�ߣ���test3������в�ͬ
// �ѳ�����ѭ�����ó����ˣ��������ٶȸ���
// ���ܷ������ performance2.png �� performance1.png�Ա�line��ִ��ʱ������2��
// ��Ȼ��Щ΢�Ż�����û�н̳�����ô���ԣ������Ǳ�������һ��(MSVC vs GCC)��
// �ο�Referrence/Bresenham Algorithm.jpg
//void line(int x0, int y0, int x1, int y1, TGAImage& image, TGAColor color)
//{
//	// 1 ������������˳���Ӱ��
//	if (x0 > x1)
//	{
//		swap(x0, x1);
//		swap(y0, y1);
//	}
//	// 2 ����б�ʵ�Ӱ��
//	// ��Ϊ����Ƶ������x����ı仯Ƶ�ʾ����ģ�y������仯Ƶ�ʴ���x����ı仯Ƶ�ʵ�ʱ�򣬲���Ƶ�ʿ��ܸ�����y��ı仯Ƶ�ʣ�����ʧ��
//	bool steep = false;
//	if (abs(x1 - x0) < abs(y1 - y0))
//		steep = true;
//
//	if (steep)
//	{
//		// ת�ã�ʼ���ڸ�Ƶ�ķ����ϲ���
//		swap(x0, y0);
//		swap(x1, y1);
//	}
//
//	// ����ת�ã�Ҫ��ת��֮���ټ�������
//	int dx = abs(x1 - x0);
//	int dy = abs(y1 - y0);
//
//	float de = (float)dy / dx;
//	float e = 0.0;
//	int y = y0;
//
//	for (int x = x0; x <= x1; x++)
//	{
//		if (steep)
//			image.set(y, x, color); // ת��ֻ��Ϊ�˲����������ɫ��ʱ�򻹵�ת�û�ȥ
//		else
//			image.set(x, y, color);
//
//		e += de;
//		if (e > 0.5)
//		{
//			y += (y0 < y1 ? 1 : -1); // ����y�����췽���1���1
//			e -= 1; //��Ϊy�������п��ܲ���1��������㲹����������y������0.6>0.5������y��1��
//			        //ʵ�ʶ�������1 - 0.6 = 0.4�����Բ�������-0.4��Ҫ�ӵ���һ�ε�����������
//		}
//	}
//}

// test5
// Bresenham�㷨��ֱ�ߣ���test3������в�ͬ
// �ѳ�����ѭ�����ó����ˣ�����������float���㡣�������ٶȸ���
// ���ܷ������ performance3.png �� performance1.png�Ա�line��ִ��ʱ������2��
// ��Ȼ��Щ΢�Ż�����û�н̳�����ô���ԣ������Ǳ�������һ��(MSVC vs GCC)��
// �ο�Referrence/Bresenham Algorithm.jpg
void line(int x0, int y0, int x1, int y1, TGAImage& image,const TGAColor& color)
{
	// 1 ������������˳���Ӱ��
	if (x0 > x1)
	{
		swap(x0, x1);
		swap(y0, y1);
	}
	// 2 ����б�ʵ�Ӱ��
	// ��Ϊ����Ƶ������x����ı仯Ƶ�ʾ����ģ�y������仯Ƶ�ʴ���x����ı仯Ƶ�ʵ�ʱ�򣬲���Ƶ�ʿ��ܸ�����y��ı仯Ƶ�ʣ�����ʧ��
	bool steep = false;
	if (abs(x1 - x0) < abs(y1 - y0))
		steep = true;

	if (steep)
	{
		// ת�ã�ʼ���ڸ�Ƶ�ķ����ϲ���
		swap(x0, y0);
		swap(x1, y1);
	}

	// ����ת�ã�Ҫ��ת��֮���ټ�������
	int dx = abs(x1 - x0);
	int dy = abs(y1 - y0);

	int de = 2 * dy;
	int e = 0.0;
	int y = y0;

	for (int x = x0; x <= x1; x++)
	{
		if (steep)
			image.set(y, x, color); // ת��ֻ��Ϊ�˲����������ɫ��ʱ�򻹵�ת�û�ȥ
		else
			image.set(x, y, color);

		e += de;
		
		if (e > dx)
		{
			y += (y0 < y1 ? 1 : -1); // ����y�����췽���1���1
			e -= 2 * dx; //��Ϊy�������п��ܲ���1��������㲹����������y������0.6>0.5������y��1��
			//ʵ�ʶ�������1 - 0.6 = 0.4�����Բ�������-0.4��Ҫ�ӵ���һ�ε�����������
		}
	}
}

int main(int argc, char** argv) 
{
	TGAImage image(100, 100, TGAImage::RGB);

	for (int i = 0; i < 1000000; i++) // for ���ܷ���
	{
		line(13, 20, 80, 40, image, white);
		line(20, 13, 40, 80, image, red);
		line(80, 40, 13, 20, image, red);
	}

	image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
	image.write_tga_file("output.tga");
	return 0;
}

