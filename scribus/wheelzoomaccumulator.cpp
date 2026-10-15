/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include "wheelzoomaccumulator.h"

int WheelZoomAccumulator::consume(int verticalAngleDelta, Qt::ScrollPhase phase)
{
	if (phase == Qt::ScrollBegin)
		reset();
	if (verticalAngleDelta == 0)
	{
		if (phase == Qt::ScrollEnd)
			reset();
		return 0;
	}
	// A reversed gesture should not first have to cancel a previous partial step.
	if ((m_remainder > 0 && verticalAngleDelta < 0)
		|| (m_remainder < 0 && verticalAngleDelta > 0))
		reset();
	const qint64 total = static_cast<qint64>(m_remainder) + verticalAngleDelta;
	constexpr int anglePerStep = 120;
	const int steps = static_cast<int>(total / anglePerStep);
	m_remainder = static_cast<int>(total % anglePerStep);
	if (phase == Qt::ScrollEnd)
		reset();
	return steps;
}
