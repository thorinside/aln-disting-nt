// Generated from learned ngspice outputs. Do not hand-edit.
#pragma once

#include <stdint.h>

namespace aln_fold {

static const uint16_t kLeafMask = 0x8000u;
static const uint8_t kBuchla259Model = 0u;
static const uint8_t kSixteenFoldModel = 1u;
static const uint8_t kFoldModelCount = 2u;

struct DTreeNode {
    float threshold;
    uint16_t lowerRef;
    uint16_t upperRef;
};

struct AffineLeaf {
    float slope;
    float intercept;
    float integralConstant;
};

struct FoldModelData {
    const DTreeNode* nodes;
    const AffineLeaf* leaves;
    const float* thresholds;
    const float* coefficients;
    uint16_t hingeCount;
    uint16_t rootRef;
    uint16_t nodeCount;
    uint16_t leafCount;
    uint16_t maximumDepth;
    float inputMinimum;
    float inputMaximum;
    float driveMinimum;
    float driveMaximum;
    float outputGain;
};

static_assert(sizeof(DTreeNode) == 8, "D-tree node size changed");
static_assert(sizeof(AffineLeaf) == 12, "affine leaf size changed");

static const DTreeNode kBuchla259Nodes[8] = {
    {-0.600000024f, 1u, 4u},
    {-2.99499989f, 2u, 3u},
    {-4.07999992f, 32768u, 32769u},
    {-1.79999995f, 32770u, 32771u},
    {1.79999995f, 5u, 6u},
    {0.600000024f, 32772u, 32773u},
    {2.99499989f, 32774u, 7u},
    {4.07999992f, 32775u, 32776u},
};

static const AffineLeaf kBuchla259Leaves[9] = {
    {4.39208317f, 20.578968f, 0.0f},
    {-5.33674049f, -19.1146336f, -80.9749451f},
    {5.12922621f, 12.230937f, -34.0349541f},
    {-4.99796009f, -5.99799776f, -50.4409943f},
    {4.998703f, 0.0f, -48.6415977f},
    {-4.99796009f, 5.99799776f, -50.4409943f},
    {5.12922621f, -12.230937f, -34.0349541f},
    {-5.33674049f, 19.1146336f, -80.9749451f},
    {4.39208317f, -20.578968f, 0.0f},
};

static const float kBuchla259Thresholds[4] = {
    0.600000024f,
    1.79999995f,
    2.99499989f,
    4.07999992f,
};

static const float kBuchla259Coefficients[5] = {
    4.998703f,
    -9.99666309f,
    10.1271858f,
    -10.4659662f,
    9.72882366f,
};

static const DTreeNode kSixteenFoldNodes[16] = {
    {-0.314999998f, 1u, 8u},
    {-2.78500009f, 2u, 5u},
    {-4.08500004f, 3u, 4u},
    {-4.72249985f, 32768u, 32769u},
    {-3.45499992f, 32770u, 32771u},
    {-1.56500006f, 6u, 7u},
    {-2.19000006f, 32772u, 32773u},
    {-0.94749999f, 32774u, 32775u},
    {2.19000006f, 9u, 12u},
    {0.94749999f, 10u, 11u},
    {0.314999998f, 32776u, 32777u},
    {1.56500006f, 32778u, 32779u},
    {3.45499992f, 13u, 14u},
    {2.78500009f, 32780u, 32781u},
    {4.08500004f, 32782u, 15u},
    {4.72249985f, 32783u, 32784u},
};

static const AffineLeaf kSixteenFoldLeaves[17] = {
    {9.4135952f, 48.873745f, 0.0f},
    {-9.74534225f, -41.6043358f, -213.641373f},
    {9.36904144f, 36.4779243f, -54.1583557f},
    {-9.78054047f, -29.6838837f, -168.452881f},
    {9.41430569f, 23.7737617f, -94.0131073f},
    {-9.66301632f, -18.0055714f, -139.761475f},
    {9.53324699f, 12.036581f, -116.253494f},
    {-9.52239037f, -6.01863575f, -124.807152f},
    {9.58439064f, 0.0f, -123.859215f},
    {-9.52239037f, 6.01863575f, -124.807152f},
    {9.53324699f, -12.036581f, -116.253494f},
    {-9.66301632f, 18.0055714f, -139.761475f},
    {9.41430569f, -23.7737617f, -94.0131073f},
    {-9.78054047f, 29.6838837f, -168.452881f},
    {9.36904144f, -36.4779243f, -54.1583557f},
    {-9.74534225f, 41.6043358f, -213.641373f},
    {9.4135952f, -48.873745f, 0.0f},
};

static const float kSixteenFoldThresholds[8] = {
    0.314999998f,
    0.94749999f,
    1.56500006f,
    2.19000006f,
    2.78500009f,
    3.45499992f,
    4.08500004f,
    4.72249985f,
};

static const float kSixteenFoldCoefficients[9] = {
    9.58439064f,
    -19.106781f,
    19.0556374f,
    -19.1962643f,
    19.077322f,
    -19.1948452f,
    19.1495819f,
    -19.1143837f,
    19.1589375f,
};

static const FoldModelData kFoldModels[kFoldModelCount] = {
    {
        kBuchla259Nodes,
        kBuchla259Leaves,
        kBuchla259Thresholds,
        kBuchla259Coefficients,
        4u,
        0u,
        8u,
        9u,
        4u,
        -5.0f,
        5.0f,
        0.0f,
        1.0f,
        1.59688544f,
    },
    {
        kSixteenFoldNodes,
        kSixteenFoldLeaves,
        kSixteenFoldThresholds,
        kSixteenFoldCoefficients,
        8u,
        0u,
        16u,
        17u,
        5u,
        -5.0f,
        5.0f,
        0.0f,
        1.0f,
        1.13172269f,
    },
};

}  // namespace aln_fold
