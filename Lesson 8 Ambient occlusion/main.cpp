#include <vector>
#include <limits>
#include <iostream>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

Model* model = NULL;

const int width = 800;
const int height = 800;
const float M_PI = 3.14159265358;

Vec3f       eye(1.2, -0.8, 3);
Vec3f    center(0, 0, 0);
Vec3f        up(0, 1, 0);

// 影描画用の光源視点深度マップをレンダリング
struct DepthShader : public IShader
{
    mat<3, 3, float> varying_tri;

    DepthShader() : varying_tri() {}

    virtual Vec4f vertex(int iface, int nthvert)
    {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3])); // NDC化
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bc, TGAColor& color)
    {
        Vec3f p = varying_tri * bc; // 補間座標
        color = TGAColor(255, 255, 255) * (p.z / depth); // 深度値を[0,1]範囲にマッピング
        return false;
    }
};

// Screen-based AO
float max_elevation_angle(float* zbuffer, Vec2f p, Vec2f dir) 
{
    float maxangle = 0;
    const float t_max = 10;

    // 遍历像素p附近t_max个时间单位的像素
    // 
    // ピクセルpからdir方向へt_max距離まで探索
    for (float t = 0.; t < t_max; t += 1.)
    {
        // 沿dir方向发射线，当前位置为cur
        // 現在の探索位置を計算
        Vec2f cur = p + dir * t;
        // 如果超出屏幕范围，返回目前为止的结果
        // 画面外判定（早期リターン）
        if (cur.x >= width || cur.y >= height || cur.x < 0 || cur.y < 0) return maxangle;

        // 计算最大梯度值
        // 最大勾配を計算

        float distance = (p - cur).length();

        // distance过小，会导致分母的影响过大，从而淡化了分子elevation的影响，使结果偏大
        // 可以对比diablo_AO.tga 和 diablo_AO_no_distance_fix.tga这两个结果，显然diablo_AO.tga的效果更自然
        // 
        // 距離が小さすぎると分母の影響が過大になり、標高差(elevation)の影響が相対的に小さくなるため計算結果が過大評価される‌:
        // diablo_AO.tga と diablo_AO_no_distance_fix.tga を比較すると、距離補正を施したdiablo_AO.tgaの結果がより自然な表現となる‌

        if (distance < 1.f) continue; 
        float elevation = zbuffer[int(cur.x) + int(cur.y) * width] - zbuffer[int(p.x) + int(p.y) * width];
        maxangle = std::max(maxangle, atanf(elevation / distance));
    }
    return maxangle;
}

int main(int argc, char** argv)
{
    if (2 > argc)
    {
        std::cerr << "Usage: " << argv[0] << "obj/model.obj" << std::endl;
        return 1;
    }

    float* zbuffer = new float[width * height];  // メインシーンの深度バッファ

    // 最小値で初期化
    for (int i = width * height; --i; )
    {
        zbuffer[i] = -std::numeric_limits<float>::max();
    }

    model = new Model(argv[1]);

    // depth buffer
    TGAImage depth(width, height, TGAImage::RGB);
    lookat(eye, center, up);
    viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
    projection(-1.f / (eye - center).length());

    DepthShader depthshader;
    Vec4f screen_coords[3];
    for (int i = 0; i < model->nfaces(); i++)
    {
        for (int j = 0; j < 3; j++)
        {
            screen_coords[j] = depthshader.vertex(i, j);
        }
        triangle(screen_coords, depthshader, depth, zbuffer);
    }
    depth.flip_vertically();
    depth.write_tga_file("depth.tga"); // 深度マップ出力


    // AO
    TGAImage frame(width, height, TGAImage::RGB);

    for (int x = 0; x < width; x++)
    {
        for (int y = 0; y < height; y++) 
        {
            // zbuffer过小，说明没有物体。跳过
            // 深度値が極端に小さい領域はオブジェクトが存在しないためスキップ
            if (zbuffer[x + y * width] < -1e5) continue;

            float total = 0;
            // 每个片段按圆周8采样个方向
            // 円周上8方向サンプリング
            for (float a = 0; a < M_PI * 2 - 1e-4; a += M_PI / 4) 
            {
                // 梯度越大，说明物体的遮罩程度越大（想象山谷和平原的遮罩区别）
                // 梯度在[0,90]度之间，因此total作为遮罩系数，是90 - 梯度，梯度越大，越暗
                //
                /* 仰角計算の物理的意味:
                   勾配が大きい＝遮蔽が強い（谷間と平原地形の遮蔽差を想像）
                   仰角範囲[0, 90度]のため、遮蔽係数totalは (90度 - 測定角度) で計算
                   勾配↑ → 測定角度↑ → total値↓ → 画像暗く */
                total += M_PI / 2 - max_elevation_angle(zbuffer, Vec2f(x, y), Vec2f(cos(a), sin(a)));
            }
            // 采样求平均，应该算是一种简单的Monte Carlo采样了
            // サンプリングの平均化（簡易Monte Carlo積分）
            total /= (M_PI / 2) * 8;

            // 用100次幂放大结果的方差，使结果更鲜明(试过了，不放大结果的话，模型看起来近乎纯白的)
            // 100乗でコントラストを強調（未処理時はモデルが白色過多になる現象を実験で確認）
            total = pow(total, 100.f);

            frame.set(x, y, TGAColor(total * 255, total * 255, total * 255));
        }
    }

    frame.flip_vertically();
    frame.write_tga_file("AO.tga"); 


    delete model;
    delete[] zbuffer;
    return 0;
}