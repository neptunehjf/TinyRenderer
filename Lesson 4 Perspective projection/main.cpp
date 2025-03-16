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

// 光线垂直射入屏幕里
Vec3f light_dir(0, 0, -1);

// Bresenham算法画直线
// 把除法从循环里拿出来了，并且消除了float计算。理论上速度更快
// 虽然有些微优化，并没有教程中那么明显，可能是编译器不一样(MSVC vs GCC)？
// 参考Referrence/Bresenham Algorithm.jpg
void line(int x0, int y0, int x1, int y1, TGAImage& image, const TGAColor& color)
{

	// 1 消除斜率的影响
	// 因为采样频率是由x方向的变化频率决定的，y当方向变化频率大于x方向的变化频率的时候，采样频率可能跟不上y轴的变化频率，导致失真
	bool steep = false;
	if (abs(x1 - x0) < abs(y1 - y0))
	{
		steep = true;
		// 转置，始终在高频的方向上采样
		swap(x0, y0);
		swap(x1, y1);
	}

	// 2 消除参数输入顺序的影响
	if (x0 > x1)
	{
		swap(x0, x1);
		swap(y0, y1);
	}

	// 如有转置，要在转置之后再计算增量
	int dx = abs(x1 - x0);
	int dy = abs(y1 - y0);

	int de = 2 * dy;
	int e = 0.0;
	int y = y0;

	for (int x = x0; x <= x1; x++)
	{
		if (steep)
			image.set(y, x, color); // 转置只是为了采样，输出颜色的时候还得转置回去
		else
			image.set(x, y, color);

		e += de;

		if (e > dx)
		{
			y += (y0 < y1 ? 1 : -1); // 根据y的沿伸方向加1或减1
			e -= 2 * dx; //因为y的增量有可能不到1，这里计算补正量。比如y增量是0.6>0.5，于是y加1，
			//实际多增加了1 - 0.6 = 0.4，所以补正量是-0.4，要加到下一次的增量计算里
		}
	}
}

// 求重心坐标系数
// 参考Referrence/barycentric coordinates.jpg
Vec3f barycentric(Vec3f* pts, Vec3f P) 
{
	// 重心坐标向量t
	Vec3f t = Vec3f(pts[2].x - pts[0].x, pts[1].x - pts[0].x, pts[0].x - P.x) ^
		      Vec3f(pts[2].y - pts[0].y, pts[1].y - pts[0].y, pts[0].y - P.y);
	// 因为t.z = 2 * 三角形面积，小于等于0说明是退化三角形。
	// 又因为精度原因，取一个阈值1
	// 如果是退化三角形，则返回带负值的重心坐标系数，因为重心坐标系数小于0的时候说明点不在三角形内部，绘制时舍弃
	if (abs(t.z) < 1) return Vec3f(-1, -1, -1);

	// 根据向量t返回重心坐标系数
	return Vec3f(1.0 - (t.x + t.y) / t.z, t.y / t.z, t.x / t.z);
}

// barycentric方式画填充三角形
// barycentric方式需要和bbox结合起来，否则效率太低
void triangle(Vec3f* verts, Vec2f* texs, float* zbuffer, TGAImage& texture, TGAImage& image, TGAColor color)
{
	// 初始bbox为空
	Vec2f bboxmin(numeric_limits<float>::max(), numeric_limits<float>::max());
	Vec2f bboxmax(-numeric_limits<float>::max(), -numeric_limits<float>::max());

	Vec2f clamp(image.get_width() - 1, image.get_height() - 1);

	// 计算bbox
	for (int i = 0; i < 3; i++) 
	{
		bboxmin.x = max(0.0f, min(bboxmin.x, verts[i].x));
		bboxmin.y = max(0.0f, min(bboxmin.y, verts[i].y));

		bboxmax.x = min(clamp.x, max(bboxmax.x, verts[i].x));
		bboxmax.y = min(clamp.y, max(bboxmax.y, verts[i].y));
	}

	// 遍历bbox内的所有像素，barycentric方式判断，如果在三角形内部就渲染，否则不渲染
	Vec3f P;
	Vec2f tex_uv;
	Vec2i tex_coord;

	for (P.x = bboxmin.x; P.x <= bboxmax.x; P.x++) 
	{
		for (P.y = bboxmin.y; P.y <= bboxmax.y; P.y++) 
		{
			Vec3f bc_screen = barycentric(verts, P); // 重心坐标系数
			if (bc_screen.x < 0 || bc_screen.y < 0 || bc_screen.z < 0)
				continue;
			// 根据重心坐标，用插值法算出三角形任意一个像素的深度z
			P.z = bc_screen.x * verts[0].z + bc_screen.y * verts[1].z + bc_screen.z * verts[2].z;
			// 根据重心坐标，用插值法算出三角形任意一个像素的UV坐标
			tex_uv.x = bc_screen.x * texs[0].x + bc_screen.y * texs[1].x + bc_screen.z * texs[2].x;
			tex_uv.y = bc_screen.x * texs[0].y + bc_screen.y * texs[1].y + bc_screen.z * texs[2].y;
			
			// UV映射
			tex_coord.x = tex_uv.x * texture.get_width();
			tex_coord.y = tex_uv.y * texture.get_height();

			// 如果当前像素的深度值大于zbuffer，则绘制当前像素并更新zbuffer
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
	//		// +0.5四舍五入
	//		screen_coords[j] = Vec3f(int((v.x + 1.) * width / 2. + .5), int((v.y + 1.) * height / 2. + .5), v.z);
	//		world_coords[j] = v;
	//		texture_coords[j] = model->uv(face_uv[j]);
	//	}
	//	// 通过三角形表面法线和光线夹角计算光照强弱
	//	// intensity < 0 说明光线来自物体内部，此时不绘制，是一种简单的面剔除，但是依赖于法线方向，不能完全剔除所有面
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

