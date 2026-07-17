#ifndef ARROW_H
#define ARROW_H

#include "point.h"
#include "shape.h"

Shape* arrow_new(Point start, Point end);
void   arrow_free_data(Shape* self);


#endif // ARROW_H
