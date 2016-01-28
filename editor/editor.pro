TEMPLATE = app
TARGET = editor
DEPENDPATH += .
INCLUDEPATH += .
QMAKE_CLEAN += $(QMAKE_TARGET)
CONFIG += silent

QT = core gui xml network

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets

    qtHaveModule(websockets): QT += websockets
}

!contains(QT, websockets): message("QWebSockets module not found, disabling websocket support...")

# Input
HEADERS += clientsocket.h \
    mainwindow.h \
    syncdocument.h \
    synctrack.h \
    trackview.h

SOURCES += clientsocket.cpp \
    editor.cpp \
    mainwindow.cpp \
    syncdocument.cpp \
    trackview.cpp

RESOURCES += editor.qrc

RC_FILE = editor.rc
ICON = appicon.icns
