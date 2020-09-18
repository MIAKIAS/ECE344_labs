#include <assert.h>
#include <math.h>
#include "common.h"
#include "point.h"

void
point_translate(struct point *p, double x, double y)
{
	p->x += x;
	p->y += y;
}

double
point_distance(const struct point *p1, const struct point *p2)
{
	double sideA = p2->y - p1->y;
	double sideB = p2->x - p1->x;
	return sqrt(pow(sideA, 2) + pow(sideB, 2));
}

int
point_compare(const struct point *p1, const struct point *p2)
{
	struct point origin;
	origin.x = origin.y = 0;
	double dif = point_distance(p1, &origin) - point_distance(p2, &origin);
	if (dif < 0) return -1;
	else if (dif == 0) return 0;
	else return 1;
}
