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
void line2D(int x0, int y0, int x1, int y1, TGAImage& image, const TGAColor& color)
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

// 重心座標係数の計算
// 参照 Referrence/barycentric coordinates.jpg
Vec3f barycentric(Vec3f* pts, Vec3f P) 
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
void triangle(Vec3f* verts, Vec2f* texs, float* zbuffer, TGAImage& texture, TGAImage& image, TGAColor color)
{
	// BBox初期化（空領域）
	Vec2f bboxmin(numeric_limits<float>::max(), numeric_limits<float>::max());
	Vec2f bboxmax(-numeric_limits<float>::max(), -numeric_limits<float>::max());

	Vec2f clamp(image.get_width() - 1, image.get_height() - 1);

	// BBox計算処理
	for (int i = 0; i < 3; i++) 
	{
		bboxmin.x = max(0.0f, min(bboxmin.x, verts[i].x));
		bboxmin.y = max(0.0f, min(bboxmin.y, verts[i].y));

		bboxmax.x = min(clamp.x, max(bboxmax.x, verts[i].x));
		bboxmax.y = min(clamp.y, max(bboxmax.y, verts[i].y));
	}

	// 遍历bbox内的所有像素，barycentric方式判断，如果在三角形内部就渲染，否则不渲染
	// BBox領域内ピクセル単位走査 重心座標による包含判定：三角形内部なら描画、それ以外はスキップ
	Vec3f P;
	Vec2f tex_uv;
	Vec2i tex_coord;

	for (P.x = bboxmin.x; P.x <= bboxmax.x; P.x++) 
	{
		for (P.y = bboxmin.y; P.y <= bboxmax.y; P.y++) 
		{
			Vec3f bc_screen = barycentric(verts, P); // 重心座標係数
			if (bc_screen.x < 0 || bc_screen.y < 0 || bc_screen.z < 0)
				continue;
			// 重心座標を用いた深度値の補間計算
			P.z = bc_screen.x * verts[0].z + bc_screen.y * verts[1].z + bc_screen.z * verts[2].z;
			// 重心座標を用いたUV座標の補間計算
			tex_uv.x = bc_screen.x * texs[0].x + bc_screen.y * texs[1].x + bc_screen.z * texs[2].x;
			tex_uv.y = bc_screen.x * texs[0].y + bc_screen.y * texs[1].y + bc_screen.z * texs[2].y;
			
			// UV mapping
			tex_coord.x = tex_uv.x * texture.get_width();
			tex_coord.y = tex_uv.y * texture.get_height();

			// 如果当前像素的深度值大于zbuffer，则绘制当前像素并更新zbuffer
			// 深度バッファ比較による描画制御
			if (zbuffer[int(P.x + P.y * width)] < P.z)
			{
				zbuffer[int(P.x + P.y * width)] = P.z;
				image.set(P.x, P.y, texture.get(tex_coord.x, tex_coord.y));
			}	
		}
	}
}

// 行列変換，参照Referrence/Transformation.jpg
/******************* 行列変換 *******************/
// スケーリング
Matrix scale(Vec3f t)
{
	Matrix S = Matrix::identity(4);
	S[0][0] = t[0];
	S[1][1] = t[1];
	S[2][2] = t[2];

	return S;
}

// シアー変換（2D版）
// 注：3D実装ではパラメータが複雑化するため実験用に2D限定
Matrix shear2D(Vec2f t)
{
	Matrix SH = Matrix::identity(3);
	SH[0][1] = t[0];
	SH[1][0] = t[1];

	return SH;
}

// 回転変換（各軸独立）
// 事前計算済みcosθ/sinθで効率化
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

// 平行移動
Matrix translate(Vec3f t)
{
	Matrix T = Matrix::identity(4);
	T[3][0] = t[0];
	T[3][1] = t[1];
	T[3][2] = t[2];

	return T;
}
/******************* 行列変換 *******************/

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

