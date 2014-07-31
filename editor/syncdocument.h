#ifndef SYNCDOCUMENT_H
#define SYNCDOCUMENT_H

#include <QStack>
#include <QList>
#include <QVector>
#include <QString>

#include "clientsocket.h"
#include "synctrack.h"

class SyncDocument : public QObject {
	Q_OBJECT
public:
	SyncDocument() :
	    rows(128),
	    savePointDelta(0),
	    savePointUnreachable(false)
	{
	}

	~SyncDocument();

	int createTrack(const QString &name)
	{
		tracks.append(new SyncTrack(name));
		int index = tracks.size() - 1;
		trackOrder.push_back(index);
		Q_ASSERT(trackOrder.size() == tracks.size());
		return index;
	}

	SyncTrack *getTrack(int index)
	{
		Q_ASSERT(index >= 0 && index < tracks.size());
		return tracks[index];
	}

	const SyncTrack *getTrack(int index) const
	{
		Q_ASSERT(index >= 0 && index < tracks.size());
		return tracks[index];
	}

	SyncTrack *findTrack(const QString &name)
	{
		for (int i = 0; i < tracks.size(); ++i)
			if (name == tracks[i]->name)
				return tracks[i];
		return NULL;
	}

	size_t getTrackCount() const
	{
		Q_ASSERT(trackOrder.size() == tracks.size());
		return tracks.size();
	}

	class Command
	{
	public:
		virtual ~Command() {}
		virtual void exec(SyncDocument *data) = 0;
		virtual void undo(SyncDocument *data) = 0;
	};
	
	class InsertCommand : public Command
	{
	public:
		InsertCommand(int track, const SyncTrack::TrackKey &key) :
		    track(track),
		    key(key)
		{}

		~InsertCommand() {}
		
		void exec(SyncDocument *data)
		{
			SyncTrack *t = data->getTrack(track);
			Q_ASSERT(!t->isKeyFrame(key.row));
			t->setKey(key);
		}
		
		void undo(SyncDocument *data)
		{
			SyncTrack *t = data->getTrack(track);
			Q_ASSERT(t->isKeyFrame(key.row));
			t->removeKey(key.row);
		}

	private:
		int track;
		SyncTrack::TrackKey key;
	};
	
	class DeleteCommand : public Command
	{
	public:
		DeleteCommand(int track, int row) : track(track), row(row) {}
		~DeleteCommand() {}
		
		void exec(SyncDocument *data)
		{
			SyncTrack *t = data->getTrack(track);
			Q_ASSERT(t->isKeyFrame(row));
			oldKey = t->getKeyFrame(row);
			Q_ASSERT(oldKey.row == row);
			t->removeKey(row);
		}
		
		void undo(SyncDocument *data)
		{
			SyncTrack *t = data->getTrack(track);
			Q_ASSERT(!t->isKeyFrame(row));
			Q_ASSERT(oldKey.row == row);
			t->setKey(oldKey);
		}

	private:
		int track, row;
		SyncTrack::TrackKey oldKey;
	};

	
	class EditCommand : public Command
	{
	public:
		EditCommand(int track, const SyncTrack::TrackKey &key) : track(track), key(key) {}
		~EditCommand() {}

		void exec(SyncDocument *data)
		{
			SyncTrack *t = data->getTrack(track);
			Q_ASSERT(t->isKeyFrame(key.row));
			oldKey = t->getKeyFrame(key.row);
			Q_ASSERT(key.row == oldKey.row);
			t->setKey(key);
		}

		void undo(SyncDocument *data)
		{
			SyncTrack *t = data->getTrack(track);
			Q_ASSERT(t->isKeyFrame(oldKey.row));
			Q_ASSERT(key.row == oldKey.row);
			t->setKey(oldKey);
		}

	private:
		int track;
		SyncTrack::TrackKey oldKey, key;
	};
	
	class MultiCommand : public Command
	{
	public:
		~MultiCommand()
		{
			while (!commands.isEmpty())
				delete commands.takeFirst();
		}
		
		void addCommand(Command *cmd)
		{
			commands.push_back(cmd);
		}
		
		size_t getSize() const { return commands.size(); }
		
		void exec(SyncDocument *data)
		{
			QListIterator<Command *> i(commands);
			while (i.hasNext())
				i.next()->exec(data);
		}
		
		void undo(SyncDocument *data)
		{
			QListIterator<Command *> i(commands);
			i.toBack();
			while (i.hasPrevious())
				i.previous()->undo(data);
		}
		
	private:
		QList<Command*> commands;
	};
	
	void exec(Command *cmd)
	{
		undoStack.push(cmd);
		cmd->exec(this);
		clearRedoStack();

		bool oldModified = modified();
		if (savePointDelta < 0)
			savePointUnreachable = true;
		savePointDelta++;
		if (oldModified != modified())
			emit modifiedChanged(modified());
	}
	
	bool undo();
	bool redo();
	void clearUndoStack();
	void clearRedoStack();
	bool modified() const;

	Command *getSetKeyFrameCommand(int track, const SyncTrack::TrackKey &key);

	size_t getTrackIndexFromPos(size_t track) const;
	void swapTrackOrder(size_t t1, size_t t2);

	static SyncDocument *load(const QString &fileName);
	bool save(const QString &fileName);

	bool isRowBookmark(int row) const;
	void toggleRowBookmark(int row);

	size_t getRows() const { return rows; }
	void setRows(size_t rows) { this->rows = rows; }

	QString fileName;

	int nextRowBookmark(int row) const;
	int prevRowBookmark(int row) const;

private:
	QList<SyncTrack*> tracks;
	QList<int> rowBookmarks;
	QVector<size_t> trackOrder;
	size_t rows;

	// undo / redo functionality
	QStack<Command*> undoStack;
	QStack<Command*> redoStack;
	int savePointDelta;        // how many undos must be done to get to the last saved state
	bool savePointUnreachable; // is the save-point reachable?

signals:
	void modifiedChanged(bool modified);
};

#endif // !defined(SYNCDOCUMENT_H)
