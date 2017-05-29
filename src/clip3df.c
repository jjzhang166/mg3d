/*
** $Id: clip3df.c,v 1.9 2006-01-14 05:26:07 xwyan Exp $
**
** Port to MiniGUI by Feynman from allegro.
**
** Copyright (C) 2006 Feynman Software.
 *
 * Fixed point version of the 3D polygon clipper.
 *
 * By Shawn Hargreaves.
 *
 * Allegro is a gift-ware.
**/

/*
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/fixedmath.h>

#include "mg3d.h"

#define INT_NONE        0
#define INT_1COL        1
#define INT_3COL        2
#define INT_3COLP       4
#define INT_UV          8

/* point_inside:
 *  Copies a vertex to the output table.
 */
#define point_inside(vv)                                 \
{                                                        \
   v3->x = v2->x; v3->y = v2->y; v3->z = v2->z;          \
   v3->u = v2->u; v3->v = v2->v; v3->c = v2->c;          \
   vv++;                                                 \
}

/* point_interp_f:
 *  Interpolates between v1 and v2, with slope "t", resulting vertex v3.
 */
#define point_interp_f(vv, hdc)                          \
{                                                        \
   v3->x = (v2->x - v1->x) * t + v1->x;                  \
   v3->y = (v2->y - v1->y) * t + v1->y;                  \
   v3->z = (v2->z - v1->z) * t + v1->z;                  \
		                             					 \
   if (flags & INT_1COL) {                               \
      v3->c = (int)((v2->c - v1->c) * t + v1->c);        \
   }                                                     \
   else if (flags & INT_3COLP) {                         \
       Uint8 r1, r2, g1, g2 , b1 ,b2 ;                                                                         \
       int bpp = GetGDCapability(hdc, GDCAP_DEPTH);                                                          \
       Pixel2RGB(hdc, v2->c, &r2, &g2, &b2);                                                                 \
       Pixel2RGB(hdc, v1->c, &r1, &g1, &b1);                                                                 \
       int r = (int)((r2 - r1) * t + r1);                                                            \
       int b = (int)((b2 - b1) * t + b1);                                                            \
       int g = (int)((g2 - g1) * t + g1);                                                            \
       v3->c = RGB2Pixel(hdc, r & 255, g & 255, b & 255);                                                    \
   }                                                     \
   else if (flags & INT_3COL) {                          \
      int r = (int)(((v2->c & 0xFF0000) - (v1->c & 0xFF0000)) * t + (v1->c & 0xFF0000)); \
      int g = (int)(((v2->c & 0x00FF00) - (v1->c & 0x00FF00)) * t + (v1->c & 0x00FF00)); \
      int b = (int)(((v2->c & 0x0000FF) - (v1->c & 0x0000FF)) * t + (v1->c & 0x0000FF)); \
      v3->c = (r & 0xFF0000) | (g & 0x00FF00) | (b & 0x0000FF); \
   }                                                     \
   if (flags & INT_UV) {                                 \
      v3->u = (v2->u - v1->u) * t + v1->u;               \
      v3->v = (v2->v - v1->v) * t + v1->v;               \
   }                                                     \
   vv++;                                                 \
}

/*  mg3dClipF:
 *  Clips a 3D polygon against planes x = -z, x = z, y = -z, y = z,
 *  z = min_z, z = max_z. If max_z <= min_z the z = max_z clipping is
 *  not done. Type is the rendering style - MG3D_POLYTYPE_*, vc is the 
 *  number of vertices, vtx contains the input vertices, vout will 
 *  receive the result, vtmp and out must be supplied for temporary 
 *  storage. The size of arrays vout, vtmp and out should be twice as 
 *  big as vtx.
 */
int mg3dClipF(HDC mem_dc, int type, float min_z, float max_z, int vc, const mg3dVpf *vtx[], mg3dVpf *vout[], mg3dVpf *vtmp[], int out[])
{
    int i, j, vo, vt, flags;
    float t;
    mg3dVpf *v3;

    const mg3dVpf *v1, *v2, **vin;

    static int flag_table[] = 
    {
        INT_NONE,                             /* flat */
        INT_3COLP,                            /* gcol */
        INT_3COL,                             /* grgb */
        INT_UV,                               /* atex */
        INT_UV,                               /* ptex */
        INT_UV,                               /* atex mask */
        INT_UV,                               /* ptex mask */
        INT_UV + INT_1COL,                    /* atex lit */
        INT_UV + INT_1COL,                    /* ptex lit */
        INT_UV + INT_1COL,                    /* atex mask lit */
        INT_UV + INT_1COL,                    /* ptex mask lit */
        INT_UV,                               /* atex trans */
        INT_UV,                               /* ptex trans */
        INT_UV,                               /* atex mask trans */
        INT_UV                                /* ptex mask trans */
    };

    type &= ~MG3D_POLYTYPE_ZBUF;
    flags = flag_table[type];

    if (max_z > min_z) 
    {
        vt = 0;

        for (i=0; i<vc; i++)
            out[i] = (vtx[i]->z > max_z);

        for (i=0, j=vc-1; i<vc; j=i, i++) 
        {
            v1 = vtx[j];
            v2 = vtx[i];
            v3 = vtmp[vt];

            if ((out[j] & out[i]) != 0)
                continue;

            if ((out[j] | out[i]) == 0) 
            {
                point_inside(vt);
                continue;
            }

            t = (max_z - v1->z) / (v2->z - v1->z);
            point_interp_f(vt, mem_dc);
            v3 = vtmp[vt];

            if (out[j])
                point_inside(vt);
        }
        vin = (const mg3dVpf**)vtmp;
    }
    else 
    {
        vt = vc;
        vin = vtx;
    }

    vo = 0;

    for (i=0; i<vt; i++)
        out[i] = (vin[i]->z < min_z);

    for (i=0, j=vt-1; i<vt; j=i, i++) 
    {
        v1 = vin[j];
        v2 = vin[i];
        v3 = vout[vo];

        if ((out[j] & out[i]) != 0)
            continue;

        if ((out[j] | out[i]) == 0) 
        {
            point_inside(vo);
            continue;
        }

        t = (min_z - v1->z) / (v2->z - v1->z);
        point_interp_f(vo, mem_dc);
        v3 = vout[vo];

        if (out[j])
            point_inside(vo);
    }

    vt = 0;

    for (i=0; i<vo; i++)
        out[i] = (vout[i]->x < -vout[i]->z);

    for (i=0, j=vo-1; i<vo; j=i, i++) 
    {
        v1 = vout[j];
        v2 = vout[i];
        v3 = vtmp[vt];

        if ((out[j] & out[i]) != 0)
            continue;

        if ((out[j] | out[i]) == 0) 
        {
            point_inside(vt);
            continue;
        }

        t = (-v1->z - v1->x) / (v2->x - v1->x + v2->z - v1->z);
        point_interp_f(vt, mem_dc);
        v3 = vtmp[vt];

        if (out[j])
            point_inside(vt);
    }

    vo = 0;

    for (i=0; i<vt; i++)
        out[i] = (vtmp[i]->x > vtmp[i]->z);

    for (i=0, j=vt-1; i<vt; j=i, i++) 
    {
        v1 = vtmp[j];
        v2 = vtmp[i];
        v3 = vout[vo];

        if ((out[j] & out[i]) != 0)
            continue;

        if ((out[j] | out[i]) == 0) 
        {
            point_inside(vo);
            continue;
        }

        t = (v1->z - v1->x) / (v2->x - v1->x - v2->z + v1->z);
        point_interp_f(vo, mem_dc);
        v3 = vout[vo];

        if (out[j])
            point_inside(vo);
    }

    vt = 0;

    for (i=0; i<vo; i++)
        out[i] = (vout[i]->y < -vout[i]->z);

    for (i=0, j=vo-1; i<vo; j=i, i++) 
    {
        v1 = vout[j];
        v2 = vout[i];
        v3 = vtmp[vt];

        if ((out[j] & out[i]) != 0)
            continue;

        if ((out[j] | out[i]) == 0) {
            point_inside(vt);
            continue;
        }

        t = (-v1->z - v1->y) / (v2->y - v1->y + v2->z - v1->z);
        point_interp_f(vt, mem_dc);
        v3 = vtmp[vt];

        if (out[j])
            point_inside(vt);
    }

    vo = 0;

    for (i=0; i<vt; i++)
        out[i] = (vtmp[i]->y > vtmp[i]->z);

    for (i=0, j=vt-1; i<vt; j=i, i++) 
    {
        v1 = vtmp[j];
        v2 = vtmp[i];
        v3 = vout[vo];

        if ((out[j] & out[i]) != 0)
            continue;

        if ((out[j] | out[i]) == 0) 
        {
            point_inside(vo);
            continue;
        }

        t = (v1->z - v1->y) / (v2->y - v1->y - v2->z + v1->z);
        point_interp_f(vo, mem_dc);
        v3 = vout[vo];

        if (out[j])
            point_inside(vo);
    }

    if (type == MG3D_POLYTYPE_FLAT)
        vout[0]->c = vtx[0]->c;

    return vo;
}

