#include "tgaimage.h"
#include "geometry.h"
#include "model.h"

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);
Model* model = NULL;
const int width = 800;
const int height = 800;

using namespace std;

// Bresenham�㷨��ֱ��
// �ѳ�����ѭ�����ó����ˣ�����������float���㡣�������ٶȸ���
// ��Ȼ��Щ΢�Ż�����û�н̳�����ô���ԣ������Ǳ�������һ��(MSVC vs GCC)��
// �ο�Referrence/Bresenham Algorithm.jpg
void line(int x0, int y0, int x1, int y1, TGAImage& image,const TGAColor& color)
{

	// 1 ����б�ʵ�Ӱ��
	// ��Ϊ����Ƶ������x����ı仯Ƶ�ʾ����ģ�y������仯Ƶ�ʴ���x����ı仯Ƶ�ʵ�ʱ�򣬲���Ƶ�ʿ��ܸ�����y��ı仯Ƶ�ʣ�����ʧ��
	bool steep = false;
	if (abs(x1 - x0) < abs(y1 - y0))
	{
		steep = true;
		// ת�ã�ʼ���ڸ�Ƶ�ķ����ϲ���
		swap(x0, y0);
		swap(x1, y1);
	}

	// 2 ������������˳���Ӱ��
	if (x0 > x1)
	{
		swap(x0, x1);
		swap(y0, y1);
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
	if (2 == argc) 
	{
		model = new Model(argv[1]);
	}
	else 
	{
		model = new Model("obj/african_head.obj");
	}

	TGAImage image(width, height, TGAImage::RGB);

	for (int i = 0; i < model->nfaces(); i++) 
	{
		std::vector<int> face = model->face(i);
		for (int j = 0; j < 3; j++) 
		{
			Vec3f v0 = model->vert(face[j]);
			Vec3f v1 = model->vert(face[(j + 1) % 3]);
			int x0 = (v0.x + 1.) * width / 2.;
			int y0 = (v0.y + 1.) * height / 2.;
			int x1 = (v1.x + 1.) * width / 2.;
			int y1 = (v1.y + 1.) * height / 2.;
			line(x0, y0, x1, y1, image, white);
		}
	}

	image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
	image.write_tga_file("output.tga");
	delete model;

	return 0;
}

