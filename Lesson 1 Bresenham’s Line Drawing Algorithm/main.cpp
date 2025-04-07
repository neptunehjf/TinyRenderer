#include "tgaimage.h"
#include "geometry.h"
#include "model.h"

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);
Model* model = NULL;
const int width = 800;
const int height = 800;

using namespace std;

// test1
//void line(int x0, int y0, int x1, int y1, TGAImage& image, TGAColor color)
//{
//  floatでサンプリング
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
// 最適化注釈：浮動小数点演算(float)回避 → 整数演算(int)採用
// 処理軸：X軸方向の離散化サンプリング（1px単位）
//	for (int x = x0; x <= x1; x++)
//	{
//		float t = (float)(x - x0) / (x1 - x0);
//		int y = y0 + (y1 - y0) * t;
//		image.set(x, y, color);
//	}
//}

// test3
// パフォーマンス解析結果 performance1.png
//void line(int x0, int y0, int x1, int y1, TGAImage& image, TGAColor color)
//{
//	// 1. パラメータ入力順序の影響排除
//	if (x0 > x1)
//	{
//		swap(x0, x1);
//		swap(y0, y1);
//	}
//	// 2 消除斜率的影响
//	// 因为采样频率是由x方向的变化频率决定的，y当方向变化频率大于x方向的变化频率的时候，采样频率可能跟不上y轴的变化频率，导致失真
// // 2. 勾配の影響吸収
// // X軸変化率が基準のサンプリングでは、Y軸変化率が大きい場合 サンプリングが追従できず歪み発生のため
//	int dx = abs(x1 - x0);
//	int dy = abs(y1 - y0);
//	if (dx < dy)
//	{
//		// 转置，始终在高频的方向上采样
//      // 高周波方向へ座標転置
//		swap(x0, y0);
//		swap(x1, y1);
//	}
//
//	for (int x = x0; x <= x1; x++)
//	{
//		float t = (float)(x - x0) / (x1 - x0);
//		int y = y0 + (y1 - y0) * t;
//		if (dx < dy)
//			image.set(y, x, color); // 转置只是为了采样，输出颜色的时候还得转置回去
//									// 転置座標を復元して描画
//		else
//			image.set(x, y, color);
//	}
//}

// test4 
// 参考Referrence/Bresenham Algorithm.jpg
// 
// Bresenham算法画直线，与test3结果稍有不同
// 把除法从循环里拿出来了，理论上速度更快
// 性能分析结果 performance2.png 跟 performance1.png对比line的执行时间少了2秒
// 虽然有些微优化，并没有教程中那么明显，可能是编译器不一样(MSVC vs GCC)？
// 
// Bresenhamのアルゴリズムによる直線描画（test3と結果が微妙に異なる）
// 除算処理をループ外に移動し理論上高速化
// パフォーマンス解析結果 performance2.png（line処理時間がperformance1比2秒短縮）
// 最適化効果はチュートリアル程顕著でない（コンパイラ差異？ MSVC vs GCC）
// 
//void line(int x0, int y0, int x1, int y1, TGAImage& image, TGAColor color)
//{
//	// 1. パラメータ入力順序の影響排除
//	if (x0 > x1)
//	{
//		swap(x0, x1);
//		swap(y0, y1);
//	}
//	// 2 消除斜率的影响
//	// 因为采样频率是由x方向的变化频率决定的，y当方向变化频率大于x方向的变化频率的时候，采样频率可能跟不上y轴的变化频率，导致失真
// // 2. 勾配の影響吸収
// // X軸変化率が基準のサンプリングでは、Y軸変化率が大きい場合 サンプリングが追従できず歪み発生のため
//	bool steep = false;
//	if (abs(x1 - x0) < abs(y1 - y0))
//		steep = true;
//
//	if (steep)
//	{
//		// 转置，始终在高频的方向上采样
//      // 高周波方向へ座標転置
//		swap(x0, y0);
//		swap(x1, y1);
//	}
//
//	// 如有转置，要在转置之后再计算增量
//  転置後の座標系で増分計算
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
//			image.set(y, x, color); // 转置只是为了采样，输出颜色的时候还得转置回去
//									// 転置座標を復元して描画
//		else
//			image.set(x, y, color);
//
//		e += de;
//		if (e > 0.5)
//		{
//			y += (y0 < y1 ? 1 : -1); // 根据y的沿伸方向加1或减1
//									 // Y進行方向に応じた増分
//			e -= 1; //因为y的增量有可能不到1，这里计算补正量。比如y增量是0.6>0.5，于是y加1，
//			        //实际多增加了1 - 0.6 = 0.4，所以补正量是-0.4，要加到下一次的增量计算里
//				    // 過剰分補正（例：0.6増加時 1.0-0.6=0.4を次回計算に反映）
//		}
//	}
//}

// test5
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

//void line(int x0, int y0, int x1, int y1, TGAImage& image, TGAColor color) {
//	bool steep = false;
//	if (std::abs(x0 - x1) < std::abs(y0 - y1)) {
//		std::swap(x0, y0);
//		std::swap(x1, y1);
//		steep = true;
//	}
//	if (x0 > x1) {
//		std::swap(x0, x1);
//		std::swap(y0, y1);
//	}
//	int dx = x1 - x0;
//	int dy = y1 - y0;
//	int derror2 = std::abs(dy) * 2;
//	int error2 = 0;
//	int y = y0;
//	for (int x = x0; x <= x1; x++) {
//		if (steep) {
//			image.set(y, x, color);
//		}
//		else {
//			image.set(x, y, color);
//		}
//		error2 += derror2;
//		if (error2 > dx) {
//			y += (y1 > y0 ? 1 : -1);
//			error2 -= dx * 2;
//		}
//	}
//}

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

