#include "shape.h"


Shape* shape_new() {
    Shape* s = calloc(1, sizeof(Shape));
    if(s != NULL) {
        s->type = SHAPE_TYPE_NONE;
    }
    return s;
}

void shape_free(Shape* self) {
    if(self != NULL) {
        if(self->text != NULL) {
            free(self->text);
        }
        free(self);
    }
}

void shape_draw_handle_at(Shape* /*self*/, Point p, cairo_t* cr) {
    cairo_save (cr);

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_arc (cr, p.x, p.y, 8, 0.0, 2 * M_PI);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.2588, 0.5294, 0.9608);
    cairo_arc (cr, p.x, p.y, 6, 0.0, 2 * M_PI);
    cairo_fill(cr);

    cairo_restore (cr);    
}
