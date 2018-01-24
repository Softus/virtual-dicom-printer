/*
 * Copyright (C) 2014-2018 Softus Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "product.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QUtf8Settings>

#ifdef UNICODE
#define DCMTK_UNICODE_BUG_WORKAROUND
#undef UNICODE
#endif

#include "printscp.h"
#include "storescp.h"

#include <dcmtk/oflog/logger.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmpstat/dvpsdef.h>

// DCMTK prior to 3.6.1 has no its own namespace.
namespace dcmtk{}
using namespace dcmtk;

#ifdef DCMTK_UNICODE_BUG_WORKAROUND
#define UNICODE
#undef DCMTK_UNICODE_BUG_WORKAROUND
#endif

#define DEFAULT_SPOOL_INTERVAL 600

static int resendWorkerPid = 0;

static void cleanChildren()
{
    qDebug() << __func__;

#ifdef HAVE_WAITPID
    int status;
#elif HAVE_WAIT3
    struct rusage rusage;
#if defined(__NeXT__)
    // some systems need a union wait as argument to wait3
    union wait status;
#else
    int        status;
#endif
#endif

#if defined(HAVE_WAITPID) || defined(HAVE_WAIT3)
    int child = 1;
    int options = WNOHANG;
    while (child > 0)
    {
#ifdef HAVE_WAITPID
        child = (int)(waitpid(-1, &status, options));
#elif defined(HAVE_WAIT3)
        child = wait3(&status, options, &rusage);
#endif
        if (child < 0)
        {
            if (errno != ECHILD)
            {
                qDebug() << "wait for child failed: " << QString::fromLocal8Bit(strerror(errno));
            }
        }
        else if (child > 0)
        {
            qDebug() << "Child process" << child << "terminated with status" << status;
            if (resendWorkerPid == child)
            {
                resendWorkerPid = 0;
            }
        }

    }
#endif
}

static bool resendFailedPrints(QSettings& settings)
{
    // Retry failed prints
    //
    auto spoolPath = settings.value("spool-path").toString();
    if (spoolPath.isEmpty())
    {
        // Retry spool is disabled
        return false;
    }

    if (QDateTime::currentDateTime() < settings.value("next-spool-ts").toDateTime())
    {
        // Not yet. May be next time
        qDebug() << __func__ << "delayed";
        return false;
    }

    auto spoolInterval = settings.value("spool-interval-in-seconds", DEFAULT_SPOOL_INTERVAL).toInt();
    settings.setValue("next-spool-ts", QDateTime::currentDateTime().addSecs(spoolInterval));

    // Save it immediatelly to avoid corruption.
    //
    settings.sync();

#ifdef HAVE_FORK
    if (resendWorkerPid > 0)
    {
        qDebug() << "Worker process" << resendWorkerPid << "is still alive, resend delayed";
        return false;
    }

    resendWorkerPid = fork();

    if (resendWorkerPid > 0)
    {
        qDebug() << "Worker process to resend failed prints spawned. Pid" << resendWorkerPid;
        return false;
    }
#endif

    // Really start to process failed prints
    //
    OFCondition cond;

    // Retry failed web queries
    //
    qDebug() << __func__ << "retrying prints";
    Q_FOREACH (auto file, QDir(spoolPath).entryInfoList(QDir::Files))
    {
        auto filePath = file.absoluteFilePath();
        qDebug() << "Retrying " << filePath;

        DcmFileFormat dcmFF;
        cond = dcmFF.loadFile((const char*)filePath.toLocal8Bit());
        if (cond.bad())
        {
            qDebug() << "Failed to load " << filePath << ": " << QString::fromLocal8Bit(cond.text());
            continue;
        }

        const char* printer = nullptr;
        dcmFF.getDataset()->findAndGetString(DCM_RETIRED_PrintQueueID, printer);
        if (printer == nullptr)
        {
            qDebug() << "Failed to retry " << filePath << ": no printer instance specified";
            continue;
        }

        PrintSCP retryPrintSCP(nullptr, nullptr, QString::fromUtf8(printer));

        if (retryPrintSCP.webQuery(dcmFF.getDataset()))
        {
            foreach (auto server, settings.value("storage-servers").toStringList())
            {
                StoreSCP sscp(server);
                const char* SOPInstanceUID = nullptr;
                dcmFF.getDataset()->findAndGetString(DCM_SOPInstanceUID, SOPInstanceUID);

                cond = sscp.sendToServer(dcmFF.getDataset(), SOPInstanceUID);
                if (cond.bad())
                {
                    // The Web query secceded, but store failed.
                    // Move the file down to the queue.
                    // At this point, we will copy the dataset as many times,
                    // as need for each failed store server.
                    //
                    saveToDisk(QString(spoolPath).append(QDir::separator()).append(server), dcmFF.getDataset());
                }
            }

            if (!QFile::remove(filePath))
            {
                qDebug() << "Failed to remove file " << filePath
                         << ": " << QString::fromLocal8Bit(strerror(errno));
            }
        }
    }

    qDebug() << __func__ << "retrying dcmstore";
    foreach (auto server, settings.value("storage-servers").toStringList())
    {
        StoreSCP sscp(server);
        auto path = QDir(QString(spoolPath).append(QDir::separator()).append(server));
        Q_FOREACH (auto file, path.entryInfoList(QDir::Files))
        {
            auto filePath = file.absoluteFilePath();
            qDebug() << "Resending " << filePath;

            DcmFileFormat dcmFF;
            cond = dcmFF.loadFile((const char*)filePath.toLocal8Bit());
            if (cond.bad())
            {
                qDebug() << "Failed to load " << filePath
                         << ": " << QString::fromLocal8Bit(cond.text());
                continue;
            }

            const char* SOPInstanceUID = nullptr;
            dcmFF.getDataset()->findAndGetString(DCM_SOPInstanceUID, SOPInstanceUID);

            cond = sscp.sendToServer(dcmFF.getDataset(), SOPInstanceUID);
            if (cond.good())
            {
                if (!QFile::remove(filePath))
                {
                    qDebug() << "Failed to remove file " << filePath
                             << ": " << QString::fromLocal8Bit(strerror(errno));
                }
            }
        }
    }
    qDebug() << __func__ << "done";
    return true;
}

void handleClient(T_ASC_Association *assoc)
{
    PrintSCP printSCP(assoc);

    if (printSCP.negotiateAssociation())
    {
        printSCP.handleClient();
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(PRODUCT_SHORT_NAME);
    app.setOrganizationName(ORGANIZATION_DOMAIN);

    QUtf8Settings settings;
    auto logLevel = settings.value("log-level").toString();
    if (!logLevel.isEmpty())
    {
        auto level = log4cplus::getLogLevelManager().fromString(logLevel.toUtf8().constData());
        log4cplus::Logger::getRoot().setLogLevel(level);
    }

    auto debugUpstream = settings.value("debug-upstream").toBool();
    if (debugUpstream)
    {
        log4cplus::Logger log = log4cplus::Logger::getInstance("dcmtk.dcmpstat.dump");
        log.setLogLevel(OFLogger::DEBUG_LOG_LEVEL);
    }

    auto port = settings.value("port", DEFAULT_LISTEN_PORT).toInt();
    auto tout = settings.value("timeout", DEFAULT_TIMEOUT).toInt();
    T_ASC_Network *net;
    OFCondition cond = ASC_initializeNetwork(NET_ACCEPTOR, port, tout, &net);

    // When the listener process has been recently crashed,
    // the port will be busy for some time (see CLOSE_WAIT).
    //
    for (int i = 0; cond.bad() && i < 20; ++i)
    {
        usleep(200000);
        cond = ASC_initializeNetwork(NET_ACCEPTOR, port, tout, &net);
    }

    if (cond.bad())
    {
        qWarning() << "cannot initialize network" << QString::fromLocal8Bit(cond.text());
        return 1;
    }

#if defined(HAVE_SETUID) && defined(HAVE_GETUID)
    // Return to normal uid so that we can't do too much damage in case
    // things go very wrong.   Only relevant if the program is setuid root,
    // and run by another user.  Running as root user may be
    // potentially disasterous if this program screws up badly.
    //
    auto err = setuid(getuid());
    if (err)
    {
        qWarning() << "setuid failed" << errno << "this may be dangerous";
    }
#endif

#ifdef HAVE_FORK
    int listen_timeout=10;
#else
    int listen_timeout=10000;
#endif

    qCritical() << "Virtual DICOM printer version" << PRODUCT_VERSION_STR
             << "started. Master process pid" << getpid();

    Q_FOREVER
    {
        do
        {
           cleanChildren();
           if (resendFailedPrints(settings))
           {
               // Resend worker routine has been completed
               return 0;
           }
           qDebug() << "waiting for connection";
        }
        while (!ASC_associationWaiting(net, listen_timeout));

        qDebug() << "Client connected";

        T_ASC_Association *assoc = nullptr;
        cond = ASC_receiveAssociation(net, &assoc, DEFAULT_MAXPDU);
        if (cond.bad())
        {
            qWarning() << "Failed to receive association";
            ASC_dropSCPAssociation(assoc);
            ASC_destroyAssociation(&assoc);
            continue;
        }

#if defined(HAVE_FORK) && !defined(QT_DEBUG)
        auto pid = fork();
        if (pid < 0)
        {
            qDebug() << "fork() failed, err" << errno
                     << "\nWill handle client connection in the main process";
            handleClient(assoc);
        }
        else if (pid == 0)
        {
            // Do the real work.
            //
            handleClient(assoc);
            qDebug() << "Child process completed. pid" << getpid();
            return 0;
        }
        else
        {
            qDebug() << "Child process" << pid << "spawned";
        }
#else
        qDebug() << "Will handle client connection in the main process";
        handleClient(assoc);
#endif
    }

    return 0;
}
