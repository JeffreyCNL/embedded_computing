/*
 * ctable2.h
 *
 *  Created on: Sep 29, 2015
 *      Author: garbini
 */

#ifndef CTABLE_H_
#define CTABLE_H_

#endif /* CTABLE_H_ */

#include <stdio.h>
#include "me477.h"

typedef struct {
    char *e_label;    // entry label label
    int e_type;       // entry type (0-show; 1-edit
    double value;     // value
} table;

int		ctable2(char *TableTitle, table *t, int nval);
void	*Table_Update_Thread(void* resource);
void	upsub(table *a_table);
void    edt1();
void    helpmsg(void);
void	update();
