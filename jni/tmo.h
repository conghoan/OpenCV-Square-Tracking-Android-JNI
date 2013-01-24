//#ifndef __TMO_H__
//#define __TMO_H__
//#include "tmo.h"
//#include <omp.h>

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
using namespace std;

//#include <cv.h>
//#include <cxcore.h>
//#include <highgui.h>
//using namespace cv;


void makehdr3(Mat* im1, Mat* im2, Mat* im3, Mat* hdr) {

    Scalar s1 = mean(*im1); // dark
    Scalar s2 = mean(*im2); // med
    Scalar s3 = mean(*im3); // light

    float mean1 = (s1[1] + s1[2] + s1[3]) / 3;
    float mean2 = (s2[1] + s2[2] + s2[3]) / 3;
    float mean3 = (s3[1] + s3[2] + s3[3]) / 3;

    float r1 = mean1 / mean3 ;
    float r2 = 1  ;
    float r3 = mean3 / mean1 ;

    // add
    *hdr = (*im1 / r1 + *im2 / r2 + *im3 / r3) ;
}

void makehdr2(Mat* im1, Mat* im3, Mat* hdr) {

    Scalar s1 = mean(*im1); // dark
    Scalar s3 = mean(*im3); // light

    float mean1 = (s1[1] + s1[2] + s1[3]) / 3;
    float mean3 = (s3[1] + s3[2] + s3[3]) / 3;

    float r1 = mean1 / mean3;
    float r3 = mean3 / mean1;

    // add
    *hdr = (*im1 / r1 + *im3 / r3) ;
}

void makehdr3log(Mat* im1, Mat* im2, Mat* im3, Mat* hdr) {

    (*im1).convertTo(*im1, CV_32FC3);
    (*im2).convertTo(*im2, CV_32FC3);
    (*im3).convertTo(*im3, CV_32FC3);

    *im1 += .01;
    *im2 += .01;
    *im3 += .01;

    Mat temp1((*im1).reshape(1, hdr->rows));
    Mat temp2((*im2).reshape(1, hdr->rows));
    Mat temp3((*im3).reshape(1, hdr->rows));

    cv::log(temp1, temp1);
    cv::log(temp2, temp2);
    cv::log(temp3, temp3);

    temp1 = temp1.reshape(3, hdr->rows);
    temp2 = temp2.reshape(3, hdr->rows);
    temp3 = temp3.reshape(3, hdr->rows);

    *hdr = (temp1 + temp2 + temp3) / 3;
}

void makehdr2log(Mat* im1, Mat* im3, Mat* hdr) {

    (*im1).convertTo(*im1, CV_32FC3);
    (*im3).convertTo(*im3, CV_32FC3);

    *im1 += .01;
    *im3 += .01;

    Mat temp1((*im1).reshape(1, hdr->rows));
    Mat temp3((*im3).reshape(1, hdr->rows));

    cv::log(temp1, temp1);
    cv::log(temp3, temp3);

    temp1 = temp1.reshape(3, hdr->rows);
    temp3 = temp3.reshape(3, hdr->rows);

    *hdr = (temp1  + temp3) / 2;
}



/**
 * Modified by Chris McClanahan for Android JNI
 *
 * @Brief Contrast mapping TMO
 *
 * From:
 *
 * Rafal Mantiuk, Karol Myszkowski, Hans-Peter Seidel.
 * A Perceptual Framework for Contrast Processing of High Dynamic Range Images
 * In: ACM Transactions on Applied Perception 3 (3), pp. 286-308, 2006
 * http://www.mpi-inf.mpg.de/~mantiuk/contrast_domain/
 *
 * This file is a part of LuminanceHDR package, based on pfstmo.
 * ----------------------------------------------------------------------
 * Copyright (C) 2007 Grzegorz Krawczyk
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * ----------------------------------------------------------------------
 *
 * @author Radoslaw Mantiuk, <radoslaw.mantiuk@gmail.com>
 * @author Rafal Mantiuk, <mantiuk@gmail.com>
 * Updated 2007/12/17 by Ed Brambley <E.J.Brambley@damtp.cam.ac.uk>
 *  (more information on the changes:
 *  http://www.damtp.cam.ac.uk/user/ejb48/hdr/index.html)
 * Updated 2008/06/25 by Ed Brambley <E.J.Brambley@damtp.cam.ac.uk>
 *  bug fixes and openMP patches
 *  more on this:
 *  http://groups.google.com/group/pfstools/browse_thread/thread/de2378af98ec6185/0dee5304fc14e99d?hl=en#0dee5304fc14e99d
 *  Optimization improvements by Lebed Dmytry
 *
 * Updated 2008/07/26 by Dejan Beric <dejan.beric@live.com>
 *  Added the detail factor slider which offers more control over contrast in details
 * Update 2010/10/06 by Axel Voitier <axel.voitier@gmail.com>
 *  detail_factor patch in order to remove potential issues in a multithreading environment
 * @author Davide Anastasia <davideanastasia@users.sourceforge.net>
 *  Improvement & Clean up
 * @author Bruce Guenter <bruce@untroubled.org>
 *  Added trivial downsample and upsample functions when both dimension are even
 *
 * $Id: contrast_domain.cpp,v 1.14 2008/08/26 17:08:49 rafm Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


typedef struct pyramid_s {
    int rows;
    int cols;
    float* Gx;
    float* Gy;
    struct pyramid_s* next;
    struct pyramid_s* prev;
} pyramid_t;

#define PYRAMID_MIN_PIXELS      3
#define LOOKUP_W_TO_R           107
//#define DEBUG_MANTIUK06

void contrast_equalization(pyramid_t* pp, const float contrastFactor);

void transform_to_luminance(pyramid_t* pyramid, float* const x, const bool bcg);
void matrix_add(const int n, const float* const a, float* const b);
void matrix_subtract(const int n, const float* const a, float* const b);
void matrix_copy(const int n, const float* const a, float* const b);
void matrix_multiply_const(const int n, float* const a, const float val);
void matrix_divide(const int n, const float* a, float* b);
float* matrix_alloc(const int size);
void matrix_free(float* m);
float matrix_DotProduct(const int n, const float* const a, const float* const b);
void matrix_zero(const int n, float* const m);
void calculate_and_add_divergence(const int rows, const int cols, const float* const Gx, const float*  Gy, float*  divG);
void pyramid_calculate_divergence(pyramid_t* pyramid);
void pyramid_calculate_divergence_sum(pyramid_t* pyramid, float* divG_sum);
void calculate_scale_factor(const int n, const float* const G, float* const C);
void pyramid_calculate_scale_factor(pyramid_t* pyramid, pyramid_t* pC);
void scale_gradient(const int n, float* G, const float* C);
void pyramid_scale_gradient(pyramid_t* pyramid, pyramid_t* pC);
void pyramid_free(pyramid_t* pyramid);
pyramid_t* pyramid_allocate(const int cols, const int rows);
void calculate_gradient(const int cols, const int rows, const float* const lum, float* const Gx, float* const Gy);
void pyramid_calculate_gradient(const pyramid_t* pyramid, const float* lum);
void solveX(const int n, const float* const b, float* const x);
void multiplyA(pyramid_t* px, pyramid_t* pyramid, const float* const x, float* const divG_sum);
void linbcg(pyramid_t* pyramid, pyramid_t* pC, const float* const b, float* const x, const int itmax, const float tol);
void lincg(pyramid_t* pyramid, pyramid_t* pC, const float* const b, float* const x, const int itmax, const float tol);
float lookup_table(const int n, const float* const in_tab, const float* const out_tab, const float val);
void transform_to_R(const int n, float* const G, float detail_factor);
void pyramid_transform_to_R(pyramid_t* pyramid, float detail_factor);
void transform_to_G(const int n, float* const R, float detail_factor);
void pyramid_transform_to_G(pyramid_t* pyramid, float detail_factor);
void pyramid_gradient_multiply(pyramid_t* pyramid, const float val);

void swap_pointers(float* &pOne, float* &pTwo); // utility function

void dump_matrix_to_file(const int width, const int height, const float* const m, const char* const file_name);
void matrix_show(const char* const text, int rows, int cols, const float* const data);
void pyramid_show(pyramid_t* pyramid);


static float W_table[] = {0.000000f, 0.010000f, 0.021180f, 0.031830f, 0.042628f, 0.053819f, 0.065556f, 0.077960f, 0.091140f, 0.105203f, 0.120255f, 0.136410f, 0.153788f, 0.172518f, 0.192739f, 0.214605f, 0.238282f, 0.263952f, 0.291817f, 0.322099f, 0.355040f, 0.390911f, 0.430009f, 0.472663f, 0.519238f, 0.570138f, 0.625811f, 0.686754f, 0.753519f, 0.826720f, 0.907041f, 0.995242f, 1.092169f, 1.198767f, 1.316090f, 1.445315f, 1.587756f, 1.744884f, 1.918345f, 2.109983f, 2.321863f, 2.556306f, 2.815914f, 3.103613f, 3.422694f, 3.776862f, 4.170291f, 4.607686f, 5.094361f, 5.636316f, 6.240338f, 6.914106f, 7.666321f, 8.506849f, 9.446889f, 10.499164f, 11.678143f, 13.000302f, 14.484414f, 16.151900f, 18.027221f, 20.138345f, 22.517282f, 25.200713f, 28.230715f, 31.655611f, 35.530967f, 39.920749f, 44.898685f, 50.549857f, 56.972578f, 64.280589f, 72.605654f, 82.100619f, 92.943020f, 105.339358f, 119.530154f, 135.795960f, 154.464484f, 175.919088f, 200.608905f, 229.060934f, 261.894494f, 299.838552f, 343.752526f, 394.651294f, 453.735325f, 522.427053f, 602.414859f, 695.706358f, 804.693100f, 932.229271f, 1081.727632f, 1257.276717f, 1463.784297f, 1707.153398f, 1994.498731f, 2334.413424f, 2737.298517f, 3215.770944f, 3785.169959f, 4464.187290f, 5275.653272f, 6247.520102f, 7414.094945f, 8817.590551f, 10510.080619f};
static float R_table[] = {0.000000f, 0.009434f, 0.018868f, 0.028302f, 0.037736f, 0.047170f, 0.056604f, 0.066038f, 0.075472f, 0.084906f, 0.094340f, 0.103774f, 0.113208f, 0.122642f, 0.132075f, 0.141509f, 0.150943f, 0.160377f, 0.169811f, 0.179245f, 0.188679f, 0.198113f, 0.207547f, 0.216981f, 0.226415f, 0.235849f, 0.245283f, 0.254717f, 0.264151f, 0.273585f, 0.283019f, 0.292453f, 0.301887f, 0.311321f, 0.320755f, 0.330189f, 0.339623f, 0.349057f, 0.358491f, 0.367925f, 0.377358f, 0.386792f, 0.396226f, 0.405660f, 0.415094f, 0.424528f, 0.433962f, 0.443396f, 0.452830f, 0.462264f, 0.471698f, 0.481132f, 0.490566f, 0.500000f, 0.509434f, 0.518868f, 0.528302f, 0.537736f, 0.547170f, 0.556604f, 0.566038f, 0.575472f, 0.584906f, 0.594340f, 0.603774f, 0.613208f, 0.622642f, 0.632075f, 0.641509f, 0.650943f, 0.660377f, 0.669811f, 0.679245f, 0.688679f, 0.698113f, 0.707547f, 0.716981f, 0.726415f, 0.735849f, 0.745283f, 0.754717f, 0.764151f, 0.773585f, 0.783019f, 0.792453f, 0.801887f, 0.811321f, 0.820755f, 0.830189f, 0.839623f, 0.849057f, 0.858491f, 0.867925f, 0.877358f, 0.886792f, 0.896226f, 0.905660f, 0.915094f, 0.924528f, 0.933962f, 0.943396f, 0.952830f, 0.962264f, 0.971698f, 0.981132f, 0.990566f, 1.000000f};

inline int imin(int a, int b) {
    return a < b ? a : b;
}

inline float max(float a, float b) {
    return a > b ? a : b;
}

inline float min(float a, float b) {
    return a < b ? a : b;
}

// upsample the matrix
// upsampled matrix is twice bigger in each direction than data[]
// res should be a pointer to allocated memory for bigger matrix
// cols and rows are the dimensions of the output matrix
void matrix_upsample_full(const int outCols, const int outRows, const float* const in, float* const out) {
    const int inRows = outRows / 2;
    const int inCols = outCols / 2;

    // Transpose of experimental downsampling matrix (theoretically the correct thing to do)

    const float dx = (float)inCols / ((float)outCols);
    const float dy = (float)inRows / ((float)outRows);
    const float factor = 1.0f / (dx * dy); // This gives a genuine upsampling matrix, not the transpose of the downsampling matrix
    // const float factor = 1.0f; // Theoretically, this should be the best.

    //#pragma omp parallel for schedule(static)
    for (int y = 0; y < outRows; y++) {
        const float sy = y * dy;
        const int iy1 = (y   * inRows) / outRows;
        const int iy2 = imin(((y + 1) * inRows) / outRows, inRows - 1);

        for (int x = 0; x < outCols; x++) {
            const float sx = x * dx;
            const int ix1 = (x   * inCols) / outCols;
            const int ix2 = imin(((x + 1) * inCols) / outCols, inCols - 1);

            out[x + y* outCols] = (((ix1 + 1) - sx) * ((iy1 + 1 - sy)) * in[ix1 + iy1 * inCols] +
                                   ((ix1 + 1) - sx) * (sy + dy - (iy1 + 1)) * in[ix1 + iy2 * inCols] +
                                   (sx + dx - (ix1 + 1)) * ((iy1 + 1 - sy)) * in[ix2 + iy1 * inCols] +
                                   (sx + dx - (ix1 + 1)) * (sy + dx - (iy1 + 1)) * in[ix2 + iy2 * inCols]) * factor;
        }
    }
}

void matrix_upsample_simple(const int outCols, const int outRows, const float* const in, float* const out) {
    //#pragma omp parallel for schedule(static)
    for (int y = 0; y < outRows; y++) {
        const int iy1 = y / 2;
        float* outp = out + y * outCols;
        const float* inp = in + iy1 * (outCols / 2);
        for (int x = 0; x < outCols; x += 2) {
            const int ix1 = x / 2;
            outp[x] = outp[x + 1] = inp[ix1];
        }
    }
}

void matrix_upsample(const int outCols, const int outRows, const float* const in, float* const out) {
    if (outRows % 2 == 0 && outCols % 2 == 0)
        { matrix_upsample_simple(outCols, outRows, in, out); }
    else
        { matrix_upsample_full(outCols, outRows, in, out); }
}

// downsample the matrix
void matrix_downsample_full(const int inCols, const int inRows, const float*   data, float*   res) {
    const int outRows = inRows / 2;
    const int outCols = inCols / 2;
    const float dx = (float)inCols / ((float)outCols);
    const float dy = (float)inRows / ((float)outRows);

    // New downsampling by Ed Brambley:
    // Experimental downsampling that assumes pixels are square and
    // integrates over each new pixel to find the average value of the
    // underlying pixels.
    //
    // Consider the original pixels laid out, and the new (larger)
    // pixels layed out over the top of them.  Then the new value for
    // the larger pixels is just the integral over that pixel of what
    // shows through; i.e., the values of the pixels underneath
    // multiplied by how much of that pixel is showing.
    //
    // (ix1, iy1) is the coordinate of the top left visible pixel.
    // (ix2, iy2) is the coordinate of the bottom right visible pixel.
    // (fx1, fy1) is the fraction of the top left pixel showing.
    // (fx2, fy2) is the fraction of the bottom right pixel showing.

    const float normalize = 1.0f / (dx * dy);
    //#pragma omp parallel for schedule(static)
    for (int y = 0; y < outRows; y++) {
        const int iy1 = (y   * inRows) / outRows;
        const int iy2 = ((y + 1) * inRows) / outRows;
        const float fy1 = (iy1 + 1) - y * dy;
        const float fy2 = (y + 1) * dy - iy2;

        for (int x = 0; x < outCols; ++x) {
            const int ix1 = (x   * inCols) / outCols;
            const int ix2 = ((x + 1) * inCols) / outCols;
            const float fx1 = (ix1 + 1) - x * dx;
            const float fx2 = (x + 1) * dx - ix2;

            float pixVal = 0.0f;
            float factorx, factory;
            for (int i = iy1; i <= iy2 && i < inRows; i++) {
                if (i == iy1)
                    { factory = fy1; }  // We're just getting the bottom edge of this pixel
                else if (i == iy2)
                    { factory = fy2; }  // We're just gettting the top edge of this pixel
                else
                    { factory = 1.0f; } // We've got the full height of this pixel
                for (int j = ix1; j <= ix2 && j < inCols; j++) {
                    if (j == ix1)
                        { factorx = fx1; }  // We've just got the right edge of this pixel
                    else if (j == ix2)
                        { factorx = fx2; } // We've just got the left edge of this pixel
                    else
                        { factorx = 1.0f; } // We've got the full width of this pixel

                    pixVal += data[j + i * inCols] * factorx * factory;
                }
            }

            res[x + y* outCols] = pixVal * normalize;   // Normalize by the area of the new pixel
        }
    }
}

void matrix_downsample_simple(const int inCols, const int inRows, const float* const data, float* const res) {
    const int outRows = inRows / 2;
    const int outCols = inCols / 2;

    // Simplified downsampling by Bruce Guenter:
    //
    // Follows exactly the same math as the full downsampling above,
    // except that inRows and inCols are known to be even.  This allows
    // for all of the boundary cases to be eliminated, reducing the
    // sampling to a simple average.

    //#pragma omp parallel for schedule(static)
    for (int y = 0; y < outRows; y++) {
        const int iy1 = y * 2;
        const float* datap = data + iy1 * inCols;
        float* resp = res + y * outCols;

        for (int x = 0; x < outCols; x++) {
            const int ix1 = x * 2;

            resp[x] = (datap[ix1]
                       + datap[ix1 + 1]
                       + datap[ix1   + inCols]
                       + datap[ix1 + 1 + inCols]) / 4.0f;
        }
    }
}

void matrix_downsample(const int inCols, const int inRows, const float* const data, float* const res) {
    if (inCols % 2 == 0 && inRows % 2 == 0)
        { matrix_downsample_simple(inCols, inRows, data, res); }
    else
        { matrix_downsample_full(inCols, inRows, data, res); }
}

// return = a + b
inline void matrix_add(const int n, const float* const a, float* const b) {
    //#pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        b[i] += a[i];
    }
}

// return = a - b
inline void matrix_subtract(const int n, const float* const a, float* const b) {
    //#pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        b[i] = a[i] - b[i];
    }
}

// copy matix a to b, return = a
inline void matrix_copy(const int n, const float* const a, float* const b) {
    memcpy(b, a, sizeof(float)*n);
}

// multiply matrix a by scalar val
inline void matrix_multiply_const(const int n, float* const a, const float val) {
    //#pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        a[i] *= val;
    }
}

// b = a[i] / b[i]
inline void matrix_divide(const int n, float* a, float* b) {
    //#pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        b[i] = a[i] / b[i];
    }
}

// alloc memory for the float table
inline float* matrix_alloc(int size) {
    float* m  = (float*)malloc(sizeof(float) * size);
    if (m == NULL) {
        fprintf(stderr, "ERROR: malloc in matrix_alloc() (size:%d)", size);
        exit((int)155);
    }
    return m;
}

// free memory for matrix
inline void matrix_free(float* m) {
    if (m != NULL) {
        free(m);
        //_mm_free(m);
        m = NULL;
    } else {
        fprintf(stderr, "ERROR: This pointer has already been freed");
    }
}

// multiply vector by vector (each vector should have one dimension equal to 1)
float matrix_DotProduct(const int n, const float*   a, const float*   b) {
    float val = 0;
    //#pragma omp parallel for reduction(+:val) schedule(static)
    for (int j = 0; j < n; ++j) {
        val += a[j] * b[j];
    }
    return val;
}

// set zeros for matrix elements
inline void matrix_zero(int n, float* m) {
    //bzero(m, n*sizeof(float));
    memset(m, 0, n * sizeof(float));
}

// Davide Anastasia <davideanastasia@users.sourceforge.net> (2010 08 31)
// calculate divergence of two gradient maps (Gx and Gy)
// divG(x,y) = Gx(x,y) - Gx(x-1,y) + Gy(x,y) - Gy(x,y-1)
void calculate_and_add_divergence(const int COLS, const int ROWS, const float*   Gx, const float*   Gy, float*   divG) {
    float divGx, divGy;
    //#pragma omp parallel sections private(divGx, divGy)
    {
        //#pragma omp section
        {
            // kx = 0 AND ky = 0;
            divG[0] += Gx[0] + Gy[0];                       // OUT

            // ky = 0
            for (int kx = 1; kx < COLS; kx++) {
                divGx = Gx[kx] - Gx[kx - 1];
                divGy = Gy[kx];
                divG[kx] += divGx + divGy;                    // OUT
            }
        }
        //#pragma omp section
        {
            //#pragma omp parallel for schedule(static, 5120) private(divGx, divGy)
            for (int ky = 1; ky < ROWS; ky++) {
                // kx = 0
                divGx = Gx[ky * COLS];
                divGy = Gy[ky * COLS] - Gy[ky * COLS - COLS];
                divG[ky* COLS] += divGx + divGy;              // OUT

                // kx > 0
                for (int kx = 1; kx < COLS; kx++) {
                    divGx = Gx[kx + ky * COLS] - Gx[kx + ky * COLS - 1];
                    divGy = Gy[kx + ky * COLS] - Gy[kx + ky * COLS - COLS];
                    divG[kx + ky* COLS] += divGx + divGy;       // OUT
                }
            }
        }
    }   // END PARALLEL SECTIONS
}

// Calculate the sum of divergences for the all pyramid level
// The smaller divergence map is upsampled and added to the divergence map for the higher level of pyramid
void pyramid_calculate_divergence_sum(pyramid_t* pyramid, float* divG_sum) {
    float* temp = matrix_alloc((pyramid->rows * pyramid->cols) / 4);

    // Find the coarsest pyramid, and the number of pyramid levels
    bool swap = true;
    while (pyramid->next != NULL) {
        swap = (!swap);
        pyramid = pyramid->next;
    }

    // For every level, we swap temp and divG_sum
    // So, if there are an odd number of levels...
    if (swap) { swap_pointers(divG_sum, temp); }

    if (pyramid) {
        matrix_zero(pyramid->rows * pyramid->cols, temp);
        calculate_and_add_divergence(pyramid->cols, pyramid->rows, pyramid->Gx, pyramid->Gy, temp);

        swap_pointers(divG_sum, temp);
        pyramid = pyramid->prev;
    }

    while (pyramid) {
        matrix_upsample(pyramid->cols, pyramid->rows, divG_sum, temp);
        calculate_and_add_divergence(pyramid->cols, pyramid->rows, pyramid->Gx, pyramid->Gy, temp);

        swap_pointers(divG_sum, temp);
        pyramid = pyramid->prev;
    }

    matrix_free(temp);
}

// calculate scale factors (Cx,Cy) for gradients (Gx,Gy)
// C is equal to EDGE_WEIGHT for gradients smaller than GFIXATE or 1.0 otherwise
inline void calculate_scale_factor(const int n, const float* const G, float* const C) {
    //  float GFIXATE = 0.1f;
    //  float EDGE_WEIGHT = 0.01f;
    const float detectT = 0.001f;
    const float a = 0.038737f;
    const float b = 0.537756f;

    //#pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        //#if 1
        const float g = max(detectT, fabsf(G[i]));
        C[i] = 1.0f / (a * powf(g, b));
        //#else
        //    if(fabsf(G[i]) < GFIXATE)
        //      C[i] = 1.0f / EDGE_WEIGHT;
        //    else
        //      C[i] = 1.0f;
        //#endif
    }
}

// calculate scale factor for the whole pyramid
void pyramid_calculate_scale_factor(pyramid_t* pyramid, pyramid_t* pC) {
    while (pyramid != NULL) {
        calculate_scale_factor(pyramid->rows * pyramid->cols, pyramid->Gx, pC->Gx);
        calculate_scale_factor(pyramid->rows * pyramid->cols, pyramid->Gy, pC->Gy);

        pyramid = pyramid->next;
        pC = pC->next;
    }
}

// Scale gradient (Gx and Gy) by C (Cx and Cy)
// G = G * C
inline void scale_gradient(const int n, float* G, const float* C) {
    //#pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        G[i] *= C[i];
    }
}

// scale gradients for the whole one pyramid with the use of (Cx,Cy) from the other pyramid
void pyramid_scale_gradient(pyramid_t* pyramid, pyramid_t* pC) {
    while (pyramid != NULL) {
        scale_gradient(pyramid->rows * pyramid->cols, pyramid->Gx, pC->Gx);
        scale_gradient(pyramid->rows * pyramid->cols, pyramid->Gy, pC->Gy);

        pyramid = pyramid->next;
        pC = pC->next;
    }
}


// free memory allocated for the pyramid
void pyramid_free(pyramid_t* pyramid) {
    pyramid_t* t_next; // = pyramid->next;

    while (pyramid) {
        t_next = pyramid->next;

        if (pyramid->Gx != NULL) {
            matrix_free(pyramid->Gx);   //free(pyramid->Gx);
            pyramid->Gx = NULL;
        }
        if (pyramid->Gy != NULL) {
            matrix_free(pyramid->Gy);   //free(pyramid->Gy);
            pyramid->Gy = NULL;
        }

        //pyramid->prev = NULL;
        //pyramid->next = NULL;

        free(pyramid);
        pyramid = t_next;
    }
}


// allocate memory for the pyramid
pyramid_t* pyramid_allocate(int cols, int rows) {
    pyramid_t* level = NULL;
    pyramid_t* pyramid = NULL;
    pyramid_t* prev = NULL;

    while (rows >= PYRAMID_MIN_PIXELS && cols >= PYRAMID_MIN_PIXELS) {
        level = (pyramid_t*) malloc(sizeof(pyramid_t));
        if (level == NULL) {
            fprintf(stderr, "ERROR: malloc in pyramid_alloc() (size:%zu)", sizeof(pyramid_t));
            exit((int)155);
        }
        memset(level, 0, sizeof(pyramid_t));

        level->rows = rows;
        level->cols = cols;
        const int size = level->rows * level->cols;
        level->Gx = matrix_alloc(size);
        level->Gy = matrix_alloc(size);

        level->prev = prev;
        if (prev != NULL)
            { prev->next = level; }
        prev = level;

        if (pyramid == NULL)
            { pyramid = level; }

        rows /= 2;
        cols /= 2;
    }

    return pyramid;
}


// calculate gradients
//TODO: check this implementation in Linux, where the OMP is enabled!
inline void calculate_gradient(const int COLS, const int ROWS, const float* const lum, float* const Gx, float* const Gy) {
    int Y_IDX, IDX;

    //#pragma omp parallel for schedule(static) private(Y_IDX, IDX)
    for (int ky = 0; ky < ROWS - 1; ky++) {
        Y_IDX = ky * COLS;
        for (int kx = 0; kx < COLS - 1; kx++) {
            IDX = Y_IDX + kx;

            Gx[IDX] = lum[IDX + 1]    - lum[IDX];
            Gy[IDX] = lum[IDX + COLS] - lum[IDX];
        }

        Gx[Y_IDX + COLS - 1] = 0.0f; // last columns (kx = COLS - 1)
        Gy[Y_IDX + COLS - 1] = lum[Y_IDX + COLS - 1 + COLS] - lum[Y_IDX + COLS - 1];
    }

    // last row (ky = ROWS-1)
    for (int kx = 0; kx < (COLS - 1); kx++) {
        IDX = (ROWS - 1) * COLS + kx;

        Gx[IDX] = lum[IDX + 1] - lum[IDX];
        Gy[IDX] = 0.0f;
    }

    // last row & last col = last element
    Gx[ROWS* COLS - 1] = 0.0f;
    Gy[ROWS* COLS - 1] = 0.0f;
}

void swap_pointers(float* &pOne, float* &pTwo) {
    float* pTemp = pOne;
    pOne = pTwo;
    pTwo = pTemp;
}

// calculate gradients for the pyramid
// lum_temp WILL NOT BE overwritten!
void pyramid_calculate_gradient(const pyramid_t* pyramid, const float* Y /*lum_temp*/) {
    float* buffer1 = matrix_alloc((pyramid->rows * pyramid->cols) / 4); // /4
    float* buffer2 = matrix_alloc((pyramid->rows * pyramid->cols) / 16); // /16

    float* p_t1 = buffer1;
    float* p_t2 = buffer2;

    calculate_gradient(pyramid->cols, pyramid->rows, Y, pyramid->Gx, pyramid->Gy);

    pyramid_t* py_curr = pyramid->next;
    pyramid_t* py_prev = py_curr->prev;

    if (py_curr) {
        matrix_downsample(py_prev->cols, py_prev->rows, Y, p_t1);
        calculate_gradient(py_curr->cols, py_curr->rows, p_t1, py_curr->Gx, py_curr->Gy);

        py_prev = py_curr;
        py_curr = py_curr->next;
    }

    while (py_curr) {
        matrix_downsample(py_prev->cols, py_prev->rows, p_t1, p_t2);
        calculate_gradient(py_curr->cols, py_curr->rows, p_t2, py_curr->Gx, py_curr->Gy);

        // swap pointers
        swap_pointers(p_t1, p_t2);

        py_prev = py_curr;
        py_curr = py_curr->next;
    }

    matrix_free(buffer1);
    matrix_free(buffer2);
}

// x = -0.25 * b
inline void solveX(const int n, const float* const b, float* const x) {
    //#pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        x[i] = (-0.25f) * b[i];
    }
}

// divG_sum = A * x = sum(divG(x))
inline void multiplyA(pyramid_t* px, pyramid_t* pC, const float* const x, float* divG_sum) {
    pyramid_calculate_gradient(px, x);                // x won't be changed
    pyramid_scale_gradient(px, pC);                   // scale gradients by Cx,Cy from main pyramid
    pyramid_calculate_divergence_sum(px, divG_sum);   // calculate the sum of divergences
}


// bi-conjugate linear equation solver
// overwrites pyramid!
void linbcg(pyramid_t* pyramid, pyramid_t* pC, float*   b, float*   x, const int itmax, const float tol) {
    const int rows = pyramid->rows;
    const int cols = pyramid->cols;
    const int n = rows * cols;
    const float tol2 = tol * tol;

    float* const z = matrix_alloc(n);
    float* const zz = matrix_alloc(n);
    float* const p = matrix_alloc(n);
    float* const pp = matrix_alloc(n);
    float* const r = matrix_alloc(n);
    float* const rr = matrix_alloc(n);
    float* const x_save = matrix_alloc(n);

    const float bnrm2 = matrix_DotProduct(n, b, b);
    multiplyA(pyramid, pC, x, r); // r = A*x = divergence(x)
    matrix_subtract(n, b, r); // r = b - r
    float err2 = matrix_DotProduct(n, r, r); // err2 = r.r
    multiplyA(pyramid, pC, r, rr); // rr = A*r

    float bkden = 0;
    float saved_err2 = err2;
    matrix_copy(n, x, x_save);

    // const float ierr2 = err2;
    // const float percent_sf = 100.0f / logf(tol2 * bnrm2 / ierr2);

    int iter = 0;
    bool reset = true;
    int num_backwards = 0;
    const int num_backwards_ceiling = 3;
    for (; iter < itmax; ++iter) {
        // ph->newValue( (int) (logf(err2 / ierr2)*percent_sf) );
        // if (ph->isTerminationRequested()) { //user request abort
        // 	break;
        // }

        solveX(n, r, z);   //  z = ~A(-1) *  r = -0.25 *  r
        solveX(n, rr, zz); // zz = ~A(-1) * rr = -0.25 * rr

        const float bknum = matrix_DotProduct(n, z, rr);

        if (reset) {
            reset = false;
            matrix_copy(n, z, p);
            matrix_copy(n, zz, pp);
        } else {
            const float bk = bknum / bkden; // beta = ...
            //#pragma omp parallel for schedule(static)
            for (int i = 0; i < n; i++) {
                p[i]  =  z[i] + bk *  p[i];
                pp[i] = zz[i] + bk * pp[i];
            }
        }

        bkden = bknum; // numerato becomes the dominator for the next iteration

        // slow!
        multiplyA(pyramid, pC,  p,  z); //  z = A* p = divergence( p)
        multiplyA(pyramid, pC, pp, zz); // zz = A*pp = divergence(pp)

        const float ak = bknum / matrix_DotProduct(n, z, pp); // alfa = ...

        //#pragma omp parallel for schedule(static)
        for (int i = 0 ; i < n ; i++) {
            r[i]  -= ak *  z[i];	// r =  r - alfa * z
            rr[i] -= ak * zz[i];	//rr = rr - alfa * zz
        }

        const float old_err2 = err2;
        err2 = matrix_DotProduct(n, r, r);

        // Have we gone unstable?
        if (err2 > old_err2) {
            // Save where we've got to if it's the best yet
            if (num_backwards == 0 && old_err2 < saved_err2) {
                saved_err2 = old_err2;
                matrix_copy(n, x, x_save);
            }

            num_backwards++;
        } else {
            num_backwards = 0;
        }

        //#pragma omp parallel for schedule(static)
        for (int i = 0 ; i < n ; i++) {
            x[i] += ak * p[i];	// x =  x + alfa * p
        }

        if (num_backwards > num_backwards_ceiling) {
            // Reset
            reset = true;
            num_backwards = 0;

            // Recover saved value
            matrix_copy(n, x_save, x);

            // r = Ax
            multiplyA(pyramid, pC, x, r);

            // r = b - r
            matrix_subtract(n, b, r);

            // err2 = r.r
            err2 = matrix_DotProduct(n, r, r);
            saved_err2 = err2;

            // rr = A*r
            multiplyA(pyramid, pC, r, rr);
        }

        // fprintf(stderr, "iter:%d err:%f\n", iter+1, sqrtf(err2/bnrm2));
        if (err2 / bnrm2 < tol2) {
            break;
        }
    }

    // Use the best version we found
    if (err2 > saved_err2) {
        err2 = saved_err2;
        matrix_copy(n, x_save, x);
    }

    if (err2 / bnrm2 > tol2) {
        // Not converged
        // ph->newValue( (int) (logf(err2 / ierr2)*percent_sf));
        if (iter == itmax) {
            MSG(" Warning: Not converged (hit maximum iterations), error = %f (should be below %f).\n", sqrtf(err2 / bnrm2), tol);
        } else {
            MSG(" Warning: Not converged (going unstable), error = %f (should be below %f).\n", sqrtf(err2 / bnrm2), tol);
        }
    } else {
        // ph->newValue(100);
    }


    matrix_free(x_save);
    matrix_free(p);
    matrix_free(pp);
    matrix_free(z);
    matrix_free(zz);
    matrix_free(r);
    matrix_free(rr);
}


// conjugate linear equation solver
// overwrites pyramid!
// This version is a slightly modified version by Davide Anastasia <davideanastasia@users.sourceforge.net>
// March 25, 2011
void lincg(pyramid_t* pyramid, pyramid_t* pC, const float* const b, float* const x, const int itmax, const float tol) {
    const int num_backwards_ceiling = 3;

    float rdotr_curr, rdotr_prev, rdotr_best;
    float alpha, beta;

#ifdef TIMER_PROFILING
    msec_timer f_timer;
    f_timer.start();
#endif

    const int rows = pyramid->rows;
    const int cols = pyramid->cols;
    const int n = rows * cols;
    const float tol2 = tol * tol;

    float* const x_best = matrix_alloc(n);
    float* const r = matrix_alloc(n);
    float* const p = matrix_alloc(n);
    float* const Ap = matrix_alloc(n);

    // bnrm2 = ||b||
    const float bnrm2 = matrix_DotProduct(n, b, b);

    // r = b - Ax
    multiplyA(pyramid, pC, x, r);     // r = A x
    matrix_subtract(n, b, r);         // r = b - r

    // rdotr = r.r
    rdotr_best = rdotr_curr = matrix_DotProduct(n, r, r);

    // Setup initial vector
    matrix_copy(n, r, p);             // p = r
    matrix_copy(n, x, x_best);

    // const float irdotr = rdotr;
    // const float percent_sf = 100.0f / logf(tol2 * bnrm2 / irdotr);

    int iter = 0;
    int num_backwards = 0;
    for (; iter < itmax; iter++) {
        // TEST
        //ph->newValue((int)(logf(rdotr_curr / irdotr)*percent_sf));
        // User requested abort
        //if (ph->isTerminationRequested() && iter > 0) {
        //    break;
        //}

        // Ap = A p
        multiplyA(pyramid, pC, p, Ap);

        // alpha = r.r / (p . Ap)
        alpha = rdotr_curr / matrix_DotProduct(n, p, Ap);

        // r = r - alpha Ap
        //#pragma omp parallel for schedule(static)
        for (int i = 0; i < n; i++) {
            r[i] -= alpha * Ap[i];
        }

        // rdotr = r.r
        rdotr_prev = rdotr_curr;
        rdotr_curr = matrix_DotProduct(n, r, r);

        // Have we gone unstable?
        if (rdotr_curr > rdotr_prev) {
            // Save where we've got to
            if (num_backwards == 0 && rdotr_prev < rdotr_best) {
                rdotr_best = rdotr_prev;
                matrix_copy(n, x, x_best);
            }

            num_backwards++;
        } else {
            num_backwards = 0;
        }

        // x = x + alpha * p
        //#pragma omp parallel for schedule(static)
        for (int i = 0; i < n; i++) {
            x[i] += alpha * p[i];
        }

        // Exit if we're done
        // fprintf(stderr, "iter:%d err:%f\n", iter+1, sqrtf(rdotr/bnrm2));
        if (rdotr_curr / bnrm2 < tol2)
            { break; }

        if (num_backwards > num_backwards_ceiling) {
            // Reset
            num_backwards = 0;
            matrix_copy(n, x_best, x);

            // r = Ax
            multiplyA(pyramid, pC, x, r);

            // r = b - r
            matrix_subtract(n, b, r);

            // rdotr = r.r
            rdotr_best = rdotr_curr = matrix_DotProduct(n, r, r);

            // p = r
            matrix_copy(n, r, p);
        } else {
            // p = r + beta p
            beta = rdotr_curr / rdotr_prev;

            //#pragma omp parallel for schedule(static)
            for (int i = 0; i < n; i++) {
                p[i] = r[i] + beta * p[i];
            }
        }
    }

    // Use the best version we found
    if (rdotr_curr > rdotr_best) {
        rdotr_curr = rdotr_best;
        matrix_copy(n, x_best, x);
    }

    if (rdotr_curr / bnrm2 > tol2) {
        // Not converged
        //ph->newValue((int)(logf(rdotr_curr / irdotr)*percent_sf));
        if (iter == itmax) {
            MSG(" Warning: Not converged (hit maximum iterations), error = %f (should be below %f).\n", sqrtf(rdotr_curr / bnrm2), tol);
        } else {
            MSG(" Warning: Not converged (going unstable), error = %f (should be below %f).\n", sqrtf(rdotr_curr / bnrm2), tol);
        }
    } else {
        // ph->newValue(100);
    }

    matrix_free(x_best);
    matrix_free(p);
    matrix_free(Ap);
    matrix_free(r);
}

// conjugate linear equation solver
// overwrites pyramid!
//void lincg(pyramid_t* pyramid, pyramid_t* pC, const float* const b, float* const x, const int itmax, const float tol, ProgressHelper *ph)
//{
//  const int num_backwards_ceiling = 3;
//
//#ifdef TIMER_PROFILING
//  //msec_timer f_timer;
//  //f_timer.start();
//#endif
//
//  const int rows = pyramid->rows;
//  const int cols = pyramid->cols;
//  const int n = rows*cols;
//  const float tol2 = tol*tol;
//
//  float* const x_save = matrix_alloc(n);
//  float* const r = matrix_alloc(n);
//  float* const p = matrix_alloc(n);
//  float* const Ap = matrix_alloc(n);
//
//  // bnrm2 = ||b||
//  const float bnrm2 = matrix_DotProduct(n, b, b);
//
//  // r = b - Ax
//  multiplyA(pyramid, pC, x, r);
//  matrix_subtract(n, b, r);
//
//  // rdotr = r.r
//  float rdotr = matrix_DotProduct(n, r, r);
//
//  // p = r
//  matrix_copy(n, r, p);
//
//  // Setup initial vector
//  float saved_rdotr = rdotr;
//  matrix_copy(n, x, x_save);
//
//  const float irdotr = rdotr;
//  const float percent_sf = 100.0f/logf(tol2*bnrm2/irdotr);
//  int iter = 0;
//  int num_backwards = 0;
//
//  for (; iter < itmax; iter++)
//  {
//    // TEST
//    ph->newValue( (int) (logf(rdotr/irdotr)*percent_sf) );
//    if (ph->isTerminationRequested() && iter > 0 ) // User requested abort
//      break;
//
//    // Ap = A p
//    multiplyA(pyramid, pC, p, Ap);
//
//    // alpha = r.r / (p . Ap)
//    const float alpha = rdotr / matrix_DotProduct(n, p, Ap);
//
//    // r = r - alpha Ap
//#ifdef __SSE__
//    VEX_vsubs(r, alpha, Ap, r, n);
//#else
//    //#pragma omp parallel for schedule(static)
//    for (int i = 0; i < n; i++)
//      r[i] -= alpha * Ap[i];
//#endif
//
//    // rdotr = r.r
//    const float old_rdotr = rdotr;
//    rdotr = matrix_DotProduct(n, r, r);
//
//    // Have we gone unstable?
//    if (rdotr > old_rdotr)
//    {
//      // Save where we've got to
//      if (num_backwards == 0 && old_rdotr < saved_rdotr)
//	    {
//	      saved_rdotr = old_rdotr;
//	      matrix_copy(n, x, x_save);
//	    }
//
//      num_backwards++;
//    }
//    else
//    {
//      num_backwards = 0;
//    }
//
//    // x = x + alpha * p
//#ifdef __SSE__
//    VEX_vadds(x, alpha, p, x, n);
//#else
//    //#pragma omp parallel for schedule(static)
//    for (int i = 0; i < n; i++)
//      x[i] += alpha * p[i];
//#endif
//
//    // Exit if we're done
//    // fprintf(stderr, "iter:%d err:%f\n", iter+1, sqrtf(rdotr/bnrm2));
//    if (rdotr/bnrm2 < tol2)
//      break;
//
//    if (num_backwards > num_backwards_ceiling)
//    {
//      // Reset
//      num_backwards = 0;
//      matrix_copy(n, x_save, x);
//
//      // r = Ax
//      multiplyA(pyramid, pC, x, r);
//
//      // r = b - r
//      matrix_subtract(n, b, r);
//
//      // rdotr = r.r
//      rdotr = matrix_DotProduct(n, r, r);
//      saved_rdotr = rdotr;
//
//      // p = r
//      matrix_copy(n, r, p);
//    }
//    else
//    {
//      // p = r + beta p
//      const float beta = rdotr/old_rdotr;
//#ifdef __SSE__
//      VEX_vadds(r, beta, p, p, n);
//#else
//      //#pragma omp parallel for schedule(static)
//      for (int i = 0; i < n; i++)
//        p[i] = r[i] + beta*p[i];
//#endif
//    }
//  }
//
//  // Use the best version we found
//  if (rdotr > saved_rdotr)
//  {
//    rdotr = saved_rdotr;
//    matrix_copy(n, x_save, x);
//  }
//
//  if (rdotr/bnrm2 > tol2)
//  {
//    // Not converged
//    ph->newValue( (int) (logf(rdotr/irdotr)*percent_sf));
//    if (iter == itmax)
//      fprintf(stderr, "\npfstmo_mantiuk06: Warning: Not converged (hit maximum iterations), error = %g (should be below %g).\n", sqrtf(rdotr/bnrm2), tol);
//    else
//      fprintf(stderr, "\npfstmo_mantiuk06: Warning: Not converged (going unstable), error = %g (should be below %g).\n", sqrtf(rdotr/bnrm2), tol);
//  }
//  else
//    ph->newValue(100);
//
//  matrix_free(x_save);
//  matrix_free(p);
//  matrix_free(Ap);
//  matrix_free(r);
//
//#ifdef TIMER_PROFILING
//  //f_timer.stop_and_update();
//  //std::cout << "lincg() = " << f_timer.get_time() << " msec" << std::endl;
//#endif
//}


// in_tab and out_tab should contain inccreasing float values
inline float lookup_table(const int n, const float* const in_tab, const float* const out_tab, const float val) {
    if ((val < in_tab[0]))
        { return out_tab[0]; }

    for (int j = 1; j < n; ++j) {
        if (val < in_tab[j]) {
            const float dd = (val - in_tab[j - 1]) / (in_tab[j] - in_tab[j - 1]);
            return out_tab[j - 1] + (out_tab[j] - out_tab[j - 1]) * dd;
        }
    }

    return out_tab[n - 1];
}


// transform gradient (Gx,Gy) to R
inline void transform_to_R(const int n, float*   G, float detail_factor) {
    const float log10 = 2.3025850929940456840179914546844 * detail_factor;

    //#pragma omp parallel for schedule(static, 1024)
    for (int j = 0; j < n; j++) {
        // G to W
        float Curr_G = G[j];

        if (Curr_G < 0.0f) {
            Curr_G = -(powf(10, (-Curr_G) * log10) - 1.0f);
        } else {
            Curr_G = (powf(10, Curr_G * log10) - 1.0f);
        }
        // W to RESP
        if (Curr_G < 0.0f) {
            Curr_G = -lookup_table(LOOKUP_W_TO_R, W_table, R_table, -Curr_G);
        } else {
            Curr_G = lookup_table(LOOKUP_W_TO_R, W_table, R_table, Curr_G);
        }

        G[j] = Curr_G;
    }
}

// transform from R to G
inline void transform_to_G(const int n, float* const R, float detail_factor) {
    //here we are actually changing the base of logarithm
    const float log10 = 2.3025850929940456840179914546844 * detail_factor;

    //#pragma omp parallel for schedule(static,1024)
    for (int j = 0; j < n; j++) {
        float Curr_R = R[j];

        // RESP to W
        if (Curr_R < 0.0f) {
            Curr_R = -lookup_table(LOOKUP_W_TO_R, R_table, W_table, -Curr_R);
        } else {
            Curr_R = lookup_table(LOOKUP_W_TO_R, R_table, W_table, Curr_R);
        }

        // W to G
        if (Curr_R < 0.0f) {
            Curr_R = -log((-Curr_R) + 1.0f) / log10;
        } else {
            Curr_R = log(Curr_R + 1.0f) / log10;
        }

        R[j] = Curr_R;
    }
}


// transform gradient (Gx,Gy) to R for the whole pyramid
inline void pyramid_transform_to_R(pyramid_t* pyramid, float detail_factor) {
    while (pyramid != NULL) {
        transform_to_R(pyramid->rows * pyramid->cols, pyramid->Gx, detail_factor);
        transform_to_R(pyramid->rows * pyramid->cols, pyramid->Gy, detail_factor);

        pyramid = pyramid->next;
    }
}



// transform from R to G for the pyramid
inline void pyramid_transform_to_G(pyramid_t* pyramid, float detail_factor) {
    while (pyramid != NULL) {
        transform_to_G(pyramid->rows * pyramid->cols, pyramid->Gx, detail_factor);
        transform_to_G(pyramid->rows * pyramid->cols, pyramid->Gy, detail_factor);
        pyramid = pyramid->next;
    }
}

// multiply gradient (Gx,Gy) values by float scalar value for the whole pyramid
inline void pyramid_gradient_multiply(pyramid_t* pyramid, const float val) {
    while (pyramid != NULL) {
        matrix_multiply_const(pyramid->rows * pyramid->cols, pyramid->Gx, val);
        matrix_multiply_const(pyramid->rows * pyramid->cols, pyramid->Gy, val);
        pyramid = pyramid->next;
    }
}


int sort_float(const void* const v1, const void* const v2) {
    if (*((float*)v1) < *((float*)v2))
        { return -1; }

    if ((*((float*)v1) > *((float*)v2)))
        { return 1; }

    return 0;
}


// transform gradients to luminance
void transform_to_luminance(pyramid_t* pp, float*   x, const bool bcg, const int itmax, const float tol) {
    pyramid_t* pC = pyramid_allocate(pp->cols, pp->rows);
    pyramid_calculate_scale_factor(pp, pC); // calculate (Cx,Cy)
    pyramid_scale_gradient(pp, pC); // scale small gradients by (Cx,Cy);

    float* b = matrix_alloc(pp->cols * pp->rows);
    pyramid_calculate_divergence_sum(pp, b);  // calculate the sum of divergences (equal to b)

    MSG("  cg-lum-grad-solve");
    // calculate luminances from gradients
    if (bcg) {
        linbcg(pp, pC, b, x, itmax, tol);
    } else {
        lincg(pp, pC, b, x, itmax, tol);
    }

    matrix_free(b);
    pyramid_free(pC);
}


struct hist_data {
    float size;
    float cdf;
    int index;
};

int hist_data_order(const void* const v1, const void* const v2) {
    if (((struct hist_data*) v1)->size < ((struct hist_data*) v2)->size) {
        return -1;
    }

    if (((struct hist_data*) v1)->size > ((struct hist_data*) v2)->size) {
        return 1;
    }

    return 0;
}


int hist_data_index(const void* const v1, const void* const v2) {
    return ((struct hist_data*) v1)->index - ((struct hist_data*) v2)->index;
}


void contrast_equalization(pyramid_t* pp, const float contrastFactor) {
    // Count sizes
    int total_pixels = 0;
    pyramid_t* l = pp;
    while (l != NULL) {
        total_pixels += l->rows * l->cols;
        l = l->next;
    }

    // Allocate memory
    struct hist_data* hist = (struct hist_data*) malloc(sizeof(struct hist_data) * total_pixels);
    if (hist == NULL) {
        fprintf(stderr, "ERROR: malloc in contrast_equalization() (size:%zu)", sizeof(struct hist_data) * total_pixels);
        exit((int)155);
    }

    // Build histogram info
    l = pp;
    int index = 0;
    while (l != NULL) {
        const int pixels = l->rows * l->cols;
        const int offset = index;
        //#pragma omp parallel for schedule(static)
        for (int c = 0; c < pixels; ++c) {
            hist[c + offset].size = sqrtf(l->Gx[c] * l->Gx[c] + l->Gy[c] * l->Gy[c]);
            hist[c + offset].index = c + offset;
        }
        index += pixels;
        l = l->next;
    }

    // Generate histogram
    qsort(hist, total_pixels, sizeof(struct hist_data), hist_data_order);

    // Calculate cdf
    const float norm = 1.0f / (float) total_pixels;
    //#pragma omp parallel for schedule(static)
    for (int i = 0; i < total_pixels; ++i) {
        hist[i].cdf = ((float) i) * norm;
    }

    // Recalculate in terms of indexes
    qsort(hist, total_pixels, sizeof(struct hist_data), hist_data_index);

    //Remap gradient magnitudes
    l = pp;
    index = 0;
    while (l != NULL) {
        const int pixels = l->rows * l->cols;
        const int offset = index;
        //#pragma omp parallel for schedule(static)
        for (int c = 0; c < pixels; ++c) {
            const float scale = contrastFactor * hist[c + offset].cdf / hist[c + offset].size;
            l->Gx[c] *= scale;
            l->Gy[c] *= scale;
        }
        index += pixels;
        l = l->next;
    }

    free(hist);
}


// tone mapping

/**
 * @brief: Tone mapping algorithm [Mantiuk2006]
 *
 * @param R red channel
 * @param G green channel
 * @param B blue channel
 * @param Y luminance channel
 * @param contrastFactor contrast scaling factor (in 0-1 range)
 * @param saturationFactor color desaturation (in 0-1 range)
 * @param bcg true if to use BiConjugate Gradients, false if to use Conjugate Gradients
 * @param itmax maximum number of iterations for convergence (typically 50)
 * @param tol tolerence to get within for convergence (typically 1e-3)
 * @return PFSTMO_OK if tone-mapping was sucessful, PFSTMO_ABORTED if
 * it was stopped from a callback function and PFSTMO_ERROR if an
 * error was encountered.
 */

int tmo_mantiuk06_contmap(int c, int r, float* R, float* G, float* B, float* Y,
                          const float contrastFactor, const float saturationFactor, float detailfactor,
                          const bool bcg, const int itmax, const float tol) {
    const int n = c * r ;

    MSG("  normalize");

    /* Normalize */
    float  Ymax = Y[0];
    for (int j = 1; j < n; ++j) {
        if (Y[j] > Ymax) {
            Ymax = Y[j];
        }
    }

    const float clip_min = 1e-7f * Ymax;
    //#pragma omp parallel for schedule(static)
    for (int j = 0; j < n; ++j) {
        if ((R[j] < clip_min)) { R[j] = clip_min; }
        if ((G[j] < clip_min)) { G[j] = clip_min; }
        if ((B[j] < clip_min)) { B[j] = clip_min; }
        if ((Y[j] < clip_min)) { Y[j] = clip_min; }
    }

    //#pragma omp parallel for schedule(static)
    for (int j = 0; j < n; ++j) {
        R[j] /= Y[j];
        G[j] /= Y[j];
        B[j] /= Y[j];
        Y[j] = log10f(Y[j]);
    }

    MSG("  gradient-pyramid");

    pyramid_t* pp = pyramid_allocate(c, r);                 // create pyramid
    pyramid_calculate_gradient(pp, Y);                      // calculate gradients for pyramid (Y won't be changed)
    pyramid_transform_to_R(pp, detailfactor);               // transform gradients to R

    MSG("  contrast-pyramid");

    /* Contrast map */
    if (contrastFactor > 0.0f) {
        pyramid_gradient_multiply(pp, contrastFactor); // Contrast mapping
    } else {
        contrast_equalization(pp, -contrastFactor); // Contrast equalization
    }

    MSG("  pyramid-transform");

    pyramid_transform_to_G(pp, detailfactor);   // transform R to gradients

    MSG("  contrast-luminance");

    transform_to_luminance(pp, Y, bcg, itmax, tol); // transform gradients to luminance Y
    pyramid_free(pp);

    MSG("  re-normalize");

    /* Renormalize luminance */
    float* temp = matrix_alloc(n);

    matrix_copy(n, Y, temp);                    // copy Y to temp
    qsort(temp, n, sizeof(float), sort_float);  // sort temp in ascending order

    const float CUT_MARGIN = 0.1f;
    float trim;
    float delta;

    trim = (n - 1) * CUT_MARGIN * 0.01f;
    delta = trim - floorf(trim);
    const float l_min = temp[(int)floorf(trim)] * delta + temp[(int)ceilf(trim)] * (1.0f - delta);

    trim = (n - 1) * (100.0f - CUT_MARGIN) * 0.01f;
    delta = trim - floorf(trim);
    const float l_max = temp[(int)floorf(trim)] * delta + temp[(int)ceilf(trim)] * (1.0f - delta);

    matrix_free(temp);

    const float disp_dyn_range = 2.3f;
    //#pragma omp parallel for schedule(static)
    for (int j = 0; j < n; ++j) {
        Y[j] = (Y[j] - l_min) / (l_max - l_min) * disp_dyn_range - disp_dyn_range;    // x scaled
    }

    MSG("  rgb convert");

    /* Transform to linear scale RGB */
    //#pragma omp parallel for schedule(static)
    for (int j = 0; j < n; ++j) {
        Y[j] = powf(10, Y[j]);
        R[j] = powf(R[j], saturationFactor) * Y[j];
        G[j] = powf(G[j], saturationFactor) * Y[j];
        B[j] = powf(B[j], saturationFactor) * Y[j];
    }

    return 1;//PFSTMO_OK;
}


//#endif // __TMO_H__









/**
 * @brief Frederic Drago logmapping operator
 *
 * Adaptive logarithmic mapping for displaying high contrast
 * scenes.
 * F. Drago, K. Myszkowski, T. Annen, and N. Chiba. In Eurographics 2003.
 *
 * This file is a part of LuminanceHDR package, based on pfstmo.
 * ----------------------------------------------------------------------
 * Copyright (C) 2003,2004 Grzegorz Krawczyk
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * ----------------------------------------------------------------------
 *
 * @author Grzegorz Krawczyk, <krawczyk@mpi-sb.mpg.de>
 *
 * $Id: tmo_drago03.cpp,v 1.4 2008/11/04 23:43:08 rafm Exp $
 */

#include <math.h>
// #include "TonemappingOperators/pfstmo.h"
// #include "tmo_drago03.h"

/// Type of algorithm
#define FAST 0

inline float biasFunc(float b, float x) {
    return powf(x, b);		// pow(x, log(bias)/log(0.5)
}

//-------------------------------------------

void calculateLuminance(unsigned int width, unsigned int height, const float* Y, float& avLum, float& maxLum) {
    avLum = 0.0f;
    maxLum = 0.0f;
    int size = width * height;
    for (int i = 0 ; i < size; i++) {
        avLum += log(Y[i] + 1e-4);
        maxLum = (Y[i] > maxLum) ? Y[i] : maxLum ;
    }
    avLum = exp((float)(avLum / size));
}

// Y = in  |  L = out
void tmo_drago03(unsigned int width, unsigned int height,
                 const float* Y, float* L,
                 float bias
                ) {
    const float LOG05 = -0.693147f; // log(0.5)

    float maxLum, avLum;
    calculateLuminance(width, height, Y, avLum, maxLum);

    int nrows = height;			// image size
    int ncols = width;

    maxLum /= avLum;	   // normalize maximum luminance by average luminance

    float divider = log10(maxLum + 1.0f);
    float biasP = logf(bias) / LOG05;

#if !FAST
    // Normal tone mapping of every pixel
    for (int y = 0 ; y < nrows; y++) {
        // ph->newValue(100 * y / nrows);
        // if (ph->isTerminationRequested()) {
        //     break;
        // }
        for (int x = 0 ; x < ncols; x++) {
            float Yw = Y[x + y * width] / avLum;
            float interpol = logf(2.0f + biasFunc(biasP, Yw / maxLum) * 8.0f);
            L[x + y* width] = (logf(Yw + 1.0f) / interpol) / divider;
        }
    }
#else
    // 	Approximation of log(x+1)
    // 		x(6+x)/(6+4x) good if x < 1
    //     x*(6 + 0.7662x)/(5.9897 + 3.7658x) between 1 and 2
    //     http://users.pandora.be/martin.brown/home/consult/logx.htm
    int i, j;
    for (int y = 0; y < nrows; y += 3) {
        for (int x = 0; x < ncols; x += 3) {
            float average = 0.0f;
            for (i = 0; i < 3; i++)
                for (j = 0; j < 3; j++) {
                    average += (*Y)(x + i, y + j) / avLum;
                }
            average = average / 9.0f - (*Y)(x, y);

            if (average > -1.0f && average < 1.0f) {
                float interpol = log(2.0f + biasFunc(biasP, (*Y)(x + 1, y + 1) / maxLum) * 8.0f);
                for (i = 0; i < 3; i++)
                    for (j = 0; j < 3; j++) {
                        float Yw = (*Y)(x + i, y + j);
                        if (Yw < 1.0f) {
                            float L = Yw * (6.0f + Yw) / (6.0f + 4.0f * Yw);
                            Yw = (L / interpol) / divider;
                        } else if (Yw >= 1.0f && Yw < 2.0f) {
                            float L = Yw * (6.0f + 0.7662 * Yw) / (5.9897f + 3.7658f * Yw);
                            Yw = (L / interpol) / divider;
                        } else {
                            Yw = (log(Yw + 1.0f) / interpol) / divider;
                        }
                        (*L)(x + i, y + j) = Yw;
                    }
            } else {
                for (i = 0; i < 3; i++)
                    for (j = 0; j < 3; j++) {
                        float Yw = (*Y)(x + i, y + j);
                        float interpol = log(2.0f + biasFunc(biasP, Yw / maxLum) * 8.0f);
                        (*L)(x + i, y + j) = (log(Yw + 1.0f) / interpol) / divider;
                    }
            }
        }
    }
#endif // #else #ifndef FAST

}

