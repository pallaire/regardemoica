#ifndef SHAPE_H
#define SHAPE_H

#include <adwaita.h>
#include "point.h"


typedef struct {
    double r, g, b, a;
} RGBA;


#define SHAPE_TYPE_NONE     0
#define SHAPE_TYPE_ARROW    1

typedef struct Shape Shape;
struct Shape {
    int     type;

    bool    is_selected;
    bool    is_dragging;
    bool    is_showing_handles;
    int     dragging_point_index;

    Point   points[2];
    RGBA    color;
    RGBA    color_accent;
    RGBA    color_shadow;
    char*   data;

    // Function pointers, poorman's oop
    void (*free_data)(Shape* self);
    void (*draw)(Shape* self, cairo_t *cr, double scale);
    bool (*is_hit)(Shape* self, double x, double y);
    bool (*is_handle_hit)(Shape* self, double x, double y);
};


Shape* shape_new();
void shape_free(Shape* self);
void shape_free_data(Shape* self);
void shape_draw_handle_at(Shape* self, Point p, cairo_t* cr, double scale);


#endif // SHAPE_H