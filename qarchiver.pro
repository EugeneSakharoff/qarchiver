QT -= gui

CONFIG += c++11 console
#CONFIG += c++11
#CONFIG += c++17 console
CONFIG -= app_bundle
QT += network sql

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        arcapplication.cpp \
        database.cpp \
        main.cpp

win32 {
    SOURCES += tooaccess_win.cpp
}

unix {
    SOURCES += tooaccess_linux.cpp
    SOURCES += spvwrap.cpp
}

#win32:INCLUDEPATH += E:/Shcnf_Include
#win32:DEPENDPATH += E:/Shcnf_Include
#win32 {
#    HEADERS += E:/Shcnf_Include/Consts.h
#    HEADERS += E:/Shcnf_Include/supvis.h
#}

unix {
    HEADERS += spvwrap.h
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

unix:INCLUDEPATH += /home/alexex/develop/trioAPI/include
unix:DEPENDPATH += /home/alexex/develop/trioAPI/include

unix:LIBS += -L/home/alexex/develop/trioAPI/lib -lTrioAPI

#win32:INCLUDEPATH += $$_PRO_FILE_PWD_/../postgreSQL/libpqxx-6.4.7/include
#win32:DEPENDPATH += $$_PRO_FILE_PWD_/../postgreSQL/libpqxx-6.4.7/include

#win32:CONFIG(release, debug|release): LIBS += -L$$_PRO_FILE_PWD_/../postgreSQL/libpqxx-6.4.7/lib64/Release/ -lpqxx
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$_PRO_FILE_PWD_/../postgreSQL/libpqxx-6.4.7/lib64/Debug -lpqxx

#win32:INCLUDEPATH  += $$_PRO_FILE_PWD_/../postgreSQL/libpqxx/include
#win32:DEPENDPATH  += $$_PRO_FILE_PWD_/../postgreSQL/libpqxx/include

#win32:CONFIG(release, debug|release): LIBS += -L$$_PRO_FILE_PWD_/../postgreSQL/libpqxx/lib64/Release/ -lpqxx
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$_PRO_FILE_PWD_/../postgreSQL/libpqxx/lib64/Debug -lpqxx

RESOURCES += \
    qarchiver.qrc

HEADERS += \
    arcapplication.h \
    database.h \
    main.h

#win32 {
win32:LIBS += User32.lib
win32:LIBS += advapi32.lib
win32:LIBS += Dbghelp.lib
#}

win32 {
QMAKE_LFLAGS_WINDOWS = /SUBSYSTEM:WINDOWS,6.01
DEFINES += "WINVER=0x0601"
DEFINES += "_WIN32_WINNT=0x0601"
}

#QMAKE_CXXFLAGS_WARN_ON += -W4
