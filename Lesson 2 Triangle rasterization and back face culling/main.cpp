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

// 光はスクリーンに垂直に入ります
Vec3f light_dir(0, 0, -1);

// 参考Referrence/Bresenham Algorithm.jpg
// 
// Bresenham算法画直线，与test3结果稍有不同
// 把除法从循环里拿出来了，并且消除了float计算。理论上速度更快
// 性能分析结果 performance3.png 跟 performance1.png对比line的执行时间少了2秒
// 虽然有些微优化，并没有教程中那么明显，可能是编译器不一样(MSVC vs GCC)？

// Bresenham改良版：浮動小数点演算排除による最適化（test3と結果差異あり）
// 除算ループ外移動＋整数演算化で理論的更高速度
// 計測結果 performance3.png（performance1比line処理2秒高速化）
// チュートリアル程の劇的改善無し（コンパイラ最適化差？ MSVC vs GCC）
void line(int x0, int y0, int x1, int y1, TGAImage& image,const TGAColor& color)
{

	// 1 消除斜率的影响
	// 因为采样频率是由x方向的变化频率决定的，y当方向变化频率大于x方向的变化频率的时候，采样频率可能跟不上y轴的变化频率，导致失真
	// 1. 勾配の影響吸収
	// X軸変化率が基準のサンプリングでは、Y軸変化率が大きい場合 サンプリングが追従できず歪み発生のため
	bool steep = false;
	if (abs(x1 - x0) < abs(y1 - y0))
	{
		steep = true;
		// 转置，始终在高频的方向上采样
		// 高周波方向へ座標転置
		swap(x0, y0);
		swap(x1, y1);
	}

	// 2. パラメータ入力順序の影響排除
	if (x0 > x1)
	{
		swap(x0, x1);
		swap(y0, y1);
	}

	// 如有转置，要在转置之后再计算增量
	//  転置後の座標系で増分計算
	int dx = abs(x1 - x0);
	int dy = abs(y1 - y0);

	int de = 2 * dy;
	int e = 0.0;
	int y = y0;

	for (int x = x0; x <= x1; x++)
	{
		if (steep)
			image.set(y, x, color); // 转置只是为了采样，输出颜色的时候还得转置回去
									// 転置座標を復元して描画
		else
			image.set(x, y, color);

		e += de;
		
		if (e > dx)
		{
			y += (y0 < y1 ? 1 : -1); // 根据y的沿伸方向加1或减1
									 // Y進行方向に応じた増分
			e -= 2 * dx; // 因为y的增量有可能不到1，这里计算补正量。比如y增量是0.6>0.5，于是y加1，
			             // 实际多增加了1 - 0.6 = 0.4，所以补正量是-0.4，要加到下一次的增量计算里
				         // 過剰分補正（例：0.6増加時 1.0-0.6=0.4を次回計算に反映）
		}
	}
}

// 线扫描法画填充三角形
// 1 把三角形分为左右两条边A,B，注意B分为上下两部分
// 2 根据y方向的变化率线性计算出A.x B.x
// 3 根据从A.x B.x逐行从左向右画线
//
// スキャンライン法による三角形塗りつぶし
// 1. 三角形を左右境界線A,Bに分割（Bは上下2領域に分離）
// 2. Y軸変化率に基づきA.x,B.xを線形計算
// 3. A.xからB.xまで行単位で走査描画
void triangle_line_scan(Vec2i t0, Vec2i t1, Vec2i t2, TGAImage& image, TGAColor color)
{
	// 退化三角形（面積0）は処理除外
	if (t0.x == t1.x && t0.x == t2.x || t0.y == t1.y && t0.y == t2.y) return;

	// バブルソートで頂点をY軸昇順に整列
	if (t0.y > t1.y) swap(t0, t1);
	if (t0.y > t2.y) swap(t0, t2);
	if (t1.y > t2.y) swap(t1, t2);

	// 総走査ライン数
	int total_line = t2.y - t0.y;

	// ライン単位走査処理
	for (int i = 0; i <= total_line; i++)
	{
		bool upper_half = i > t1.y - t0.y || t0.y == t1.y;	 // B境界の上半分判定
		int segment_line = upper_half ? (t2.y - t1.y) : (t1.y - t0.y);
		float alpha = (float)i / total_line;
		float beta = (float)(i - (upper_half ? (t1.y - t0.y) : 0)) / segment_line;
		Vec2i A = t0 + (t2 - t0) * alpha;
		Vec2i B = upper_half ? (t1 + (t2 - t1) * beta) : (t0 + (t1 - t0) * beta);
		if (A.x > B.x) swap(A, B); // 边界A在左，B在右 // 左境界・右境界の位置補正
		for (int j = A.x; j <= B.x; j++)
			image.set(j, t0.y + i, color);
	}
}

// 重心座標係数の計算
// 参照 Referrence/barycentric coordinates.jpg
Vec3f barycentric(Vec2i* pts, Vec2i P) 
{
	// 重心座標ベクトルt
	Vec3f t = Vec3f(pts[2].x - pts[0].x, pts[1].x - pts[0].x, pts[0].x - P.x) ^
		      Vec3f(pts[2].y - pts[0].y, pts[1].y - pts[0].y, pts[0].y - P.y);
	// 因为t.z = 2 * 三角形面积，小于等于0说明是退化三角形。
	// 又因为精度原因，取一个阈值1
	// 如果是退化三角形，则返回带负值的重心坐标系数，因为重心坐标系数小于0的时候说明点不在三角形内部，绘制时舍弃
	//
	/* 退化三角形判定処理：
 　  t.z = 2 * 三角形面積（符号付き）
  　 絶対値1未満→面積ほぼ0と判定（浮動小数点精度対策）
  　 異常値検出時は無効座標(-1,-1,-1)返却 */
	if (abs(t.z) < 1) return Vec3f(-1, -1, -1);

	// 根据向量t返回重心坐标系数
	// 正規化処理（係数計算式）
	return Vec3f(1.0 - (t.x + t.y) / t.z, t.y / t.z, t.x / t.z);
}

// barycentric方式画填充三角形
// barycentric方式需要和bbox结合起来，否则效率太低
// 
// 重心座標法による三角形塗りつぶし
// 効率低下防止のためBBox（バウンディングボックス）と併用必須
void triangle_barycentric(Vec2i* pts, TGAImage& image, TGAColor color)
{
	// BBox初期化（空領域）
	Vec2i bboxmin(image.get_width() - 1, image.get_height() - 1);
	Vec2i bboxmax(0, 0);

	Vec2i clamp(image.get_width() - 1, image.get_height() - 1);

	// BBox計算処理
	for (int i = 0; i < 3; i++) 
	{
		bboxmin.x = max(0, min(bboxmin.x, pts[i].x));
		bboxmin.y = max(0, min(bboxmin.y, pts[i].y));

		bboxmax.x = min(clamp.x, max(bboxmax.x, pts[i].x));
		bboxmax.y = min(clamp.y, max(bboxmax.y, pts[i].y));
	}

	// 遍历bbox内的所有像素，barycentric方式判断，如果在三角形内部就渲染，否则不渲染
	// BBox領域内ピクセル単位走査 重心座標による包含判定：三角形内部なら描画、それ以外はスキップ
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

		// 三角形の表面法線と光線の角度による光量計算
		// intensity < 0 の場合は物体内部からの光と判定し描画省略（簡易背面カリング）
		// 注: 法線方向依存のため完全なカリングは不可能
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

