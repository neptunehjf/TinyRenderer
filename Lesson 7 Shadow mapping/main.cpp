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
    mat<4, 4, float> uniform_M;   // 原场景变换矩阵
    mat<4, 4, float> uniform_MIT; // (Projection*ModelView).invert_transpose()
    mat<4, 4, float> uniform_Mshadow; // transform framebuffer screen coordinates to shadowbuffer screen coordinates
    mat<2, 3, float> varying_uv;  // triangle uv coordinates, written by the vertex shader, read by the fragment shader
    mat<3, 3, float> varying_tri; // triangle coordinates before Viewport transform, written by VS, read by FS
    mat<3, 3, float> varying_nrm; // 三角形的法线

    Shader(Matrix M, Matrix MIT, Matrix MS) : uniform_M(M), uniform_MIT(MIT), uniform_Mshadow(MS), varying_uv(), varying_tri() {}

    virtual Vec4f vertex(int iface, int nthvert) 
    {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        Vec4f gl_Vertex = Viewport * Projection * ModelView * embed<4>(model->vert(iface, nthvert));
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        varying_nrm.set_col(nthvert, proj<3>(uniform_MIT * embed<4>(model->normal(iface, nthvert), 0.f)));
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bc, TGAColor& color) 
    {
        // 采用三个顶点法线的插值，而不是其中任意一点，是因为复杂模型共享的顶点有很多，
        // 所以任一顶点的法线都不一定能代表真实的三角形的法线方向，因此取三个顶点的插值才能取得比较平滑的效果
        Vec3f n = (varying_nrm * bc).normalize();

        Vec4f sb_p = uniform_Mshadow * embed<4>(varying_tri * bc); // 本片段在光源视角下的位置
        sb_p = sb_p / sb_p[3]; // 除以齐次分量 =>ndc
        int idx = int(sb_p[0]) + int(sb_p[1]) * width; // 确定像素位置
        const float bias = 1.0; // 消除shadow acne
        const float ambient = 0.3; // 环境光
        float shadow = ambient + (1 - ambient) * (shadowbuffer[idx] < sb_p[2] + bias); // 计算阴影
        Vec2f uv = varying_uv * bc;                 // 插值uv
        Vec3f l = proj<3>(uniform_M * embed<4>(light_dir)).normalize(); // 光照方向
        Vec3f r = (l - n * (n * l * 2.f)).normalize();   //反射光方向 参照Referrence/mirror reflection.jpg
        Vec3f v = (center - eye).normalize(); //视角方向
        const float shinness = 0.2; // 反光度
        float spec = pow(std::max(0.f, r * v), shinness); // 高光系数
        float diff = std::max(0.f, n * l); // diffuse系数
        TGAColor diff_color = model->diffuse(uv); // diffuse材质
        TGAColor spec_color = model->specular(uv); // 高光材质
        float light = 2; //光照强度
        for (int i = 0; i < 3; i++) 
            color[i] = std::min<float>(light * (diff * diff_color[i] + spec * spec_color[0]) * shadow, 255);
       
        return false;
    }
};

// 渲染光源视角的深度图，用于渲染阴影
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
        Vec3f p = varying_tri * bc; // 插值
        color = TGAColor(255, 255, 255) * (p.z / depth); // 绘制深度 [0,1]
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

    float* zbuffer = new float[width * height];  //原场景的zbuffer
    shadowbuffer = new float[width * height];    //光源视角的zbuffer

    // 初始化为最小值
    for (int i = width * height; --i; ) 
    {
        zbuffer[i] = shadowbuffer[i] = -std::numeric_limits<float>::max();
    }

    model = new Model(argv[1]);
    light_dir.normalize();

    { // rendering the shadow buffer
        TGAImage depth(width, height, TGAImage::RGB);
        lookat(light_dir, center, up);  //光源视角
        viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
        projection(0); // 平行光，正交投影即可，所以参数设为0

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
        depth.write_tga_file("depth.tga"); // 深度图
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