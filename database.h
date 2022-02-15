#ifndef DATABASE_H
#define DATABASE_H

#include <QDateTime>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>
#include <QtGlobal>

bool connect_database( const QString & db_name, const QString & strUserName,
                       const QString & strPassword, bool bCreate, QString & strErr );
bool create_table( const QString & table, const QString & structure, QString & strErr );
void add_message_to_db(int idx,  const QString & message, qint64 time );
void add_value_change_to_db( int id, double value, qint64 time );
void closeDb();
bool populateDescriptionTable( QString & strErr );
bool populateGroupsTable( QString & strErr );
bool prepareRunQueries( QString & strErr );

#endif //  DATABASE_H
