#include <vector>
#include <cmath>
#include <limits>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"


using namespace std;

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red = TGAColor(255, 0, 0, 255);
const TGAColor green = TGAColor(0, 255, 0, 255);
const TGAColor blue = TGAColor(0, 0, 255, 255);
const TGAColor yellow = TGAColor(255, 255, 0, 255);

const int width = 800;
const int height = 800;
const int depth = 255;

// ���ߴ�ֱ������Ļ��
Vec3f light_dir(0, 0, -1);

// Bresenham�㷨��ֱ��
// �ѳ�����ѭ�����ó����ˣ�����������float���㡣�������ٶȸ���
// ��Ȼ��Щ΢�Ż�����û�н̳�����ô���ԣ������Ǳ�������һ��(MSVC vs GCC)��
// �ο�Referrence/Bresenham Algorithm.jpg
void line2D(int x0, int y0, int x1, int y1, TGAImage& image, const TGAColor& color)
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

void line(Vec3i p0, Vec3i p1, TGAImage& image, TGAColor color) 
{
	bool steep = false;
	if (std::abs(p0.x - p1.x) < std::abs(p0.y - p1.y)) {
		std::swap(p0.x, p0.y);
		std::swap(p1.x, p1.y);
		steep = true;
	}
	if (p0.x > p1.x) {
		std::swap(p0, p1);
	}

	for (int x = p0.x; x <= p1.x; x++) {
		float t = (x - p0.x) / (float)(p1.x - p0.x);
		int y = p0.y * (1. - t) + p1.y * t + .5;
		if (steep) {
			image.set(y, x, color);
		}
		else {
			image.set(x, y, color);
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

// ����任������Referrence/Transformation.jpg
/******************* ����任 *******************/
// ����
Matrix scale(Vec3f t)
{
	Matrix S = Matrix::identity(4);
	S[0][0] = t[0];
	S[1][1] = t[1];
	S[2][2] = t[2];

	return S;
}

// �б䣬3D�Ļ������е㸴�ӣ�����Ϊ����ʵ���ֻʵ��2D����
Matrix shear2D(Vec2f t)
{
	Matrix SH = Matrix::identity(3);
	SH[0][1] = t[0];
	SH[1][0] = t[1];

	return SH;
}

// ��ת�����Կ�����x,y,z������ת�ĵ���
// ��ǰ�����cosangle sinangleЧ�ʸ���
Matrix rotation_x(float cosangle, float sinangle)
{
	Matrix R = Matrix::identity(4);

	R[1][1] = R[2][2] = cosangle;
	R[1][2] = -sinangle;
	R[2][1] = sinangle;

	return R;
}

Matrix rotation_y(float cosangle, float sinangle) 
{
	Matrix R = Matrix::identity(4);
	R[0][0] = R[2][2] = cosangle;
	R[0][2] = sinangle;
	R[2][0] = -sinangle;

	return R;
}

Matrix rotation_z(float cosangle, float sinangle) 
{
	Matrix R = Matrix::identity(4);
	R[0][0] = R[1][1] = cosangle;
	R[0][1] = -sinangle;
	R[1][0] = sinangle;

	return R;
}

// λ��
Matrix translate(Vec3f t)
{
	Matrix T = Matrix::identity(4);
	T[3][0] = t[0];
	T[3][1] = t[1];
	T[3][2] = t[2];

	return T;
}
/******************* ����任 *******************/

int main(int argc, char** argv) 
{
	//Model* model = new Model("obj/african_head.obj");

	//TGAImage head_texture(1024, 1024, TGAImage::RGB);
	//head_texture.read_tga_file("obj/african_head_diffuse.tga");
	//head_texture.flip_vertically();

	//output
	TGAImage image(width, height, TGAImage::RGB);

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

	image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
	image.write_tga_file("output.tga");
	//delete model;
	return 0;
}

