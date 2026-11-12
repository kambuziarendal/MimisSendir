/*
 * HSLuv-C: Human-friendly HSL
 * <https://github.com/hsluv/hsluv-c>
 * <https://www.hsluv.org/>
 *
 * Copyright (c) 2015 Alexei Boronine (original idea, JavaScript implementation)
 * Copyright (c) 2015 Roger Tallada (Obj-C implementation)
 * Copyright (c) 2017 Martin Mitas (C implementation, based on Obj-C implementation)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

//#include "hsluv.h"

#include <float.h>
#include <math.h>

#include <GraphicsDefs.h>


typedef struct Triplet_tag Triplet;
struct Triplet_tag {
    double a;
    double b;
    double c;
};

/* for RGB */
static const Triplet m[3] = {
    {  3.24096994190452134377, -1.53738317757009345794, -0.49861076029300328366 },
    { -0.96924363628087982613,  1.87596750150772066772,  0.04155505740717561247 },
    {  0.05563007969699360846, -0.20397695888897656435,  1.05697151424287856072 }
};

static const double ref_u = 0.19783000664283680764;
static const double ref_v = 0.46831999493879100370;

static const double kappa = 903.29629629629629629630;
static const double epsilon = 0.00885645167903563082;


typedef struct Bounds_tag Bounds;
struct Bounds_tag {
    double a;
    double b;
};


static void
get_bounds(double l, Bounds bounds[6])
{
    double tl = l + 16.0;
    double sub1 = (tl * tl * tl) / 1560896.0;
    double sub2 = (sub1 > epsilon ? sub1 : (l / kappa));
    int channel;
    int t;

    for(channel = 0; channel < 3; channel++) {
        double m1 = m[channel].a;
        double m2 = m[channel].b;
        double m3 = m[channel].c;

        for (t = 0; t < 2; t++) {
            double top1 = (284517.0 * m1 - 94839.0 * m3) * sub2;
            double top2 = (838422.0 * m3 + 769860.0 * m2 + 731718.0 * m1) * l * sub2 -  769860.0 * t * l;
            double bottom = (632260.0 * m3 - 126452.0 * m2) * sub2 + 126452.0 * t;

            bounds[channel * 2 + t].a = top1 / bottom;
            bounds[channel * 2 + t].b = top2 / bottom;
        }
    }
}

static double
ray_length_until_intersect(double theta, const Bounds* line)
{
    return line->b / (sin(theta) - line->a * cos(theta));
}

static double
max_chroma_for_lh(double l, double h)
{
    double min_len = DBL_MAX;
    double hrad = h * 0.01745329251994329577; /* (2 * pi / 360) */
    Bounds bounds[6];
    int i;

    get_bounds(l, bounds);
    for(i = 0; i < 6; i++) {
        double len = ray_length_until_intersect(hrad, &bounds[i]);

        if(len >= 0  &&  len < min_len)
            min_len = len;
    }
    return min_len;
}

static double
dot_product(const Triplet* t1, const Triplet* t2)
{
    return (t1->a * t2->a + t1->b * t2->b + t1->c * t2->c);
}

/* Used for rgb conversions */
static double
from_linear(double c)
{
    if(c <= 0.0031308)
        return 12.92 * c;
    else
        return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

static void
xyz2rgb(Triplet* in_out)
{
    double r = from_linear(dot_product(&m[0], in_out));
    double g = from_linear(dot_product(&m[1], in_out));
    double b = from_linear(dot_product(&m[2], in_out));
    in_out->a = r;
    in_out->b = g;
    in_out->c = b;
}

/* https://en.wikipedia.org/wiki/CIELUV
 * In these formulas, Yn refers to the reference white point. We are using
 * illuminant D65, so Yn (see refY in Maxima file) equals 1. The formula is
 * simplified accordingly.
 */
static double
l2y(double l)
{
    if(l <= 8.0) {
        return l / kappa;
    } else {
        double x = (l + 16.0) / 116.0;
        return (x * x * x);
    }
}

static void
luv2xyz(Triplet* in_out)
{
    if(in_out->a <= 0.00000001) {
        /* Black will create a divide-by-zero error. */
        in_out->a = 0.0;
        in_out->b = 0.0;
        in_out->c = 0.0;
        return;
    }

    double var_u = in_out->b / (13.0 * in_out->a) + ref_u;
    double var_v = in_out->c / (13.0 * in_out->a) + ref_v;
    double y = l2y(in_out->a);
    double x = -(9.0 * y * var_u) / ((var_u - 4.0) * var_v - var_u * var_v);
    double z = (9.0 * y - (15.0 * var_v * y) - (var_v * x)) / (3.0 * var_v);
    in_out->a = x;
    in_out->b = y;
    in_out->c = z;
}

static void
lch2luv(Triplet* in_out)
{
    double hrad = in_out->c * 0.01745329251994329577;  /* (pi / 180.0) */
    double u = cos(hrad) * in_out->b;
    double v = sin(hrad) * in_out->b;

    in_out->b = u;
    in_out->c = v;
}

static void
hsluv2lch(Triplet* in_out)
{
    double h = in_out->a;
    double s = in_out->b;
    double l = in_out->c;
    double c;

    /* White and black: disambiguate chroma */
    if(l > 99.9999999 || l < 0.00000001)
        c = 0.0;
    else
        c = max_chroma_for_lh(l, h) / 100.0 * s;

    /* Grays: disambiguate hue */
    if (s < 0.00000001)
        h = 0.0;

    in_out->a = l;
    in_out->b = c;
    in_out->c = h;
}


void
hsluv2rgb(double h, double s, double l, rgb_color* out)
{
    Triplet tmp = { h, s, l };

    hsluv2lch(&tmp);
    lch2luv(&tmp);
    luv2xyz(&tmp);
    xyz2rgb(&tmp);

    out->red = tmp.a * 255;
    out->green = tmp.b * 255;
    out->blue = tmp.c * 255;
    out->alpha = 255;
}
