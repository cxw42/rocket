#ifndef CLIENTSOCKET_H
#define CLIENTSOCKET_H

#include <QTcpSocket>
#include <QByteArray>
#include <QObject>
#include <QStringList>

#include "synctrack.h"

#define CLIENT_GREET "hello, synctracker!"
#define SERVER_GREET "hello, demo!"

enum {
	SET_KEY = 0,
	DELETE_KEY = 1,
	GET_TRACK = 2,
	SET_ROW = 3,
	PAUSE = 4,
	SAVE_TRACKS = 5
};

class ClientSocket : public QObject {
	Q_OBJECT

public:
	virtual void close() = 0;
	virtual qint64 sendData(const QByteArray &data) = 0;

	void sendSetKeyCommand(const QString &trackName, const SyncTrack::TrackKey &key);
	void sendDeleteKeyCommand(const QString &trackName, int row);
	void sendSetRowCommand(int row);
	void sendPauseCommand(bool pause);
	void sendSaveCommand();

	QMap<QString, size_t> clientTracks;
	const QStringList getTrackNames() { return clientTracks.keys(); }

signals:
	void connected();
	void disconnected();
	void trackRequested(const QString &trackName);
	void rowChanged(int row);

public slots:
	void onPauseChanged(bool paused)
	{
		sendPauseCommand(paused);
	}

	void onKeyFrameChanged(const SyncTrack &track, int row)
	{
		if (track.isKeyFrame(row))
			sendSetKeyCommand(track.name, track.getKeyFrame(row));
		else
			sendDeleteKeyCommand(track.name, row);
	}

protected slots:
	void onDisconnected()
	{
		emit disconnected();
	}
};

class AbstractSocketClient : public ClientSocket {
	Q_OBJECT
public:
	explicit AbstractSocketClient(QAbstractSocket *socket) : socket(socket)
	{
		connect(socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
		connect(socket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
	}

	virtual void close()
	{
		disconnect(socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
		disconnect(socket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
		socket->close();
	}

	qint64 sendData(const QByteArray &data)
	{
		qint64 ret = socket->write(data);
		socket->flush();
		return ret;
	}

private:
	QAbstractSocket *socket;
	bool recv(char *buffer, qint64 length);

	void processCommand();
	void processGetTrack();
	void processSetRow();

private slots:
	void onReadyRead();
};

#ifdef QT_WEBSOCKETS_LIB

class QWebSocket;

class WebSocketClient : public ClientSocket {
	Q_OBJECT
public:
	explicit WebSocketClient(QWebSocket *socket);
	void close();
	qint64 sendData(const QByteArray &data);

public slots:
	void processTextMessage(const QString &message);
	void onMessageReceived(const QByteArray &data);

private:
	QWebSocket *socket;
};

#endif

#endif // !defined(CLIENTSOCKET_H)
