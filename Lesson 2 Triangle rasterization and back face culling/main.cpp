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

// 光线垂直射入屏幕里
Vec3f light_dir(0, 0, -1);



// Bresenham算法画直线
// 把除法从循环里拿出来了，并且消除了float计算。理论上速度更快
// 虽然有些微优化，并没有教程中那么明显，可能是编译器不一样(MSVC vs GCC)？
// 参考Referrence/Bresenham Algorithm.jpg
void line(int x0, int y0, int x1, int y1, TGAImage& image,const TGAColor& color)
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

// 线扫描法画填充三角形
// 1 把三角形分为左右两条边A,B，注意B分为上下两部分
// 2 根据y方向的变化率线性计算出A.x B.x
// 3 根据从A.x B.x逐行从左向右画线
void triangle_line_scan(Vec2i t0, Vec2i t1, Vec2i t2, TGAImage& image, TGAColor color)
{
	// 退化三角形不处理
	if (t0.x == t1.x && t0.x == t2.x || t0.y == t1.y && t0.y == t2.y) return;

	// 冒泡排序，使t0 t1 t2按y轴从小到大的顺序排序
	if (t0.y > t1.y) swap(t0, t1);
	if (t0.y > t2.y) swap(t0, t2);
	if (t1.y > t2.y) swap(t1, t2);

	// 总扫描行数
	int total_line = t2.y - t0.y;

	// 逐行扫描
	for (int i = 0; i <= total_line; i++)
	{
		bool upper_half = i > t1.y - t0.y || t0.y == t1.y;	// 是否是B边界的上半部分
		int segment_line = upper_half ? (t2.y - t1.y) : (t1.y - t0.y);
		float alpha = (float)i / total_line;
		float beta = (float)(i - (upper_half ? (t1.y - t0.y) : 0)) / segment_line;
		Vec2i A = t0 + (t2 - t0) * alpha;
		Vec2i B = upper_half ? (t1 + (t2 - t1) * beta) : (t0 + (t1 - t0) * beta);
		if (A.x > B.x) swap(A, B); // 边界A在左，B在右
		for (int j = A.x; j <= B.x; j++)
			image.set(j, t0.y + i, color);
	}
}

// 求重心坐标系数
// 参考Referrence/barycentric coordinates.jpg
Vec3f barycentric(Vec2i* pts, Vec2i P) 
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
void triangle_barycentric(Vec2i* pts, TGAImage& image, TGAColor color)
{
	// 初始bbox为空
	Vec2i bboxmin(image.get_width() - 1, image.get_height() - 1);
	Vec2i bboxmax(0, 0);

	Vec2i clamp(image.get_width() - 1, image.get_height() - 1);

	// 计算bbox
	for (int i = 0; i < 3; i++) 
	{
		bboxmin.x = max(0, min(bboxmin.x, pts[i].x));
		bboxmin.y = max(0, min(bboxmin.y, pts[i].y));

		bboxmax.x = min(clamp.x, max(bboxmax.x, pts[i].x));
		bboxmax.y = min(clamp.y, max(bboxmax.y, pts[i].y));
	}

	// 遍历bbox内的所有像素，barycentric方式判断，如果在三角形内部就渲染，否则不渲染
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
		// 通过三角形表面法线和光线夹角计算光照强弱
		// intensity < 0 说明光线来自物体内部，此时不绘制，是一种简单的面剔除，但是依赖于法线方向，不能完全剔除所有面
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

