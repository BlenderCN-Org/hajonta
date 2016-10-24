#include <math.h>
#include <stdint.h>
#include <string.h>

#include "hajonta/math.h"

#ifndef harray_count
#define harray_count(array) (sizeof(array) / sizeof((array)[0]))
#endif

v2
v2add(v2 left, v2 right)
{
    v2 result = {left.x + right.x, left.y + right.y};
    return result;
}

template<typename T>
T
v2sub(T left, T right)
{
    T result = {left.x - right.x, left.y - right.y};
    return result;
}

v2
v2mul(v2 v, float multiplier)
{
    v2 result = {v.x * multiplier, v.y * multiplier};
    return result;
}

v2
v2div(v2 v, float divisor)
{
    v2 result = {v.x / divisor, v.y / divisor};
    return result;
}

float
v2length(v2 v)
{
    float result = sqrtf(v.x * v.x + v.y * v.y);
    return result;
}

v2
v2normalize(v2 v)
{
    float length = v2length(v);
    if (length == 0)
    {
        return v;
    }

    v2 result = {v.x / length, v.y / length};
    return result;
}

float
v2dot(v2 left, v2 right)
{
    float result = (left.x * right.x + left.y * right.y);
    return result;
}

float
v2cross(v2 p, v2 q)
{
    return p.x * q.y - p.y * q.x;
}

v2
v2projection(v2 q, v2 p)
{
    /*
     * Projection of vector p onto q
     */
    return v2mul(q, v2dot(p, q) / v2dot(q, q));
}

bool
v2iequal(v2i left, v2i right)
{
    return left.x == right.x && left.y == right.y;
};

v3
v3add(v3 left, v3 right)
{
    v3 result = {left.x + right.x, left.y + right.y, left.z + right.z};
    return result;
}

v3
v3sub(v3 left, v3 right)
{
    v3 result = {left.x - right.x, left.y - right.y, left.z - right.z};
    return result;
}

v3
v3mul(v3 v, float multiplier)
{
    v3 result = {v.x * multiplier, v.y * multiplier, v.z * multiplier};
    return result;
}

v3
v3div(v3 v, float divisor)
{
    v3 result = {v.x / divisor, v.y / divisor, v.z / divisor};
    return result;
}

float
v3length(v3 v)
{
    float result = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    return result;
}

v3
v3normalize(v3 v)
{
    float length = v3length(v);
    if (length == 0)
    {
        return v;
    }

    v3 result = {v.x / length, v.y / length, v.z / length};
    return result;
}

float
v3dot(v3 left, v3 right)
{
    float result = (left.x * right.x + left.y * right.y + left.z * right.z);
    return result;
}

v3
v3cross(v3 p, v3 q)
{
    v3 result = {
        p.y * q.z - p.z * q.y,
        -(p.x * q.z - p.z * q.x),
        p.x * q.y - p.y * q.x
    };

    return result;
}

static bool
assertEqual(v4 left, v4 right, const char *msg, const char *file, int line)
{
    if (!(
        (left.x == right.x) &&
        (left.y == right.y) &&
        (left.z == right.z) &&
        (left.w == right.w) &&
        1))
    {
#define _P(x, ...) printf("%s(%d) : " x "\n", file, line, __VA_ARGS__)
        _P("TEST FAILED: %s", msg);
        _P("EQUAL: x: %d, y: %d, z: %d, w: %d", left.x == right.x, left.y == right.y, left.z == right.z, left.w == right.w);
        _P("EXPECT: x: %.8f, y: %.8f, z: %.8f, w: %.8f", left.x, left.y, left.z, left.w);
        _P("GOT: x: %.8f, y: %.8f, z: %.8f, w: %.8f", right.x, right.y, right.z, right.w);
#undef _P
        return false;
    }
    return true;
}

static bool
assertEqual(v3 left, v3 right, const char *msg, const char *file, int line)
{
    if (!(
        (left.x == right.x) &&
        (left.y == right.y) &&
        (left.z == right.z) &&
        1))
    {
#define _P(x, ...) printf("%s(%d) : " x "\n", file, line, __VA_ARGS__)
        _P("TEST FAILED: %s", msg);
        _P("EQUAL: x: %d, y: %d, z: %d", left.x == right.x, left.y == right.y, left.z == right.z);
        _P("EXPECT: x: %.8f, y: %.8f, z: %.8f", left.x, left.y, left.z);
        _P("GOT: x: %.8f, y: %.8f, z: %.8f", right.x, right.y, right.z);
#undef _P
        return false;
    }
    return true;
}

static bool
assertEqual(v2 left, v2 right, const char *msg, const char *file, int line)
{
    if (!(
        (left.x == right.x) &&
        (left.y == right.y) &&
        1))
    {
#define _P(x, ...) printf("%s(%d) : " x "\n", file, line, __VA_ARGS__)
        _P("TEST FAILED: %s", msg);
        _P("EQUAL: x: %d, y: %d", left.x == right.x, left.y == right.y);
        _P("EXPECT: x: %.8f, y: %.8f", left.x, left.y);
        _P("GOT: x: %.8f, y: %.8f", right.x, right.y);
#undef _P
        return false;
    }
    return true;
}

static bool
assertEqual(float left, float right, const char *msg, const char *file, int line)
{
#define _P(x, ...) printf("%s(%d) : " x "\n", file, line, __VA_ARGS__)
    if (left != right)
    {
        _P("TEST FAILED: %s", msg);
        _P("EXPECT: %.8f, GOT: %.8f", left, right);
        return false;
    }
    return true;
#undef _P
}

#define QUOTE(x) #x
#define T(x,y) {bool _unit_result = assertEqual(x, y, QUOTE(x) "        " QUOTE(y), __FILE__, __LINE__); if (!_unit_result) return false;};

bool
v2unittests()
{
    {
        v2 a = {0.5f, 0.866f};
        v2 b = {1, 0};
        v2 a1 = {0.5, 0};
        T( a1, (v2projection(b, a)) );
    }

    {
        v2 a = {-4, 1};
        v2 b = {1, 2};
        v2 p_of_a_onto_b = {-0.4f, -0.8f};
        v2 p_of_b_onto_a = {8.0f/17, -2.0f/17};
        T( p_of_a_onto_b, (v2projection(b, a)) );
        T( p_of_b_onto_a, (v2projection(a, b)) );
    }

    {
        v2 a = { 2, 1};
        v2 b = {-3, 4};
        v2 p_of_a_onto_b = {0.24f, -0.32f};
        T( p_of_a_onto_b, (v2projection(b, a)) );
    }

    return true;
}

bool
v3unittests()
{
    v3 cross_a = {3, -3, 1};
    v3 cross_b = {4, 9, 2};
    v3 cross_result = {-15, -2, 39};

    T( (v3{ 7, 6, 3}), (v3add(cross_a, cross_b)) );
    T( (v3{-1, -12, -1}), (v3sub(cross_a, cross_b)) );
    T( (v3{ 6, -6, 2}), (v3mul(cross_a, 2)) );
    T( (v3{ 2, 4.5, 1}), (v3div(cross_b, 2)) );
    T( cross_result, (v3cross(cross_a, cross_b)) );
    T( -13, (v3dot(cross_a, cross_b)) );
    T( 5 * sqrtf(70.0f) , v3length(cross_result) );
    T( 0, (v3dot(cross_result, cross_a)));
    T( 0, (v3dot(cross_result, cross_b)));

    v3 i = {1, 0, 0};
    v3 j = {0, 1, 0};
    v3 k = {0, 0, 1};

    T( k, (v3cross(i, j)) );
    T( i, (v3cross(j, k)) );
    T( j, (v3cross(k, i)) );

    T( (v3mul(k,-1)), (v3cross(j, i)) );
    T( (v3mul(i,-1)), (v3cross(k, j)) );
    T( (v3mul(j,-1)), (v3cross(i, k)) );

    T( (v3{0,0,0}), (v3cross(i, i)) );

    return true;
}

bool
point_in_triangle(v2 point, triangle2 tri)
{
    float s = tri.p0.y * tri.p2.x - tri.p0.x * tri.p2.y + (tri.p2.y - tri.p0.y) * point.x + (tri.p0.x - tri.p2.x) * point.y;
    float t = tri.p0.x * tri.p1.y - tri.p0.y * tri.p1.x + (tri.p0.y - tri.p1.y) * point.x + (tri.p1.x - tri.p0.x) * point.y;

    if ((s < 0) != (t < 0))
        return false;

    float a = -tri.p1.y * tri.p2.x + tri.p0.y * (tri.p2.x - tri.p1.x) + tri.p0.x * (tri.p1.y - tri.p2.y) + tri.p1.x * tri.p2.y;
    if (a < 0.0)
    {
        s = -s;
        t = -t;
        a = -a;
    }
    return (s > 0) && (t > 0) && ((s + t) < a);
}

v3
winded_triangle_normal(triangle3 tri)
{
    v3 e0 = v3sub(tri.p1, tri.p0);
    v3 e1 = v3sub(tri.p2, tri.p0);
    v3 normal = v3cross(e0, e1);
    normal = v3normalize(normal);
    return normal;
}

bool
point_in_rectangle(v2 point, rectangle2 rect)
{
    return (point.x > rect.position.x) &&
        (point.y > rect.position.y) &&
        (point.x < (rect.position.x + rect.dimension.x)) &&
        (point.y < (rect.position.y + rect.dimension.y));
}

bool
line_intersect(line2 ppr, line2 qqs, v2 *intersect_point)
{
    /*
    t = (q − p) × s / (r × s)
    u = (q − p) × r / (r × s)
    */
    v2 p = ppr.position;
    v2 r = ppr.direction;
    v2 q = qqs.position;
    v2 s = qqs.direction;

    v2 q_minus_p = v2sub(q, p);
    float r_cross_s = v2cross(r, s);
    float t = v2cross(q_minus_p, s) / r_cross_s;
    float u = v2cross(q_minus_p, r) / r_cross_s;

    *intersect_point = v2add(p, v2mul(r, t));
    if ((t >= 0) && (t <= 1) && (u >= 0) && (u <= 1))
    {
        return true;
    }
    return false;
}

v4
v4add(v4 left, v4 right)
{
    v4 result = {left.x + right.x, left.y + right.y, left.z + right.z, left.w + right.w};
    return result;
}

v4
v4sub(v4 left, v4 right)
{
    v4 result = {left.x - right.x, left.y - right.y, left.z - right.z, left.w - right.w};
    return result;
}

v4
v4mul(v4 v, float multiplier)
{
    v4 result = {v.x * multiplier, v.y * multiplier, v.z * multiplier, v.w * multiplier};
    return result;
}

v4
v4div(v4 v, float divisor)
{
    v4 result = {v.x / divisor, v.y / divisor, v.z / divisor, v.w / divisor};
    return result;
}

float
v4length(v4 v)
{
    float result = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
    return result;
}

v4
v4normalize(v4 v)
{
    float length = v4length(v);
    if (length == 0)
    {
        return v;
    }

    v4 result = {v.x / length, v.y / length, v.z / length, v.w / length};
    return result;
}

float
v4dot(v4 left, v4 right)
{
    float result = (left.x * right.x + left.y * right.y + left.z * right.z + left.w * right.w);
    return result;
}


m4
m4identity()
{
    m4 result = {};
    result.cols[0].E[0] = 1;
    result.cols[1].E[1] = 1;
    result.cols[2].E[2] = 1;
    result.cols[3].E[3] = 1;
    return result;
};

void
m4copy(m4 *dest, const m4 source)
{
    for (uint32_t col = 0;
            col < harray_count(dest[0].cols);
            ++col)
    {
        *(dest->cols + col) = source.cols[col];
    }
};

void
m4transpose(m4 *matrix)
{
    for (uint32_t col = 0;
            col < 4;
            ++col)
    {
        for (uint32_t row = 0;
                row < col+1;
                ++row)
        {
            float t = matrix->cols[col].E[row];
            matrix->cols[col].E[row] = matrix->cols[row].E[col];
            matrix->cols[row].E[col] = t;
        }
    }
};

m4
m4transposed(const m4 matrix)
{
    m4 result;
    m4copy(&result, matrix);
    m4transpose(&result);
    return result;
}

m4
m4mul(const m4 matrix, float multiplier)
{
    m4 result = {};

    for (uint32_t col = 0;
            col < harray_count(matrix.cols);
            ++col)
    {
        for (uint32_t row = 0;
                row < harray_count(matrix.cols[0].E);
                ++row)
        {
            result.cols[col].E[row] = matrix.cols[col].E[row] * multiplier;
        }
    }
    return result;
}


m4
m4div(const m4 &a, float divisor)
{
    float multiplier = 1.0f / divisor;
    return m4mul(a, multiplier);

}


m4
m4adjugate(const m4 &a)
{
    m4 result = {
        {
            {
                a.y.y*a.z.z*a.w.w + a.w.y*a.y.z*a.z.w + a.z.y*a.w.z*a.y.w - a.y.y*a.w.z*a.z.w - a.z.y*a.y.z*a.w.w - a.w.y*a.z.z*a.y.w,
                a.x.y*a.w.z*a.z.w + a.z.y*a.x.z*a.w.w + a.w.y*a.z.z*a.x.w - a.w.y*a.x.z*a.z.w - a.z.y*a.w.z*a.x.w - a.x.y*a.z.z*a.w.w,
                a.x.y*a.y.z*a.w.w + a.w.y*a.x.z*a.y.w + a.y.y*a.w.z*a.x.w - a.x.y*a.w.z*a.y.w - a.y.y*a.x.z*a.w.w - a.w.y*a.y.z*a.x.w,
                a.x.y*a.z.z*a.y.w + a.y.y*a.x.z*a.z.w + a.z.y*a.y.z*a.x.w - a.x.y*a.y.z*a.z.w - a.z.y*a.x.z*a.y.w - a.y.y*a.z.z*a.x.w
            },
            {
                a.y.z*a.w.w*a.z.x + a.z.z*a.y.w*a.w.x + a.w.z*a.z.w*a.y.x - a.y.z*a.z.w*a.w.x - a.w.z*a.y.w*a.z.x - a.z.z*a.w.w*a.y.x,
                a.x.z*a.z.w*a.w.x + a.w.z*a.x.w*a.z.x + a.z.z*a.w.w*a.x.x - a.x.z*a.w.w*a.z.x - a.z.z*a.x.w*a.w.x - a.w.z*a.z.w*a.x.x,
                a.x.z*a.w.w*a.y.x + a.y.z*a.x.w*a.w.x + a.w.z*a.y.w*a.x.x - a.x.z*a.y.w*a.w.x - a.w.z*a.x.w*a.y.x - a.y.z*a.w.w*a.x.x,
                a.x.z*a.y.w*a.z.x + a.z.z*a.x.w*a.y.x + a.y.z*a.z.w*a.x.x - a.x.z*a.z.w*a.y.x - a.y.z*a.x.w*a.z.x - a.z.z*a.y.w*a.x.x
            },
            {
                a.y.w*a.z.x*a.w.y + a.w.w*a.y.x*a.z.y + a.z.w*a.w.x*a.y.y - a.y.w*a.w.x*a.z.y - a.z.w*a.y.x*a.w.y - a.w.w*a.z.x*a.y.y,
                a.x.w*a.w.x*a.z.y + a.z.w*a.x.x*a.w.y + a.w.w*a.z.x*a.x.y - a.x.w*a.z.x*a.w.y - a.w.w*a.x.x*a.z.y - a.z.w*a.w.x*a.x.y,
                a.x.w*a.y.x*a.w.y + a.w.w*a.x.x*a.y.y + a.y.w*a.w.x*a.x.y - a.x.w*a.w.x*a.y.y - a.y.w*a.x.x*a.w.y - a.w.w*a.y.x*a.x.y,
                a.x.w*a.z.x*a.y.y + a.y.w*a.x.x*a.z.y + a.z.w*a.y.x*a.x.y - a.x.w*a.y.x*a.z.y - a.z.w*a.x.x*a.y.y - a.y.w*a.z.x*a.x.y
            },
            {
                a.y.x*a.w.y*a.z.z + a.z.x*a.y.y*a.w.z + a.w.x*a.z.y*a.y.z - a.y.x*a.z.y*a.w.z - a.w.x*a.y.y*a.z.z - a.z.x*a.w.y*a.y.z,
                a.x.x*a.z.y*a.w.z + a.w.x*a.x.y*a.z.z + a.z.x*a.w.y*a.x.z - a.x.x*a.w.y*a.z.z - a.z.x*a.x.y*a.w.z - a.w.x*a.z.y*a.x.z,
                a.x.x*a.w.y*a.y.z + a.y.x*a.x.y*a.w.z + a.w.x*a.y.y*a.x.z - a.x.x*a.y.y*a.w.z - a.w.x*a.x.y*a.y.z - a.y.x*a.w.y*a.x.z,
                a.x.x*a.y.y*a.z.z + a.z.x*a.x.y*a.y.z + a.y.x*a.z.y*a.x.z - a.x.x*a.z.y*a.y.z - a.y.x*a.x.y*a.z.z - a.z.x*a.y.y*a.x.z
            }
        }
    };
    return result;
}


float
m4determinant(const m4 &a)
{
    return
        a.x.x*(a.y.y*a.z.z*a.w.w + a.w.y*a.y.z*a.z.w + a.z.y*a.w.z*a.y.w - a.y.y*a.w.z*a.z.w - a.z.y*a.y.z*a.w.w - a.w.y*a.z.z*a.y.w) +
        a.x.y*(a.y.z*a.w.w*a.z.x + a.z.z*a.y.w*a.w.x + a.w.z*a.z.w*a.y.x - a.y.z*a.z.w*a.w.x - a.w.z*a.y.w*a.z.x - a.z.z*a.w.w*a.y.x) +
        a.x.z*(a.y.w*a.z.x*a.w.y + a.w.w*a.y.x*a.z.y + a.z.w*a.w.x*a.y.y - a.y.w*a.w.x*a.z.y - a.z.w*a.y.x*a.w.y - a.w.w*a.z.x*a.y.y) +
        a.x.w*(a.y.x*a.w.y*a.z.z + a.z.x*a.y.y*a.w.z + a.w.x*a.z.y*a.y.z - a.y.x*a.z.y*a.w.z - a.w.x*a.y.y*a.z.z - a.z.x*a.w.y*a.y.z);
}


m4
m4inverse(const m4 &a)
{
    return m4div(m4adjugate(a), m4determinant(a));
}


bool
m4identical(const m4 left, const m4 right)
{
    for (uint32_t col = 0;
            col < harray_count(left.cols);
            ++col)
    {
        const v4 *lc = left.cols + col;
        const v4 *rc = right.cols + col;
        for (uint32_t row = 0;
                row < harray_count(left.cols[0].E);
                ++row)
        {
            if(lc->E[row] != rc->E[row])
            {
                return false;
            }
        }
    }
    return true;
}

m4
m4add(const m4 left, const m4 right)
{
    m4 result = {};

    for (uint32_t col = 0;
            col < harray_count(left.cols);
            ++col)
    {
        for (uint32_t row = 0;
                row < harray_count(left.cols[0].E);
                ++row)
        {
            result.cols[col].E[row] = left.cols[col].E[row] + right.cols[col].E[row];
        }
    }

    return result;
}

m4
m4sub(const m4 left, const m4 right)
{
    m4 result = {};

    for (uint32_t col = 0;
            col < harray_count(left.cols);
            ++col)
    {
        for (uint32_t row = 0;
                row < harray_count(left.cols[0].E);
                ++row)
        {
            result.cols[col].E[row] = left.cols[col].E[row] - right.cols[col].E[row];
        }
    }

    return result;
}

m4
m4mul(const m4 left, const m4 right)
{
    m4 result = {};
    uint32_t ncols = harray_count(left.cols);
    uint32_t nrows = harray_count(left.cols[0].E);
    for (uint32_t col = 0;
            col < ncols;
            ++col)
    {
        for (uint32_t row = 0;
                row < nrows;
                ++row)
        {
            v4 lrow = {
                left.cols[0].E[row],
                left.cols[1].E[row],
                left.cols[2].E[row],
                left.cols[3].E[row],
            };
            result.cols[col].E[row] = v4dot(lrow, right.cols[col]);
        }
    }
    return result;
}

v4
m4mul(const m4 left, const v4 right)
{
    v4 ret = {};
    for (uint32_t row = 0; row < 4; ++row)
    {
        for (uint32_t col = 0; col < 4; ++col)
        {
            ret.E[row]  += left.cols[col].E[row] * right.E[col];
        }
    }
    return ret;
}

void
m4sprint(char *msg, uint32_t msg_size, const m4 left)
{
    sprintf(msg, "{");
    uint32_t ncols = harray_count(left.cols);
    uint32_t nrows = harray_count(left.cols[0].E);

    for (uint32_t col = 0;
            col < ncols;
            ++col)
    {
        sprintf(msg+strlen(msg), "{");

        const v4 *lc = left.cols + col;
        for (uint32_t row = 0;
                row < nrows;
                ++row)
        {
            sprintf(msg+strlen(msg), "%0.4f,", lc->E[row]);
        }

        sprintf(msg+strlen(msg), "},");
    }
    sprintf(msg+strlen(msg), "}");
}

static bool
assertEqual(m4 left, m4 right, const char *msg, const char *file, int line)
{
    if (!m4identical(left, right))
    {
#define _P(x, ...) printf("%s(%d) : " x "\n", file, line, __VA_ARGS__)
        _P("TEST FAILED: %s", msg);
        char m4msg[10240];
        m4sprint(m4msg, sizeof(m4msg), left);
        _P("EXPECTED: %s", m4msg);
        m4sprint(m4msg, sizeof(m4msg), right);
        _P("GOT     : %s", m4msg);
#undef _P
        return false;
    }
    return true;
}

bool
m4unittests()
{
    m4 i = m4identity();
    m4 ic;
    m4copy(&ic, i);
    m4 itransposed;
    itransposed = m4transposed(i);
    T( i, ic );
    T( i, itransposed );

    ic.cols[0].E[1] = 2.0;
    itransposed = m4transposed(ic);
    T( (itransposed.cols[1].E[0]), (ic.cols[0].E[1]) );

    m4copy(&ic, i);
    m4 dic = m4mul(i, 2.0f);
    T( (dic.cols[0].E[0]), 2.0f );
    m4 dic2 = m4mul(dic, 2.0f);
    T( (dic2.cols[0].E[0]), 4.0f );
    m4 ii = m4add(i, i);
    T( ii, dic );
    m4 iiminusi = m4sub(ii, i);
    T( iiminusi, i );

    m4 m4mul1 = {};
    m4mul1.cols[0] = {1,4,2,3};
    m4mul1.cols[1] = {2,3,1,2};
    m4mul1.cols[2] = {3,2,4,1};
    m4mul1.cols[3] = {4,1,3,4};
    m4 m4mule = {};
    m4mule.cols[0] = {27,23,23,25};
    m4mule.cols[1] = {19,21,17,21};
    m4mule.cols[2] = {23,27,27,21};
    m4mule.cols[3] = {31,29,33,33};
    m4 m4mulgot = m4mul(m4mul1, m4mul1);
    T( m4mule, m4mulgot );

    m4 m4mul2 = {};
    m4mul2.cols[0] = {4,1,3,2};
    m4mul2.cols[1] = {3,2,4,3};
    m4mul2.cols[2] = {2,3,1,4};
    m4mul2.cols[3] = {1,4,2,1};

    m4 m4mule2 = {};
    m4mule2.cols[0] = {23,27,27,25};
    m4mule2.cols[1] = {21,19,23,19};
    m4mule2.cols[2] = {27,23,23,29};
    m4mule2.cols[3] = {29,31,27,27};
    m4 m4mulgot2 = m4mul(m4mul2, m4mul1);
    T( m4mule2, m4mulgot2 );

    m4 m4mul1t = m4transposed(m4mul1);

    m4 m4mul1et = {};
    m4mul1et.cols[0] = {1,2,3,4};
    m4mul1et.cols[1] = {4,3,2,1};
    m4mul1et.cols[2] = {2,1,4,3};
    m4mul1et.cols[3] = {3,2,1,4};
    T (m4mul1et, m4mul1t );

    // (FG)ᵀ = GᵀFᵀ
    m4 m4mul2t = m4transposed(m4mul2);
    m4 m4mule2t = m4transposed(m4mule2);
    m4 m4mulgot2t = m4mul(m4mul1t, m4mul2t);
    T ( m4mule2t, m4mulgot2t );

    m4 ixi = m4mul(i, i);
    T ( i, ixi );

    m4 m4mul1xi = m4mul(m4mul1, i);
    T ( m4mul1, m4mul1xi );

    {
        v4 m4mulv4et = { 1, -3, 0, 0 };
        m4 m4mulv4m4 = {};
        m4mulv4m4.cols[0] = {  1,  0,  0,  0 };
        m4mulv4m4.cols[1] = { -1, -3,  0,  0 };
        m4mulv4m4.cols[2] = {  2,  1,  0,  0 };
        m4mulv4m4.cols[3] = {  0,  0,  0,  0 };
        v4 m4mulv4v4 = { 2, 1, 0, 0 };
        v4 m4mulv4got = m4mul(m4mulv4m4, m4mulv4v4);
        T( m4mulv4et, m4mulv4got );
    }

    {
        v4 m4mulv4et = { 0, -3, -6, -9 };
        m4 m4mulv4m4 = {};
        m4mulv4m4.cols[0] = {  1,  4,  7, 10 };
        m4mulv4m4.cols[1] = {  2,  5,  8, 11 };
        m4mulv4m4.cols[2] = {  3,  6,  9, 12 };
        m4mulv4m4.cols[3] = {  0,  0,  0,  0 };
        v4 m4mulv4v4 = { -2, 1, 0, 0 };
        v4 m4mulv4got = m4mul(m4mulv4m4, m4mulv4v4);
        T( m4mulv4et, m4mulv4got );
    }

    {
        v4 m4mulv4et = { 13, 31, 49,  0 };
        m4 m4mulv4m4 = {};
        m4mulv4m4.cols[0] = {  1,  4,  7, 0 };
        m4mulv4m4.cols[1] = {  2,  5,  8, 0 };
        m4mulv4m4.cols[2] = {  3,  6,  9, 0 };
        m4mulv4m4.cols[3] = {  0,  0,  0, 0 };
        v4 m4mulv4v4 = { 2, 1, 3, 0 };
        v4 m4mulv4got = m4mul(m4mulv4m4, m4mulv4v4);
        T( m4mulv4et, m4mulv4got );
    }

    return true;
}

struct
AxisAngle
{
    v3 axis;
    float angle;
};

AxisAngle
AxisAngleFromQuaternion(Quaternion _q)
{
    v4 q = {_q.x, _q.y, _q.z, _q.w};
    if (q.w > 1.0)
    {
        q = v4normalize(q);
    }

    if (q.w == 1.0f)
    {
        return { {0,1,0}, 0 };
    }
    float divisor = sqrtf(1.0f - q.w * q.w);
    v3 axis = {
        q.x / divisor,
        q.y / divisor,
        q.z / divisor,
    };
    float angle = -2.0f * acosf(q.w);
    return {
        axis,
        angle,
    };
}

m4
m4rotation(v3 axis, float angle)
{
    float cosa = cosf(angle);
    float sina = sinf(angle);
    axis = v3normalize(axis);

    m4 result;
    result.cols[0] = {
        cosa + ((1.0f - cosa) * axis.x * axis.x),
        (1.0f - cosa) * axis.y * axis.x - sina * axis.z,
        (1.0f - cosa) * axis.z * axis.x + sina * axis.y,
        0.0f,
    };
    result.cols[1] = {
        ((1.0f - cosa) * axis.x * axis.y) + (sina * axis.z),
        cosa + ((1.0f - cosa) * axis.y * axis.y),
        ((1.0f - cosa) * axis.z * axis.y) - (sina * axis.x),
        0.0f,
    };
    result.cols[2] = {
        (1.0f - cosa) * axis.x * axis.z - sina * axis.y,
        (1.0f - cosa) * axis.y * axis.z + sina * axis.x,
        cosa + (1.0f - cosa) * axis.z * axis.z,
        0.0f,
    };
    result.cols[3] = {0.0f, 0.0f, 0.0f, 1.0f};

    return result;
}

m4
m4rotation(Quaternion q)
{
    AxisAngle aa = AxisAngleFromQuaternion(q);
    return m4rotation(aa.axis, aa.angle);
}

m4
m4frustumprojection(float near_, float far_, v2 bottom_left, v2 top_right)
{
    float b = bottom_left.y;
    float l = bottom_left.x;
    float t = top_right.y;
    float r = top_right.x;

    m4 result = {};
    result.cols[0].E[0] = (near_ * 2.0f) / (r - l);
    result.cols[1].E[1] = (2.0f * near_) / (t - b);
    result.cols[2] = {
        (r + l) / (r - l),
        (t + b) / (t - b),
        -((far_ + near_) / ( far_ - near_)),
        -1,
    };
    result.cols[3].E[2] = -((2 * near_ * far_) / (far_ - near_));
    return result;
}

m4
m4orthographicprojection(float near_, float far_, v2 bottom_left, v2 top_right)
{
    float b = bottom_left.y;
    float l = bottom_left.x;
    float t = top_right.y;
    float r = top_right.x;

    m4 result = {};
    result.cols[0].E[0] = 2.0f / (r - l);
    result.cols[1].E[1] = 2.0f / (t - b);
    result.cols[2].E[2] = -2.0f / (far_ - near_);
    result.cols[3].E[0] = -(r + l) / (r - l);
    result.cols[3].E[1] = -(t + b) / (t - b);
    result.cols[3].E[2] = -(far_ + near_) / (far_ - near_);
    result.cols[3].E[3] = 1.0f;
    return result;
}

m4
m4scale(float a)
{
    m4 result = {};
    result.cols[0].E[0] = a;
    result.cols[1].E[1] = a;
    result.cols[2].E[2] = a;
    result.cols[3].E[3] = 1.0f;
    return result;
}

m4
m4scale(v3 a)
{
    m4 result = {};
    result.cols[0].E[0] = a.x;
    result.cols[1].E[1] = a.y;
    result.cols[2].E[2] = a.z;
    result.cols[3].E[3] = 1.0f;
    return result;
}

m4
m4translate(v3 a)
{
    m4 result = m4identity();
    result.cols[3].x = a.x;
    result.cols[3].y = a.y;
    result.cols[3].z = a.z;
    return result;
}

m4
m4lookat(v3 eye, v3 target, v3 up)
{
    v3 forward = v3normalize(v3sub(target, eye));
    v3 side = v3normalize(v3cross(forward, up));
    v3 calc_up = v3cross(side, forward);

    m4 view = m4identity();
    view.cols[0].E[0] = side.x;
    view.cols[1].E[0] = side.y;
    view.cols[2].E[0] = side.z;
    view.cols[0].E[1] = calc_up.x;
    view.cols[1].E[1] = calc_up.y;
    view.cols[2].E[1] = calc_up.z;
    view.cols[0].E[2] = -forward.x;
    view.cols[1].E[2] = -forward.y;
    view.cols[2].E[2] = -forward.z;
    view.cols[3].E[0] = -v3dot(side, eye);
    view.cols[3].E[1] = -v3dot(calc_up, eye);
    view.cols[3].E[2] = v3dot(forward, eye);
    return view;
}

float lerp(float a, float b, float s)
{
    return a + (b - a) * s;
}

v4 lerp(v4 a, v4 b, float s)
{
    return v4add(a, v4mul(v4sub(b, a), s));
}

bool
ray_plane_intersect(Ray r, Plane p, float *t, v3 *q)
{
    *t = (p.distance - v3dot(p.normal, r.location)) / v3dot(p.normal, r.direction);
    *q = v3add(r.location, v3mul(r.direction, *t));
    return !isinf(*t);
}

bool
segment_plane_intersect(v3 a, v3 b, Plane p, float *t, v3 *q)
{
    v3 ab = v3sub(b, a);
    *t = (p.distance - v3dot(p.normal, a)) / v3dot(p.normal, ab);
    if (*t >= 0.0f && *t <= 1.0f)
    {
        *q = v3add(a, v3mul(ab, *t));
        return true;
    }
    return false;
}
