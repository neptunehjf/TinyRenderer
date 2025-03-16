#include "tgaimage.h"
#include "geometry.h"
#include "model.h"
#include <math.h>
#include <limits.h>


using namespace std;

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);
const TGAColor green = TGAColor(0, 255, 0, 255);

const int width = 800;
const int height = 800;

// ���ߴ�ֱ������Ļ��
Vec3f light_dir(0, 0, -1);

// Bresenham�㷨��ֱ��
// �ѳ�����ѭ�����ó����ˣ�����������float���㡣�������ٶȸ���
// ��Ȼ��Щ΢�Ż�����û�н̳�����ô���ԣ������Ǳ�������һ��(MSVC vs GCC)��
// �ο�Referrence/Bresenham Algorithm.jpg
void line(int x0, int y0, int x1, int y1, TGAImage& image, const TGAColor& color)
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

// ����������ϵ��
// �ο�Referrence/barycentric coordinates.jpg
Vec3f barycentric(Vec3f* pts, Vec3f P) 
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
void triangle(Vec3f* verts, Vec2f* texs, float* zbuffer, TGAImage& texture, TGAImage& image, TGAColor color)
{
	// ��ʼbboxΪ��
	Vec2f bboxmin(numeric_limits<float>::max(), numeric_limits<float>::max());
	Vec2f bboxmax(-numeric_limits<float>::max(), -numeric_limits<float>::max());

	Vec2f clamp(image.get_width() - 1, image.get_height() - 1);

	// ����bbox
	for (int i = 0; i < 3; i++) 
	{
		bboxmin.x = max(0.0f, min(bboxmin.x, verts[i].x));
		bboxmin.y = max(0.0f, min(bboxmin.y, verts[i].y));

		bboxmax.x = min(clamp.x, max(bboxmax.x, verts[i].x));
		bboxmax.y = min(clamp.y, max(bboxmax.y, verts[i].y));
	}

	// ����bbox�ڵ��������أ�barycentric��ʽ�жϣ�������������ڲ�����Ⱦ��������Ⱦ
	Vec3f P;
	Vec2f tex_uv;
	Vec2i tex_coord;

	for (P.x = bboxmin.x; P.x <= bboxmax.x; P.x++) 
	{
		for (P.y = bboxmin.y; P.y <= bboxmax.y; P.y++) 
		{
			Vec3f bc_screen = barycentric(verts, P); // ��������ϵ��
			if (bc_screen.x < 0 || bc_screen.y < 0 || bc_screen.z < 0)
				continue;
			// �����������꣬�ò�ֵ���������������һ�����ص����z
			P.z = bc_screen.x * verts[0].z + bc_screen.y * verts[1].z + bc_screen.z * verts[2].z;
			// �����������꣬�ò�ֵ���������������һ�����ص�UV����
			tex_uv.x = bc_screen.x * texs[0].x + bc_screen.y * texs[1].x + bc_screen.z * texs[2].x;
			tex_uv.y = bc_screen.x * texs[0].y + bc_screen.y * texs[1].y + bc_screen.z * texs[2].y;
			
			// UVӳ��
			tex_coord.x = tex_uv.x * texture.get_width();
			tex_coord.y = tex_uv.y * texture.get_height();

			// �����ǰ���ص����ֵ����zbuffer������Ƶ�ǰ���ز�����zbuffer
			if (zbuffer[int(P.x + P.y * width)] < P.z)
			{
				zbuffer[int(P.x + P.y * width)] = P.z;
				image.set(P.x, P.y, texture.get(tex_coord.x, tex_coord.y));
			}	
		}
	}
}

int main(int argc, char** argv) 
{
	Model* model = new Model("obj/african_head.obj");

	TGAImage head_texture(1024, 1024, TGAImage::RGB);
	head_texture.read_tga_file("obj/african_head_diffuse.tga");
	head_texture.flip_vertically();

	//output
	TGAImage image(800, 800, TGAImage::RGB);

	/**************** debug ***************/ 
	//for (int i = 0; i < head_texture.get_height(); i++)
	//	for (int j = 0; j < head_texture.get_width(); j++)
	//	{
	//		image.set(i, j, head_texture.get(i, j));
	//	}

	////image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
	//image.write_tga_file("texture_test.tga");

	//return 0;
	/**************** debug ***************/

	float* zbuffer = new float[height * width];
	for (int i = 0; i < width * height; i++)
		zbuffer[i] = -numeric_limits<float>::max();

	//for (int i = 0; i < model->nfaces(); i++) 
	//{
	//	vector<int> face = model->face(i);
	//	vector<int> face_uv = model->face_uv(i);
	//	Vec3f screen_coords[3];
	//	Vec3f world_coords[3];
	//	Vec2f texture_coords[3];
	//	for (int j = 0; j < 3; j++) 
	//	{
	//		// x[-1,1] y[-1,1] z[-1,1]
	//		Vec3f v = model->vert(face[j]);
	//		// x[0, width] y[0, height] z[-1,1]
	//		// +0.5��������
	//		screen_coords[j] = Vec3f(int((v.x + 1.) * width / 2. + .5), int((v.y + 1.) * height / 2. + .5), v.z);
	//		world_coords[j] = v;
	//		texture_coords[j] = model->uv(face_uv[j]);
	//	}
	//	// ͨ�������α��淨�ߺ͹��߼нǼ������ǿ��
	//	// intensity < 0 ˵���������������ڲ�����ʱ�����ƣ���һ�ּ򵥵����޳������������ڷ��߷��򣬲�����ȫ�޳�������
	//	Vec3f n = (world_coords[2] - world_coords[0]) ^ (world_coords[1] - world_coords[0]);
	//	n.normalize();
	//	float intensity = n * light_dir;
	//	if (intensity > 0) 
	//	{
	//		triangle(screen_coords, texture_coords, zbuffer, head_texture, image, TGAColor());
	//	}
	//}

	image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
	image.write_tga_file("head_with_texture.tga");
	delete model;
	delete[] zbuffer;

	return 0;
}

