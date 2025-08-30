// ThresholdCompare.cpp
// aSub user routine to replace CALC-based threshold comparison with C++ logic

#include <cstdio>
#include <cmath>

#include <aSubRecord.h>
#include <registryFunction.h>
#include <epicsExport.h>
#include <epicsTypes.h>

extern "C" {

// Logic: if A >= (B + C) => 1, else if A <= (B - C) => 0, else keep D
// A: Current value, B: Threshold, C: Hysteresis, D: Previous output state
static long thresholdCompare(aSubRecord* prec)
{
    if (!prec) return 0;

    // Expect scalar inputs; DB template sets FTA/FTB/FTC/FTD = DOUBLE and NOA/NOB/NOC/NOD = 1
    double A = 0.0, B = 0.0, C = 0.0, D = 0.0;
    if (prec->a) A = *(const double*)prec->a;
    if (prec->b) B = *(const double*)prec->b;
    if (prec->c) C = *(const double*)prec->c;
    if (prec->d) D = *(const double*)prec->d;

    int prev = (int)std::lrint(D);
    int out = prev;

    if (A >= (B + C)) {
        out = 1;
    } else if (A <= (B - C)) {
        out = 0;
    } else {
        out = prev; // keep previous state in hysteresis band
    }

    // Output as LONG; DB template sets FTVA=LONG and NOVA=1
    if (prec->vala) {
        *(epicsInt32*)prec->vala = (epicsInt32)out;
    }

    return 0; // success
}

epicsRegisterFunction(thresholdCompare);

} // extern "C"

