#include "mainwindow.h"
#include "trackview.h"
#include "syncdocument.h"

#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QFileInfo>
#include <QSettings>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QTcpServer>
#include <QtEndian>

#ifdef QT_WEBSOCKETS_LIB
#include <QWebSocketServer>
#include <QWebSocket>
#endif

MainWindow::MainWindow() :
	QMainWindow(),
	clientSocket(NULL)
{
	trackView = new TrackView(this);
	setCentralWidget(trackView);

	connect(trackView, SIGNAL(posChanged(int, int)),
	        this, SLOT(onPosChanged(int, int)));
	connect(trackView, SIGNAL(currValDirty()),
	        this, SLOT(onCurrValDirty()));

	createMenuBar();
	updateRecentFiles();

	createStatusBar();

	tcpServer = new QTcpServer();
	connect(tcpServer, SIGNAL(newConnection()),
	        this, SLOT(onNewTcpConnection()));

	if (!tcpServer->listen(QHostAddress::Any, 1338))
		setStatusText(QString("Could not start server: %1").arg(tcpServer->errorString()));

#ifdef QT_WEBSOCKETS_LIB
	wsServer = new QWebSocketServer("GNU Rocket Editor", QWebSocketServer::NonSecureMode);
	connect(wsServer, SIGNAL(newConnection()),
	        this, SLOT(onNewWsConnection()));

	if (!wsServer->listen(QHostAddress::Any, 1339))
		setStatusText(QString("Could not start server: %1").arg(tcpServer->errorString()));
#endif
}

void MainWindow::showEvent(QShowEvent *event)
{
	QMainWindow::showEvent(event);

	// workaround for QTBUG-16507
	QString filePath = windowFilePath();
	setWindowFilePath(filePath + "foo");
	setWindowFilePath(filePath);
}


void MainWindow::createMenuBar()
{
	fileMenu = menuBar()->addMenu("&File");
	fileMenu->addAction(QIcon::fromTheme("document-new"), "New", this, SLOT(fileNew()), QKeySequence::New);
	fileMenu->addAction(QIcon::fromTheme("document-open"), "&Open", this, SLOT(fileOpen()), QKeySequence::Open);
	fileMenu->addAction(QIcon::fromTheme("document-save"), "&Save", this, SLOT(fileSave()), QKeySequence::Save);
	fileMenu->addAction(QIcon::fromTheme("document-save-as"),"Save &As", this, SLOT(fileSaveAs()), QKeySequence::SaveAs);
	fileMenu->addSeparator();
	fileMenu->addAction("Remote &Export", this, SLOT(fileRemoteExport()), Qt::CTRL + Qt::Key_E);
	recentFilesMenu = fileMenu->addMenu(QIcon::fromTheme("document-open-recent"), "Recent &Files");
	for (int i = 0; i < 5; ++i) {
		recentFileActions[i] = recentFilesMenu->addAction(QIcon::fromTheme("document-open-recent"), "");
		recentFileActions[i]->setVisible(false);
		connect(recentFileActions[i], SIGNAL(triggered()),
		        this, SLOT(openRecentFile()));
	}
	fileMenu->addSeparator();
	fileMenu->addAction(QIcon::fromTheme("application-exit"), "E&xit", this, SLOT(fileQuit()), QKeySequence::Quit);

	editMenu = menuBar()->addMenu("&Edit");
	editMenu->addAction(QIcon::fromTheme("edit-undo"), "Undo", trackView, SLOT(editUndo()), QKeySequence::Undo);
	editMenu->addAction(QIcon::fromTheme("edit-redo"), "Redo", trackView, SLOT(editRedo()), QKeySequence::Redo);
	editMenu->addSeparator();
	editMenu->addAction(QIcon::fromTheme("edit-copy"), "&Copy", trackView, SLOT(editCopy()), QKeySequence::Copy);
	editMenu->addAction(QIcon::fromTheme("edit-cut"), "Cu&t", trackView, SLOT(editCut()), QKeySequence::Cut);
	editMenu->addAction(QIcon::fromTheme("edit-paste"), "&Paste", trackView, SLOT(editPaste()), QKeySequence::Paste);
	editMenu->addAction(QIcon::fromTheme("edit-clear"), "Clear", trackView, SLOT(editClear()), QKeySequence::Delete);
	editMenu->addSeparator();
	editMenu->addAction(QIcon::fromTheme("edit-select-all"), "Select All", trackView, SLOT(selectAll()), QKeySequence::SelectAll);
	editMenu->addAction("Select Track", trackView, SLOT(selectTrack()), Qt::CTRL + Qt::Key_T);
	editMenu->addAction("Select Row", trackView, SLOT(selectRow()));
	editMenu->addSeparator();
	editMenu->addAction("Bias Selection", this, SLOT(editBiasSelection()), Qt::CTRL + Qt::Key_B);
	editMenu->addSeparator();
	editMenu->addAction("Set Rows", this, SLOT(editSetRows()), Qt::CTRL + Qt::Key_R);
	editMenu->addSeparator();
	editMenu->addAction("Previous Bookmark", this, SLOT(editPreviousBookmark()), Qt::ALT + Qt::Key_PageUp);
	editMenu->addAction("Next Bookmark", this, SLOT(editNextBookmark()), Qt::ALT + Qt::Key_PageDown);
}

void MainWindow::createStatusBar()
{
	statusPos = new QLabel;
	statusValue = new QLabel;
	statusKeyType = new QLabel;

	statusBar()->addPermanentWidget(statusPos);
	statusBar()->addPermanentWidget(statusValue);
	statusBar()->addPermanentWidget(statusKeyType);

	statusBar()->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

	setStatusText("Not connected");
	setStatusPosition(0, 0);
	setStatusValue(0.0f, false);
	setStatusKeyType(SyncTrack::TrackKey::STEP, false);
}

static QStringList getRecentFiles()
{
#ifdef Q_OS_WIN32
	QSettings settings("HKEY_CURRENT_USER\\Software\\GNU Rocket",
	                   QSettings::NativeFormat);
#else
	QSettings settings;
#endif
	QStringList list;
	for (int i = 0; i < 5; ++i) {
		QVariant string = settings.value(QString("RecentFile%1").arg(i));
		if (string.isValid())
			list.push_back(string.toString());
	}
	return list;
}

static void setRecentFiles(const QStringList &files)
{
#ifdef Q_OS_WIN32
	QSettings settings("HKEY_CURRENT_USER\\Software\\GNU Rocket",
	                   QSettings::NativeFormat);
#else
	QSettings settings;
#endif

	for (int i = 0; i < files.size(); ++i)
		settings.setValue(QString("RecentFile%1").arg(i), files[i]);

	// remove keys not in the list
	for (int i = files.size(); ;++i) {
		QString key = QString("RecentFile%1").arg(i);

		if (!settings.contains(key))
			break;

		settings.remove(key);
	}
}

void MainWindow::updateRecentFiles()
{
	QStringList files = getRecentFiles();

	if (!files.size()) {
		recentFilesMenu->setEnabled(false);
		return;
	}

	Q_ASSERT(files.size() <= 5);
	for (int i = 0; i < files.size(); ++i) {
		QFileInfo info(files[i]);
		QString text = QString("&%1 %2").arg(i + 1).arg(info.fileName());

		recentFileActions[i]->setText(text);
		recentFileActions[i]->setData(info.absoluteFilePath());
		recentFileActions[i]->setVisible(true);
	}
	for (int i = files.size(); i < 5; ++i)
		recentFileActions[i]->setVisible(false);
	recentFilesMenu->setEnabled(true);
}

void MainWindow::setCurrentFileName(const QString &fileName)
{
	QFileInfo info(fileName);

	QStringList files = getRecentFiles();
	files.removeAll(info.absoluteFilePath());
	files.prepend(info.absoluteFilePath());
	while (files.size() > 5)
		files.removeLast();
	setRecentFiles(files);

	updateRecentFiles();
	setWindowFilePath(fileName);
}

void MainWindow::setStatusText(const QString &text)
{
	statusBar()->showMessage(text);
}

void MainWindow::setStatusPosition(int col, int row)
{
	statusPos->setText(QString("Row %1, Col %2").arg(row).arg(col));
}

void MainWindow::setStatusValue(double val, bool valid)
{
	if (valid)
		statusValue->setText(QString::number(val, 'f', 3));
	else
		statusValue->setText("---");
}

void MainWindow::setStatusKeyType(SyncTrack::TrackKey::KeyType keyType, bool valid)
{
	if (!valid) {
		statusKeyType->setText("---");
		return;
	}

	switch (keyType) {
	case SyncTrack::TrackKey::STEP:   statusKeyType->setText("step"); break;
	case SyncTrack::TrackKey::LINEAR: statusKeyType->setText("linear"); break;
	case SyncTrack::TrackKey::SMOOTH: statusKeyType->setText("smooth"); break;
	case SyncTrack::TrackKey::RAMP:   statusKeyType->setText("ramp"); break;
	default: Q_ASSERT(false);
	}
}

void MainWindow::setDocument(SyncDocument *newDoc)
{
	SyncDocument *oldDoc = trackView->getDocument();

	if (oldDoc)
		QObject::disconnect(oldDoc, SIGNAL(modifiedChanged(bool)),
		                    this, SLOT(setWindowModified(bool)));

	if (oldDoc && clientSocket) {
		// delete old key frames
		for (int i = 0; i < oldDoc->getTrackCount(); ++i) {
			SyncTrack *t = oldDoc->getTrack(i);
			QMap<int, SyncTrack::TrackKey> keyMap = t->getKeyMap();
			QMap<int, SyncTrack::TrackKey>::const_iterator it;
			for (it = keyMap.constBegin(); it != keyMap.constEnd(); ++it)
				t->removeKey(it.key());
			QObject::disconnect(t, SIGNAL(keyFrameChanged(const SyncTrack &, int)),
			        clientSocket, SLOT(onKeyFrameChanged(const SyncTrack &, int)));
		}

		if (newDoc) {
			// add back missing client-tracks
			QStringList trackNames = clientSocket->getTrackNames();
			for (int i = 0; i < trackNames.size(); ++i) {
				SyncTrack *t = newDoc->findTrack(trackNames[i]);
				if (!t)
					newDoc->createTrack(trackNames[i]);
			}

			for (int i = 0; i < newDoc->getTrackCount(); ++i) {
				SyncTrack *t = newDoc->getTrack(i);
				QMap<int, SyncTrack::TrackKey> keyMap = t->getKeyMap();
				QMap<int, SyncTrack::TrackKey>::const_iterator it;
				for (it = keyMap.constBegin(); it != keyMap.constEnd(); ++it)
					clientSocket->sendSetKeyCommand(t->name.toUtf8().constData(), *it);
				QObject::connect(t, SIGNAL(keyFrameChanged(const SyncTrack &, int)),
						 clientSocket, SLOT(onKeyFrameChanged(const SyncTrack &, int)));
			}
		}
	}

	trackView->setDocument(newDoc);
	trackView->dirtyCurrentValue();
	trackView->viewport()->update();

	QObject::connect(newDoc, SIGNAL(modifiedChanged(bool)),
	                 this, SLOT(setWindowModified(bool)));

	if (oldDoc)
		delete oldDoc;
}

void MainWindow::fileNew()
{
	setDocument(new SyncDocument);
	setWindowFilePath("Untitled");
}

bool MainWindow::loadDocument(const QString &path)
{
	SyncDocument *newDoc = SyncDocument::load(path);
	if (newDoc) {
		// set new document
		setDocument(newDoc);
		setCurrentFileName(path);
		return true;
	}
	return false;
}

void MainWindow::fileOpen()
{
	QString fileName = QFileDialog::getOpenFileName(this, "Open File", "", "ROCKET File (*.rocket);;All Files (*.*)");
	if (fileName.length()) {
		loadDocument(fileName);
	}
}

void MainWindow::fileSaveAs()
{
	QString fileName = QFileDialog::getSaveFileName(this, "Save File", "", "ROCKET File (*.rocket);;All Files (*.*)");
	if (fileName.length()) {
		SyncDocument *doc = trackView->getDocument();
		if (doc->save(fileName)) {
			if (clientSocket)
				clientSocket->sendSaveCommand();

			setCurrentFileName(fileName);
			doc->fileName = fileName;
		}
	}
}

void MainWindow::fileSave()
{
	SyncDocument *doc = trackView->getDocument();
	if (doc->fileName.isEmpty())
		return fileSaveAs();

	if (!doc->save(doc->fileName))
		fileRemoteExport();
}

void MainWindow::fileRemoteExport()
{
	if (clientSocket)
		clientSocket->sendSaveCommand();
}

void MainWindow::openRecentFile()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (action) {
		QString fileName = action->data().toString();
		if (!loadDocument(fileName)) {
			QStringList files = getRecentFiles();
			files.removeAll(fileName);
			setRecentFiles(files);
			updateRecentFiles();
		}
	}
}

void MainWindow::fileQuit()
{
	SyncDocument *doc = trackView->getDocument();
	if (doc->isModified()) {
		QMessageBox::StandardButton res = QMessageBox::question(
		    this, "GNU Rocket", "Save before exit?",
		    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
		if (res == QMessageBox::Yes) {
			fileSave();
			QApplication::quit();
		} else if (res == QMessageBox::No)
			QApplication::quit();
	}
	else QApplication::quit();
}

void MainWindow::editBiasSelection()
{
	bool ok = false;
	float bias = QInputDialog::getDouble(this, "Bias Selection", "", 0, INT_MIN, INT_MAX, 1, &ok);
	if (ok)
		trackView->editBiasValue(bias);
}

void MainWindow::editSetRows()
{
	bool ok = false;
	int rows = QInputDialog::getInt(this, "Set Rows", "", trackView->getRows(), 0, INT_MAX, 1, &ok);
	if (ok)
		trackView->setRows(rows);
}

void MainWindow::editPreviousBookmark()
{
	int row = trackView->getDocument()->prevRowBookmark(trackView->getEditRow());
	if (row >= 0)
		trackView->setEditRow(row);
}

void MainWindow::editNextBookmark()
{
	int row = trackView->getDocument()->nextRowBookmark(trackView->getEditRow());
	if (row >= 0)
		trackView->setEditRow(row);
}

void MainWindow::onPosChanged(int col, int row)
{
	setStatusPosition(col, row);
	if (trackView->paused && clientSocket)
		clientSocket->sendSetRowCommand(row);
}

void MainWindow::onCurrValDirty()
{
	SyncDocument *doc = trackView->getDocument();
	if (doc && doc->getTrackCount() > 0) {
		const SyncTrack *t = doc->getTrack(doc->getTrackIndexFromPos(trackView->getEditTrack()));
		int row = trackView->getEditRow();

		setStatusValue(t->getValue(row), true);

		const SyncTrack::TrackKey *k = t->getPrevKeyFrame(row);
		if (k)
			setStatusKeyType(k->type, true);
		else
			setStatusKeyType(SyncTrack::TrackKey::STEP, false);
	} else {
		setStatusValue(0.0f, false);
		setStatusKeyType(SyncTrack::TrackKey::STEP, false);
	}
}

void MainWindow::onTrackRequested(const QString &trackName)
{
	SyncDocument *doc = trackView->getDocument();

	// find track
	const SyncTrack *t = doc->findTrack(trackName.toUtf8());
	if (!t)
		t = doc->createTrack(trackName);

	// hook up signals to slots
	QObject::connect(t, SIGNAL(keyFrameChanged(const SyncTrack &, int)),
	                 clientSocket, SLOT(onKeyFrameChanged(const SyncTrack &, int)));

	// send key frames
	QMap<int, SyncTrack::TrackKey> keyMap = t->getKeyMap();
	QMap<int, SyncTrack::TrackKey>::const_iterator it;
	for (it = keyMap.constBegin(); it != keyMap.constEnd(); ++it)
		clientSocket->sendSetKeyCommand(t->name.toUtf8().constData(), *it);

	trackView->update();
}

void MainWindow::onRowChanged(int row)
{
	trackView->setEditRow(row);
}

void MainWindow::onNewTcpConnection()
{
	QTcpSocket *pendingSocket = tcpServer->nextPendingConnection();
	if (!clientSocket) {
		setStatusText("Accepting...");

		QByteArray greeting = QString(CLIENT_GREET).toUtf8();
		QByteArray response = QString(SERVER_GREET).toUtf8();

		if (pendingSocket->bytesAvailable() < 1)
			pendingSocket->waitForReadyRead();
		QByteArray line = pendingSocket->read(greeting.length());
		if (line != greeting ||
		    pendingSocket->write(response) != response.length()) {
			pendingSocket->close();

			setStatusText(QString("Not Connected: %1").arg(tcpServer->errorString()));
			return;
		}

		ClientSocket *client = new AbstractSocketClient(pendingSocket);

		connect(trackView, SIGNAL(pauseChanged(bool)), client, SLOT(onPauseChanged(bool)));

		connect(client, SIGNAL(trackRequested(const QString &)), this, SLOT(onTrackRequested(const QString &)));
		connect(client, SIGNAL(rowChanged(int)), this, SLOT(onRowChanged(int)));
		connect(client, SIGNAL(connected()), this, SLOT(onConnected()));
		connect(client, SIGNAL(disconnected()), this, SLOT(onDisconnected()));

		setStatusText(QString("Connected to %1").arg(pendingSocket->peerAddress().toString()));
		clientSocket = client;

		onConnected();
	} else
		pendingSocket->close();
}

#ifdef QT_WEBSOCKETS_LIB

void MainWindow::onNewWsConnection()
{
	QWebSocket *pendingSocket = wsServer->nextPendingConnection();

	if (!clientSocket) {
		setStatusText("Accepting...");

		ClientSocket *client = new WebSocketClient(pendingSocket);
		setStatusText(QString("Connected to %1").arg(pendingSocket->peerAddress().toString()));
		clientSocket = client;

		connect(trackView, SIGNAL(pauseChanged(bool)), client, SLOT(onPauseChanged(bool)));

		connect(client, SIGNAL(trackRequested(const QString &)), this, SLOT(onTrackRequested(const QString &)));
		connect(client, SIGNAL(rowChanged(int)), this, SLOT(onRowChanged(int)));
		connect(client, SIGNAL(connected()), this, SLOT(onConnected()));
		connect(client, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
	} else
		pendingSocket->close();
}

#endif

void MainWindow::onConnected()
{
	clientSocket->sendPauseCommand(trackView->paused);
	clientSocket->sendSetRowCommand(trackView->getEditRow());
	trackView->connected = true;
}

void MainWindow::onDisconnected()
{
	trackView->paused = true;

	// disconnect track-signals
	SyncDocument *doc = trackView->getDocument();
	for (int i = 0; i < doc->getTrackCount(); ++i)
		QObject::disconnect(doc->getTrack(i), SIGNAL(keyFrameChanged(const SyncTrack &, int)),
		clientSocket, SLOT(onKeyFrameChanged(const SyncTrack &, int)));

	if (clientSocket) {
		delete clientSocket;
		clientSocket = NULL;
	}

	trackView->update();
	setStatusText("Not Connected.");
	trackView->connected = false;
}
