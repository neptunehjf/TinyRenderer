#include <cmath>
#include <limits>
#include <cstdlib>
#include "our_gl.h"

Matrix ModelView;
Matrix Viewport;
Matrix Projection;

IShader::~IShader() {}

// �ӿڱ任���ü�����=>��Ļ����
// 2D���� [-1,1] => [x+w, y+h]
// ���   [-1,1] => [0, 255]
void viewport(int x, int y, int w, int h)
{
    Viewport = Matrix::identity();
    Viewport[0][3] = x + w / 2.f;
    Viewport[1][3] = y + h / 2.f;
    Viewport[2][3] = depth / 2.f;
    Viewport[0][0] = w / 2.f;
    Viewport[1][1] = h / 2.f;
    Viewport[2][2] = depth / 2.f;
}

// ͶӰ
// ���� Referrence/projection1.png
// ���� Referrence/projection2.png
void projection(float coeff)
{
    Projection = Matrix::identity();
    Projection[3][2] = coeff;
}

// �ӽ�
void lookat(Vec3f eye, Vec3f center, Vec3f up)
{
    // �������ϵ
    Vec3f z = (eye - center).normalize();
    Vec3f x = cross(up, z).normalize();
    Vec3f y = cross(z, x).normalize();

    ModelView = Matrix::identity();
    for (int i = 0; i < 3; i++)
    {
        // ת���������ϵ(view�ռ�)
        ModelView[0][i] = x[i];
        ModelView[1][i] = y[i];
        ModelView[2][i] = z[i];
        // �����Ϊԭ�㣬����λ�������������෴��
        ModelView[i][3] = -center[i];
    }
}

// ����������ϵ��
// �ο�Referrence/barycentric coordinates.jpg
Vec3f barycentric(Vec2f A, Vec2f B, Vec2f C, Vec2f P)
{
    Vec3f s[2];
    for (int i = 2; i--; )
    {
        s[i][0] = C[i] - A[i];
        s[i][1] = B[i] - A[i];
        s[i][2] = A[i] - P[i];
    }
    // ������������u
    Vec3f u = cross(s[0], s[1]);

    // ��Ϊu.z = 2 * �����������С�ڵ���0˵�����˻������Ρ�
    // ����Ϊ����ԭ��ȡһ����ֵ1
    // ������˻������Σ��򷵻ش���ֵ����������ϵ������Ϊ��������ϵ��С��0��ʱ��˵���㲻���������ڲ�������ʱ����
    if (abs(u[2]) > 1e-2)
        return Vec3f(1.f - (u.x + u.y) / u.z, u.y / u.z, u.x / u.z);
    return Vec3f(-1, 1, 1);
}

void triangle(Vec4f* pts, IShader& shader, TGAImage& image, float* zbuffer)
{
    // ��ʼbboxΪ��
    Vec2f bboxmin( std::numeric_limits<float>::max(),  std::numeric_limits<float>::max());
    Vec2f bboxmax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());

    // // ����bbox
    Vec2f clamp(image.get_width() - 1, image.get_height() - 1);
    for (int i=0; i<3; i++) 
    {
        for (int j=0; j<2; j++) 
        {
            bboxmin[j] = std::max(0.f,      std::min(bboxmin[j], pts[i][j] / pts[i][3]));
            bboxmax[j] = std::min(clamp[j], std::max(bboxmax[j], pts[i][j] / pts[i][3]));
        }
    }
    Vec2i P;
    TGAColor color;
    // ����bbox�ڵ���������
    for (P.x=bboxmin.x; P.x<=bboxmax.x; P.x++) 
    {
        for (P.y=bboxmin.y; P.y<=bboxmax.y; P.y++) 
        {
            // ����������ϵ��bc��Ҫ�ñ�׼������Ļ�������
            // ������η���w(λ�ƺ�͸���ӽǶ���Ӱ��w)��4D=>3D
            // ��proj<2> 3D=>2D
            Vec3f bc = barycentric(proj<2>(pts[0] / pts[0][3]), proj<2>(pts[1] / pts[1][3]), proj<2>(pts[2] / pts[2][3]), proj<2>(P));

            // �����������귨���ò�ֵ�������������һ�����ص����z����η���w�����������z/w
            float z = pts[0][2] * bc.x + pts[1][2] * bc.y + pts[2][2] * bc.z;
            float w = pts[0][3] * bc.x + pts[1][3] * bc.y + pts[2][3] * bc.z;
            // ���������[0,depth]��ע����������
            float frag_depth = std::max((float)0.0, std::min(depth, (float)(z / w + .5)));
            frag_depth /= depth;

            // ����㲻���������ڲ��Ͳ���Ⱦ
            // �����ǰ���ص����ֵС��zbuffer�еģ�����Ⱦ��ע���OPENGL���ֵ�ж����෴�ģ�
            if (bc.x<0 || bc.y<0 || bc.z<0 || zbuffer[P.x+P.y*image.get_width()]>frag_depth) continue;

            bool discard = shader.fragment(bc, color);
            if (!discard) 
            {
                // ����zbuffer������
                zbuffer[P.x+P.y*image.get_width()] = frag_depth;
                image.set(P.x, P.y, color);
            }
        }
    }
}

