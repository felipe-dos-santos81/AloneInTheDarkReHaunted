///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: a near-verbatim copy of the engine's AITD1 pose loops
// (renderer.cpp InitGroupeRot, RotateList, RotateGroupe, TranslateGroupe,
// ZoomGroupe and AnimNuage's non-INFO_OPTIMISE path), the oracle for
// models::poseGroups. Differences from the engine: indices instead of
// pointers, `* 2` for `<< 1` (a left shift of a negative int is undefined
// before C++20), and the state list passed in instead of mutating the body.
//
// ReferencePose<false> is the engine: s16 points, integer arithmetic.
// ReferencePose<true> runs the same loops in double without truncation; the
// float matrices must equal it, and the integer path drifts from it by at most
// 10 * (depth + 1) units (tools/aitd_models/pose.py, spec §4.1).
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <array>
#include <cstdint>
#include <type_traits>
#include <vector>

#include "bodyPose.h"

template <bool Exact>
struct ReferencePose
{
    using Coord = std::conditional_t<Exact, double, int16_t>; // point3dStruct is s16
    using Work = std::conditional_t<Exact, double, int>;      // RotateList's locals are int
    struct Point
    {
        Coord x, y, z;
    };

    const models::PoseBody& body;
    const int16_t* cosTable;
    std::vector<Point> pointBuffer;
    int numOfBones = 0;
    bool boneRotateX = false, boneRotateY = false, boneRotateZ = false;
    int boneRotateXCos = 0, boneRotateXSin = 0;
    int boneRotateYCos = 0, boneRotateYSin = 0;
    int boneRotateZCos = 0, boneRotateZSin = 0;

    ReferencePose(const models::PoseBody& b, const int16_t* table) : body(b), cosTable(table) {}

    void InitGroupeRot(int transX, int transY, int transZ)
    {
        if (transX)
        {
            boneRotateXCos = cosTable[transX & 0x3FF];
            boneRotateXSin = cosTable[(transX + 0x100) & 0x3FF];
            boneRotateX = true;
        }
        else
            boneRotateX = false;

        if (transY)
        {
            boneRotateYCos = cosTable[transY & 0x3FF];
            boneRotateYSin = cosTable[(transY + 0x100) & 0x3FF];
            boneRotateY = true;
        }
        else
            boneRotateY = false;

        if (transZ)
        {
            boneRotateZCos = cosTable[transZ & 0x3FF];
            boneRotateZSin = cosTable[(transZ + 0x100) & 0x3FF];
            boneRotateZ = true;
        }
        else
            boneRotateZ = false;
    }

    // One axis step: ((a*S - b*C) >> 16) << 1 in the engine.
    static Work mix(Work a, Work b, int sin_, int cos_, bool second)
    {
        if constexpr (Exact)
            return second ? (a * cos_ + b * sin_) / 32768.0 : (a * sin_ - b * cos_) / 32768.0;
        else
            return second ? ((((a * cos_) + (b * sin_))) >> 16) * 2 : ((((a * sin_) - (b * cos_))) >> 16) * 2;
    }

    void RotateList(int start, int numOfPoint)
    {
        for (int i = 0; i < numOfPoint; i++)
        {
            Point& point = pointBuffer[start + i];
            Work x = point.x;
            Work y = point.y;
            Work z = point.z;

            if (boneRotateY)
            {
                Work tempX = x;
                Work tempZ = z;
                x = mix(tempX, tempZ, boneRotateYSin, boneRotateYCos, false);
                z = mix(tempX, tempZ, boneRotateYSin, boneRotateYCos, true);
            }

            if (boneRotateX)
            {
                Work tempY = y;
                Work tempZ = z;
                y = mix(tempY, tempZ, boneRotateXSin, boneRotateXCos, false);
                z = mix(tempY, tempZ, boneRotateXSin, boneRotateXCos, true);
            }

            if (boneRotateZ)
            {
                Work tempX = x;
                Work tempY = y;
                x = mix(tempX, tempY, boneRotateZSin, boneRotateZCos, false);
                y = mix(tempX, tempY, boneRotateZSin, boneRotateZCos, true);
            }

            point.x = (Coord)x;
            point.y = (Coord)y;
            point.z = (Coord)z;
        }
    }

    void RotateGroupe(int ptr)
    {
        const models::PoseGroup& g = body.groups[ptr];
        RotateList(g.start, g.count);

        int temp = g.self; // group number
        int temp2 = numOfBones - temp;

        do
        {
            if (body.groups[ptr].parent == temp) // is it one of this group's children
                RotateGroupe(ptr);               // yes, so apply the transformation to it
            ptr++;
        } while (--temp2);
    }

    void TranslateGroupe(int transX, int transY, int transZ, int group)
    {
        const models::PoseGroup& g = body.groups[group];
        for (int i = 0; i < g.count; i++)
        {
            Point& point = pointBuffer[g.start + i];
            point.x += transX;
            point.y += transY;
            point.z += transZ;
        }
    }

    static Coord zoom(Coord v, int factor)
    {
        if constexpr (Exact)
            return v * (factor + 256) / 256.0;
        else
            return (Coord)((v * (factor + 256)) / 256);
    }

    void ZoomGroupe(int zoomX, int zoomY, int zoomZ, int group)
    {
        const models::PoseGroup& g = body.groups[group];
        for (int i = 0; i < g.count; i++)
        {
            Point& point = pointBuffer[g.start + i];
            point.x = zoom(point.x, zoomX);
            point.y = zoom(point.y, zoomY);
            point.z = zoom(point.z, zoomZ);
        }
    }

    // AnimNuage without the camera part: model-space points, as pointBuffer holds them.
    std::vector<std::array<double, 3>> AnimNuage(int alpha, int beta, int gamma,
                                                 std::vector<models::GroupState> states)
    {
        pointBuffer.clear();
        for (const auto& v : body.verts)
            pointBuffer.push_back(Point{ (Coord)v[0], (Coord)v[1], (Coord)v[2] });
        numOfBones = (int)body.order.size();

        states[0].dx = (int16_t)alpha;
        states[0].dy = (int16_t)beta;
        states[0].dz = (int16_t)gamma;

        for (size_t i = 0; i < body.groups.size(); i++)
        {
            const int group = body.order[i];
            const models::GroupState& state = states[group];

            int transX = state.dx;
            int transY = state.dy;
            int transZ = state.dz;

            if (transX || transY || transZ)
            {
                switch (state.type)
                {
                case 0:
                    InitGroupeRot(transX, transY, transZ);
                    RotateGroupe(group);
                    break;
                case 1:
                    TranslateGroupe(transX, transY, transZ, group);
                    break;
                case 2:
                    ZoomGroupe(transX, transY, transZ, group);
                    break;
                }
            }
        }

        for (const models::PoseGroup& g : body.groups)
        {
            for (int j = 0; j < g.count; j++)
            {
                pointBuffer[g.start + j].x += pointBuffer[g.pivot].x;
                pointBuffer[g.start + j].y += pointBuffer[g.pivot].y;
                pointBuffer[g.start + j].z += pointBuffer[g.pivot].z;
            }
        }

        std::vector<std::array<double, 3>> out;
        for (const Point& p : pointBuffer)
            out.push_back({ (double)p.x, (double)p.y, (double)p.z });
        return out;
    }
};
