#include "syncpage.h"

#include <syncdocument.h>

SyncPage::SyncPage(SyncDocument *document, const QString &name) :
    QObject(document),
    document(document),
    name(name)
{
}

SyncTrack *SyncPage::getTrack(int index)
{
	Q_ASSERT(index >= 0 && index < trackOrder.size());
	return trackOrder[index];
}

const SyncTrack *SyncPage::getTrack(int index) const
{
	Q_ASSERT(index >= 0 && index < trackOrder.size());
	return trackOrder[index];
}

void SyncPage::addTrack(SyncTrack *track)
{
	trackOrder.push_back(track);
	QObject::connect(track, SIGNAL(keyFrameAdded(const SyncTrack &, int)),
	                 this,  SLOT(onKeyFrameAdded(const SyncTrack &, int)));
	QObject::connect(track, SIGNAL(keyFrameChanged(const SyncTrack &, int, const SyncTrack::TrackKey &)),
	                 this,  SLOT(onKeyFrameChanged(const SyncTrack &, int, const SyncTrack::TrackKey &)));
	QObject::connect(track, SIGNAL(keyFrameRemoved(const SyncTrack &, int)),
	                 this,  SLOT(onKeyFrameRemoved(const SyncTrack &, int)));
}

void SyncPage::swapTrackOrder(int t1, int t2)
{
	Q_ASSERT(0 <= t1 && t1 < trackOrder.size());
	Q_ASSERT(0 <= t2 && t2 < trackOrder.size());
	std::swap(trackOrder[t1], trackOrder[t2]);
	invalidateTrack(*trackOrder[t1]);
	invalidateTrack(*trackOrder[t2]);
}

void SyncPage::invalidateTrack(const SyncTrack &track)
{
	int trackIndex = trackOrder.indexOf((SyncTrack*)&track);
	Q_ASSERT(trackIndex >= 0);
	emit trackHeaderChanged(trackIndex);
	emit trackDataChanged(trackIndex, 0, document->getRows());
}

void SyncPage::invalidateTrackData(const SyncTrack &track, int start, int stop)
{
	int trackIndex = trackOrder.indexOf((SyncTrack*)&track);
	Q_ASSERT(trackIndex >= 0);
	emit trackDataChanged(trackIndex, start, stop);
}
