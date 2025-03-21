#include <vector>
#include <iostream>

#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

Model* model = NULL;
const int width = 800;
const int height = 800;

Vec3f light_dir(1, 1, 1);
Vec3f       eye(0, -1, 3);
Vec3f    center(0, 0, 0);
Vec3f        up(0, 1, 0);

// Gouraud Shading，先计算好各顶点的颜色，再对颜色插值
//struct GouraudShader : public IShader
//{
//    // vertex shader的输出, fragment shader的输入
//    Vec3f varying_intensity;
//
//    // vertex shader
//    virtual Vec4f vertex(int iface, int nthvert)
//    {
//        // 读取模型的顶点数据
//        //  embed<4> 3d坐标=>4d齐次坐标
//        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
//
//        // local=>view=>projection=>screen
//        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
//
//        // diffuse光照，与法线和光线的夹角有关
//        varying_intensity[nthvert] = std::max(0.f, model->normal(iface, nthvert) * light_dir);
//
//        return gl_Vertex;
//    }
//
//    // fragment shader
//    // bc是重心坐标系数，用于插值
//    virtual bool fragment(Vec3f bc, TGAColor& color)
//    {
//        // 根据各顶点的颜色进行插值
//        float intensity = varying_intensity * bc;
//
//        color = TGAColor(255, 255, 255) * intensity;
//
//        // 是否舍弃当前片段
//        return false;
//    }
//};

// Phong Shading，先插值顶点，再计算颜色
struct Shader : public IShader
{
    mat<2, 3, float> varying_uv;  // 三角形的uv坐标  
    mat<4, 3, float> varying_tri; // 三角形的裁剪(clip)坐标
    mat<3, 3, float> varying_nrm; // 三角形的法线
    mat<3, 3, float> ndc_tri;     // 三角形的NDC坐标

    // vertex shader
    virtual Vec4f vertex(int iface, int nthvert)
    {
        // 设置各fragment shader需要的参数
        // 注意和openGL一样是按列存储
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        varying_nrm.set_col(nthvert, proj<3>((Projection * ModelView).invert_transpose() * embed<4>(model->normal(iface, nthvert), 0.f)));
        Vec4f gl_Vertex = Projection * ModelView * embed<4>(model->vert(iface, nthvert));
        varying_tri.set_col(nthvert, gl_Vertex);
        ndc_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3])); // 透视除法，clip=>NDC
        return gl_Vertex;
    }

    // fragment shader
    // bc是重心坐标系数，用于插值
    virtual bool fragment(Vec3f bc, TGAColor& color)
    {
        // 采用三个顶点法线的插值，而不是其中任意一点，是因为复杂模型共享的顶点有很多，
        // 所以任一顶点的法线都不一定能代表真实的三角形的法线方向，因此取三个顶点的插值才能取得比较接近的效果
        Vec3f bn = (varying_nrm * bc).normalize();

        Vec2f uv = varying_uv * bc;

        // 计算tangent space，参照Referrence/tangent space.png
        mat<3, 3, float> A;
        A[0] = ndc_tri.col(1) - ndc_tri.col(0);
        A[1] = ndc_tri.col(2) - ndc_tri.col(0);
        A[2] = bn;

        mat<3, 3, float> AI = A.invert();

        Vec3f i = AI * Vec3f(varying_uv[0][1] - varying_uv[0][0], varying_uv[0][2] - varying_uv[0][0], 0);
        Vec3f j = AI * Vec3f(varying_uv[1][1] - varying_uv[1][0], varying_uv[1][2] - varying_uv[1][0], 0);

        mat<3, 3, float> B;
        B.set_col(0, i.normalize());
        B.set_col(1, j.normalize());
        B.set_col(2, bn);

        // 根据tangent space和normalmap 计算此片段的法线
        Vec3f n = (B * model->normal(uv)).normalize();

        // diffuse光照
        float diff = std::max(0.f, n * light_dir);
        color = model->diffuse(uv) * diff;

        // 是否舍弃当前片段
        return false;
    }
};

int main(int argc, char** argv) 
{
    if (2 > argc) 
    {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }

    float* zbuffer = new float[width * height];
    for (int i = width * height; i--; zbuffer[i] = -std::numeric_limits<float>::max());

    TGAImage frame(width, height, TGAImage::RGB);
    lookat(eye, center, up);
    viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
    projection(-1.f / (eye - center).norm());
    light_dir = proj<3>((Projection * ModelView * embed<4>(light_dir, 0.f))).normalize();

    for (int m = 1; m < argc; m++) 
    {
        model = new Model(argv[m]);
        Shader shader;
        for (int i = 0; i < model->nfaces(); i++) {
            for (int j = 0; j < 3; j++) {
                shader.vertex(i, j);
            }
            triangle(shader.varying_tri, shader, frame, zbuffer);
        }
        delete model;
    }
    frame.flip_vertically(); // to place the origin in the bottom left corner of the image
    frame.write_tga_file("framebuffer.tga");

    delete[] zbuffer;
    return 0;
}