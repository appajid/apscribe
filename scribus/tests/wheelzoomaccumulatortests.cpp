/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program.
*/
#include <QtTest/QtTest>

#include "wheelzoomaccumulator.h"

class WheelZoomAccumulatorTests : public QObject
{
	Q_OBJECT
private slots:
	void preservesMouseWheelNotches();
	void accumulatesFineTrackpadDeltas();
	void preservesRemainderAcrossLargeDeltas();
	void resetsOnDirectionChangeAndGestureEnd();
	void resetsWhenControlZoomEnds();
};

void WheelZoomAccumulatorTests::preservesMouseWheelNotches()
{
	WheelZoomAccumulator zoom;
	QCOMPARE(zoom.consume(120), 1);
	QCOMPARE(zoom.consume(-120), -1);
	QCOMPARE(zoom.consume(0), 0);
}

void WheelZoomAccumulatorTests::accumulatesFineTrackpadDeltas()
{
	WheelZoomAccumulator zoom;
	QCOMPARE(zoom.consume(0, Qt::ScrollBegin), 0);
	for (int i = 0; i < 4; ++i)
		QCOMPARE(zoom.consume(24, Qt::ScrollUpdate), 0);
	QCOMPARE(zoom.consume(24, Qt::ScrollUpdate), 1);
	QCOMPARE(zoom.consume(24, Qt::ScrollUpdate), 0);
}

void WheelZoomAccumulatorTests::preservesRemainderAcrossLargeDeltas()
{
	WheelZoomAccumulator zoom;
	QCOMPARE(zoom.consume(300), 2);
	QCOMPARE(zoom.consume(60), 1);
	QCOMPARE(zoom.consume(-240), -2);
}

void WheelZoomAccumulatorTests::resetsOnDirectionChangeAndGestureEnd()
{
	WheelZoomAccumulator zoom;
	QCOMPARE(zoom.consume(60, Qt::ScrollUpdate), 0);
	QCOMPARE(zoom.consume(-60, Qt::ScrollUpdate), 0);
	QCOMPARE(zoom.consume(-60, Qt::ScrollUpdate), -1);
	QCOMPARE(zoom.consume(60, Qt::ScrollUpdate), 0);
	QCOMPARE(zoom.consume(0, Qt::ScrollEnd), 0);
	QCOMPARE(zoom.consume(60, Qt::ScrollBegin), 0);
}

void WheelZoomAccumulatorTests::resetsWhenControlZoomEnds()
{
	WheelZoomAccumulator zoom;
	QCOMPARE(zoom.consume(60), 0);
	zoom.reset();
	QCOMPARE(zoom.consume(60), 0);
	QCOMPARE(zoom.consume(60), 1);
}

QTEST_APPLESS_MAIN(WheelZoomAccumulatorTests)
#include "wheelzoomaccumulatortests.moc"
