#ifndef POINT_H
#define POINT_H

typedef struct {
    double x;
    double y;
} Point;

Point point_new(double x, double y);

double points_distance(const Point* a, const Point* b);
double points_angle(const Point* a, const Point* b);

#endif // POINT_H
