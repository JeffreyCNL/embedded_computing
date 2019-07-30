//
//  matlabfiles.h
//  MAT_C
//
//  Created by JOSEPH GARBINI on 5/7/14.
//  Copyright (c) 2014 JOSEPH GARBINI. All rights reserved.
//

//  #ifndef MAT_C_matlabfiles_h
//  #define MAT_C_matlabfiles_h
//  #endif


#ifndef matfiles_h
#define matfiles_h

#include <stdio.h>

typedef struct
{
    FILE *fp;      /* stdio file pointer, do not access */
    int err;       /* IO error values, 0 for OK */
    long *cellpos; /* stack of cell positions */
    int celldepth; /* depth of cell nesting */
} MATFILE;

MATFILE *openmatfile(char *fname, int *err);
int matfile_addmatrix(MATFILE *mf, char *name, double *data, int m, int n, int transpose);
int matfile_addstring(MATFILE *mf, char *name, char *str);
int matfile_cellpush(MATFILE *mf, char *name, int m, int n);
int matfile_cellpop(MATFILE *mf);
int matfile_close(MATFILE *mf);

#endif