#include <vector>
#include <limits>
#include <iostream>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

Model* model = NULL;
float* shadowbuffer = NULL;

const int width = 3000;
const int height = 3000;

Vec3f light_dir(1, 1, 0);
Vec3f       eye(0, -1, 3);
Vec3f    center(0, 0, 0);
Vec3f        up(0, 1, 0);

//  Gouraud Shading（頂点単位で色計算後、補間処理）
//struct GouraudShader : public IShader
//{
//    // 頂点シェーダー出力  フラグメントシェーダー入力
//    Vec3f varying_intensity;
//
//    // vertex shader
//    virtual Vec4f vertex(int iface, int nthvert)
//    {
//        // モデル頂点データ取得
//        // embed<4> 3D座標→4D同次座標変換
//        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
//
//        // local=>view=>projection=>screen
//        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
//
//        // 拡散光（diffuse）照明計算（法線と光線の角度依存）
//        varying_intensity[nthvert] = std::max(0.f, model->normal(iface, nthvert) * light_dir);
//
//        return gl_Vertex;
//    }
//
//    // fragment shader
//    // bar: 重心座標係数（補間用）
//    virtual bool fragment(Vec3f bar, TGAColor& color)
//    {
//        // 頂点カラーを重心座標で補間
//        float intensity = varying_intensity * bar;
//
//        color = TGAColor(255, 255, 255) * intensity;
//
//        // フラグメント破棄判定（常に描画）
//        return false;
//    }
//};

// Phong Shading，（頂点補間後、ピクセル単位で色計算）
struct Shader : public IShader 
{
    mat<4, 4, float> uniform_M;        // モデル変換行列（原シーン変換用）
    mat<4, 4, float> uniform_MIT;      // 法線行列（※疑問点: OpenGLの法線はビュー空間で計算されるが、ここではクリップ空間を使用？）
    mat<4, 4, float> uniform_Mshadow;  // 光源視点変換行列
    mat<2, 3, float> varying_uv;       // UV座標
    mat<3, 3, float> varying_tri;      // クリップ空間三角形座標
    mat<3, 3, float> varying_nrm;      // 三角形の法線

    Shader(Matrix M, Matrix MIT, Matrix MS) : uniform_M(M), uniform_MIT(MIT), uniform_Mshadow(MS), varying_uv(), varying_tri() {}

    virtual Vec4f vertex(int iface, int nthvert) 
    {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        Vec4f gl_Vertex = Viewport * Projection * ModelView * embed<4>(model->vert(iface, nthvert));
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        varying_nrm.set_col(nthvert, proj<3>(uniform_MIT * embed<4>(model->normal(iface, nthvert), 0.f)));
        return gl_Vertex;
    }

    // fragment shader
    // bc: 重心座標係数（補間用）
    virtual bool fragment(Vec3f bc, TGAColor& color)
    {
        // 采用三个顶点法线的插值，而不是其中任意一点，是因为复杂模型共享的顶点有很多，
        // 所以任一顶点的法线都不一定能代表真实的三角形的法线方向，因此取三个顶点的插值才能取得比较接近的效果
        // 頂点法線の補間理由：
        // 複雑モデルでは共有頂点の法線が実際の三角形の法線方向を
        // 正確に表現できないため、3頂点補間で近似
        Vec3f n = (varying_nrm * bc).normalize();

        Vec4f sb_p = uniform_Mshadow * embed<4>(varying_tri * bc); // 光源視点でのフラグメント位置
        sb_p = sb_p / sb_p[3]; // 同次座標除算→NDC座標変換
        int idx = int(sb_p[0]) + int(sb_p[1]) * width; // 画素位置決定
        const float bias = 1.0; // シャドウアクネ防止用オフセット
        const float ambient = 0.3; // 環境光強度
        float shadow = ambient + (1 - ambient) * (shadowbuffer[idx] < sb_p[2] + bias); // 影係数計算
        Vec2f uv = varying_uv * bc;                 // UV座標補間
        Vec3f l = proj<3>(uniform_M * embed<4>(light_dir)).normalize(); // 正規化光源方向
        Vec3f r = (l - n * (n * l * 2.f)).normalize();   //鏡面反射方向 参照Referrence/mirror reflection.jpg
        Vec3f v = (center - eye).normalize(); // 視点方向ベクトル
        const float shinness = 0.2; // 鏡面反射鋭度係数
        float spec = pow(std::max(0.f, r * v), shinness); // 鏡面反射強度
        float diff = std::max(0.f, n * l);  // 拡散反射係数
        TGAColor diff_color = model->diffuse(uv); // 拡散色テクスチャ取得
        TGAColor spec_color = model->specular(uv); // 鏡面反射色テクスチャ取得
        float light = 2; // 光量係数
        for (int i = 0; i < 3; i++) 
            color[i] = std::min<float>(light * (diff * diff_color[i] + spec * spec_color[0]) * shadow, 255);
       
        return false;
    }
};

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


int main(int argc, char** argv) 
{
    if (2 > argc) 
    {
        std::cerr << "Usage: " << argv[0] << "obj/model.obj" << std::endl;
        return 1;
    }

    float* zbuffer = new float[width * height];  // メインシーンの深度バッファ
    shadowbuffer = new float[width * height];    // 光源視点用深度バッファ

    // 最小値で初期化
    for (int i = width * height; --i; ) 
    {
        zbuffer[i] = shadowbuffer[i] = -std::numeric_limits<float>::max();
    }

    model = new Model(argv[1]);
    light_dir.normalize();

    { // rendering the shadow buffer
        TGAImage depth(width, height, TGAImage::RGB);
        lookat(light_dir, center, up);  // 光源視点のビュー行列設定
        viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
        projection(0); // 平行光源のため正射影（パラメータ0で有効化）

        DepthShader depthshader;
        Vec4f screen_coords[3];
        for (int i = 0; i < model->nfaces(); i++) 
        {
            for (int j = 0; j < 3; j++) 
            {
                screen_coords[j] = depthshader.vertex(i, j);
            }
            triangle(screen_coords, depthshader, depth, shadowbuffer);
        }
        depth.flip_vertically();
        depth.write_tga_file("depth.tga"); // 深度マップ出力
    }

    Matrix M = Viewport * Projection * ModelView;

    { // rendering the frame buffer
        TGAImage frame(width, height, TGAImage::RGB);
        lookat(eye, center, up);
        viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
        projection(-1.f / (eye - center).norm());

        Shader shader(ModelView, (Projection * ModelView).invert_transpose(), M * (Viewport * Projection * ModelView).invert());
        Vec4f screen_coords[3];
        for (int i = 0; i < model->nfaces(); i++) 
        {
            for (int j = 0; j < 3; j++) 
            {
                screen_coords[j] = shader.vertex(i, j);
            }
            triangle(screen_coords, shader, frame, zbuffer);
        }
        frame.flip_vertically(); // to place the origin in the bottom left corner of the image
        frame.write_tga_file("framebuffer.tga");
    }

    delete model;
    delete[] zbuffer;
    delete[] shadowbuffer;
    return 0;
}