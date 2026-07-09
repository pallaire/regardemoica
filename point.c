#include <math.h>
#include "point.h"

Point point_new(double x, double y) {
    Point p = { x, y };
    return p;
}

// Compute the distance between the 2 points
double points_distance(const Point* a, const Point* b) {
    double dx = b->x - a->x;
    double dy = b->y - a->y;
    return hypot(dx, dy);
}

// Compute the angle of the vector between the 2 points
double points_angle(const Point* a, const Point* b) {
    double dx = b->x - a->x;
    double dy = b->y - a->y;
    return atan2(dy, dx);
}