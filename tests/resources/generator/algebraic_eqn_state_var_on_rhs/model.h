/* The content of this file was generated using the C profile of libCellML 0.2.0. */

#pragma once

#include <stddef.h>

extern const char LIBCELLML_VERSION[];

extern const size_t STATE_COUNT;
extern const size_t VARIABLE_COUNT;

typedef enum {
    CONSTANT,
    COMPUTED_CONSTANT,
    ALGEBRAIC
} VariableType;

typedef struct {
    char name[3];
    char units[14];
    char component[17];
} VariableInfo;

typedef struct {
    char name[3];
    char units[14];
    char component[17];
    VariableType type;
} VariableInfoWithType;

extern const VariableInfo VOI_INFO;
extern const VariableInfo STATE_INFO[];
extern const VariableInfoWithType VARIABLE_INFO[];

extern double * createStatesArray();
extern double * createVariablesArray();
extern void deleteArray(double *array);

extern void initializeStatesAndConstants(double *states, double *variables);
extern void computeComputedConstants(double *variables);
extern void computeRates(double voi, double *states, double *rates, double *variables);
extern void computeVariables(double voi, double *states, double *rates, double *variables);
