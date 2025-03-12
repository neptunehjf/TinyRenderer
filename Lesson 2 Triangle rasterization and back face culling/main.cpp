#include "tgaimage.h"
#include "geometry.h"
#include "model.h"
#include "math.h"

using namespace std;

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);
const TGAColor green = TGAColor(0, 255, 0, 255);
Model* model = NULL;
const int width = 800;
const int height = 800;

// ���ߴ�ֱ������Ļ��
Vec3f light_dir(0, 0, -1);



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

// ��ɨ�跨�����������
// 1 �������η�Ϊ����������A,B��ע��B��Ϊ����������
// 2 ����y����ı仯�����Լ����A.x B.x
// 3 ���ݴ�A.x B.x���д������һ���
void triangle_line_scan(Vec2i t0, Vec2i t1, Vec2i t2, TGAImage& image, TGAColor color)
{
	// �˻������β�����
	if (t0.x == t1.x && t0.x == t2.x || t0.y == t1.y && t0.y == t2.y) return;

	// ð������ʹt0 t1 t2��y���С�����˳������
	if (t0.y > t1.y) swap(t0, t1);
	if (t0.y > t2.y) swap(t0, t2);
	if (t1.y > t2.y) swap(t1, t2);

	// ��ɨ������
	int total_line = t2.y - t0.y;

	// ����ɨ��
	for (int i = 0; i <= total_line; i++)
	{
		bool upper_half = i > t1.y - t0.y || t0.y == t1.y;	// �Ƿ���B�߽���ϰ벿��
		int segment_line = upper_half ? (t2.y - t1.y) : (t1.y - t0.y);
		float alpha = (float)i / total_line;
		float beta = (float)(i - (upper_half ? (t1.y - t0.y) : 0)) / segment_line;
		Vec2i A = t0 + (t2 - t0) * alpha;
		Vec2i B = upper_half ? (t1 + (t2 - t1) * beta) : (t0 + (t1 - t0) * beta);
		if (A.x > B.x) swap(A, B); // �߽�A����B����
		for (int j = A.x; j <= B.x; j++)
			image.set(j, t0.y + i, color);
	}
}

// ����������ϵ��
// �ο�Referrence/barycentric coordinates.jpg
Vec3f barycentric(Vec2i* pts, Vec2i P) 
{
	// ������������t
	Vec3f t = Vec3f(pts[2].x - pts[0].x, pts[1].x - pts[0].x, pts[0].x - P.x) ^
		      Vec3f(pts[2].y - pts[0].y, pts[1].y - pts[0].y, pts[0].y - P.y);
	// ��Ϊt.z = 2 * �����������С�ڵ���0˵�����˻������Ρ�
	// ����Ϊ����ԭ��ȡһ����ֵ1
	// ������˻������Σ��򷵻ش���ֵ����������ϵ������Ϊ��������ϵ��С��0��ʱ��˵���㲻���������ڲ�������ʱ����
	if (abs(t.z) < 1) return Vec3f(-1, -1, -1);

	// ��������t������������ϵ��
	return Vec3f(1.0 - (t.x + t.y) / t.z, t.y / t.z, t.x / t.z);
}

// barycentric��ʽ�����������
// barycentric��ʽ��Ҫ��bbox�������������Ч��̫��
void triangle_barycentric(Vec2i* pts, TGAImage& image, TGAColor color)
{
	// ��ʼbboxΪ��
	Vec2i bboxmin(image.get_width() - 1, image.get_height() - 1);
	Vec2i bboxmax(0, 0);

	Vec2i clamp(image.get_width() - 1, image.get_height() - 1);

	// ����bbox
	for (int i = 0; i < 3; i++) 
	{
		bboxmin.x = max(0, min(bboxmin.x, pts[i].x));
		bboxmin.y = max(0, min(bboxmin.y, pts[i].y));

		bboxmax.x = min(clamp.x, max(bboxmax.x, pts[i].x));
		bboxmax.y = min(clamp.y, max(bboxmax.y, pts[i].y));
	}

	// ����bbox�ڵ��������أ�barycentric��ʽ�жϣ�������������ڲ�����Ⱦ��������Ⱦ
	Vec2i P;
	for (P.x = bboxmin.x; P.x <= bboxmax.x; P.x++) 
	{
		for (P.y = bboxmin.y; P.y <= bboxmax.y; P.y++) 
		{
			Vec3f bc_screen = barycentric(pts, P);
			if (bc_screen.x < 0 || bc_screen.y < 0 || bc_screen.z < 0) continue;
			image.set(P.x, P.y, color);
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

	Vec2i t0[3] = { Vec2i(10, 70),   Vec2i(50, 160),  Vec2i(70, 80) };
	Vec2i t1[3] = { Vec2i(180, 50),  Vec2i(150, 1),   Vec2i(70, 180) };
	Vec2i t2[3] = { Vec2i(180, 150), Vec2i(120, 160), Vec2i(130, 180) };

	for (int i = 0; i < model->nfaces(); i++) 
	{
		vector<int> face = model->face(i);
		Vec2i screen_coords[3];
		Vec3f world_coords[3];
		for (int j = 0; j < 3; j++) 
		{
			Vec3f v = model->vert(face[j]);
			screen_coords[j] = Vec2i(int((v.x + 1.) * width / 2. + .5), int((v.y + 1.) * height / 2. + .5));
			world_coords[j] = v;
		}
		// ͨ�������α��淨�ߺ͹��߼нǼ������ǿ��
		// intensity < 0 ˵���������������ڲ�����ʱ�����ƣ���һ�ּ򵥵����޳������������ڷ��߷��򣬲�����ȫ�޳�������
		Vec3f n = (world_coords[2] - world_coords[0]) ^ (world_coords[1] - world_coords[0]);
		n.normalize();
		float intensity = n * light_dir;
		if (intensity > 0) 
		{
			triangle_barycentric(screen_coords, image, TGAColor(intensity * 255, intensity * 255, intensity * 255, 255));
		}
	}

	image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
	image.write_tga_file("head_triangles_light.tga");
	delete model;

	return 0;
}

