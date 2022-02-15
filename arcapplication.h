#ifndef QXARCAPPLICATION_H
#define QXARCAPPLICATION_H

#include <QObject>
#include <QCoreApplication>
#include <QUdpSocket>
#include <QSet>
#include <QTimerEvent>

class MLibAccess;
struct VDATA;
struct MVARREC;
struct MVARD;

/////////////////////////////////////////////////////////////////////////////
// class QxArcApplication
/////////////////////////////////////////////////////////////////////////////

class QxArcApplication : public QCoreApplication
{
    Q_OBJECT

private:
    QUdpSocket m_quitSocket;
    QSet<int> m_changedVars;
    int m_timerId;

public:
    MLibAccess* m_pLibAccess;

public:
    QxArcApplication( int & argc, char** argv );
    virtual ~QxArcApplication();

private:
    QString processMessage( const QString & strMsg, const MVARREC & mvarrec ) const;
    QString convVarVal( const QVariant & var, int nDigits, bool bString ) const;
    uint codeFromChar( QChar ch ) const;
    QString makeHtml( const QString & strText ) const;
    void updateMinMaxVals( MVARD & mvardRef );
    void takePrevVal( MVARD & mvardRef, double dblVal, double & dblPrevVal, qint64 & prevNowTimeU64, qint64 & prevSourceTimeU64 );
    void takeMinMaxVal( MVARD & mvardRef, double dblVal, double & dblMin, double & dblMax );

signals:

private slots:
    void initApp();
    void onInpVarChanged( const VDATA & vdata );
    void readQuitDatagram();
    void onAboutToQuit();
    virtual void timerEvent( QTimerEvent* event );
};

#endif // QXARCAPPLICATION_H
