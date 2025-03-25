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

// ��Ⱦ���ͼ
struct DepthShader : public IShader
{
    mat<3, 3, float> varying_tri;

    DepthShader() : varying_tri() {}

    virtual Vec4f vertex(int iface, int nthvert)
    {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        gl_Vertex = Viewport * Projection * ModelView * gl_Vertex;
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3])); // NDC��
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bc, TGAColor& color)
    {
        Vec3f p = varying_tri * bc; // ��ֵ
        color = TGAColor(255, 255, 255) * (p.z / depth); // ������� [0,1]
        return false;
    }
};

// Screen-based AO�㷨
float max_elevation_angle(float* zbuffer, Vec2f p, Vec2f dir) 
{
    float maxangle = 0;
    const float t_max = 10;

    // ��������p����t_max��ʱ�䵥λ������
    for (float t = 0.; t < t_max; t += 1.)
    {
        // ��dir�������ߣ���ǰλ��Ϊcur
        Vec2f cur = p + dir * t;
        // ���������Ļ��Χ������ĿǰΪֹ�Ľ��
        if (cur.x >= width || cur.y >= height || cur.x < 0 || cur.y < 0) return maxangle;

        // ��������ݶ�ֵ
        float distance = (p - cur).length();

        // distance��С���ᵼ�·�ĸ��Ӱ����󣬴Ӷ������˷���elevation��Ӱ�죬ʹ���ƫ��
        // ���ԶԱ�diablo_AO.tga �� diablo_AO_no_distance_fix.tga�������������Ȼdiablo_AO.tga��Ч������Ȼ
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

    float* zbuffer = new float[width * height];  //ԭ������zbuffer

    // ��ʼ��Ϊ��Сֵ
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
    depth.write_tga_file("depth.tga"); // ���ͼ


    // AO
    TGAImage frame(width, height, TGAImage::RGB);

    for (int x = 0; x < width; x++)
    {
        for (int y = 0; y < height; y++) 
        {
            // zbuffer��С��˵��û�����塣����
            if (zbuffer[x + y * width] < -1e5) continue;

            float total = 0;
            // ÿ��Ƭ�ΰ�Բ��8����������
            for (float a = 0; a < M_PI * 2 - 1e-4; a += M_PI / 4) 
            {
                // �ݶ�Խ��˵����������̶ֳ�Խ������ɽ�Ⱥ�ƽԭ����������
                // �ݶ���[0,90]��֮�䣬���total��Ϊ����ϵ������90 - �ݶȣ��ݶ�Խ��Խ��
                total += M_PI / 2 - max_elevation_angle(zbuffer, Vec2f(x, y), Vec2f(cos(a), sin(a)));
            }
            // ������ƽ����Ӧ������һ�ּ򵥵�Monte Carlo������
            total /= (M_PI / 2) * 8;

            // ��100���ݷŴ����ķ��ʹ���������(�Թ��ˣ����Ŵ����Ļ���ģ�Ϳ������������׵�)
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