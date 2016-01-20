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
}
