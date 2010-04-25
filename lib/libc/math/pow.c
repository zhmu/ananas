double
pow(double x, double y)
{
	/* XXX Yes, this is incorrect if y isn't an positive integer. But should be good enough for now */
	double v = y;
	while (y > 0) {
		x *= v;
		y--;
	}
	return x;
}
