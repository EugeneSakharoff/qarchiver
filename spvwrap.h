#ifndef SPVWRAP_H
#define SPVWRAP_H

#include "trioAPI.h"

#include <QtGlobal>
#include <QString>

/////////////////////////////////////////////////////////////////////////////
// class CSpv
/////////////////////////////////////////////////////////////////////////////

class CSpv
{
protected:
    bool m_bAttached;

public:
    CSpv();
    ~CSpv();

protected:
    void stopGetVarThread() const;

public:
    void detachTrioTask();
    int attachTrioTask( uint32_t dwFlags, const QString & strTaskName, QString & strErr );
    QString errorTextForm( int nErrorCode );
    long derefToVarKey( long iSRPPOID, const QString & sName, int iDerefFlags );
    long propertiesOperation( void* pComlexResult, long iID, const QString & sName, int iCmd );
    long waitForNewVar( void* pComplexVal, TUAPIe_W4NV_cmd eCmd );
    bool waitSynchroStart( QString & strErr );
    bool universalWriteVar( TUAPI_key iVarKey, const void* pValue, int iVarState, int iFlags );
    bool inputQueueContol( struct pollfd* p_pollfld, TUAPIe_IQS_Cmd nCmd );
    bool isGetVarThreadRunning() const;
};

#endif // SPVWRAP_H
