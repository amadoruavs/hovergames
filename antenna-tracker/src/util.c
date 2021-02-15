#include "util.h"

float constrain(float value, float low, float high)
{
	value = (value < low) ? low : value;
	value = (value > high) ? high : value;

	return value;
}
