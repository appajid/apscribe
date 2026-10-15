/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#ifndef WHEELZOOMACCUMULATOR_H
#define WHEELZOOMACCUMULATOR_H

#include <Qt>

// Converts high-resolution vertical wheel movement to whole zoom steps.
// Qt reports angle deltas in eighths of a degree: 120 is one usual wheel notch.
class WheelZoomAccumulator
{
public:
	int consume(int verticalAngleDelta, Qt::ScrollPhase phase = Qt::NoScrollPhase);
	void reset() { m_remainder = 0; }

private:
	int m_remainder { 0 };
};

#endif
