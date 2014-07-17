/*
 * Copyright (C) 2014 Irkutsk Diagnostic Center.
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
#include "printscp.h"

#include <QCoreApplication>
#include <QDebug>

static void cleanChildren()
{
#ifdef HAVE_WAITPID
    int stat_loc;
#elif HAVE_WAIT3
    struct rusage rusage;
#if defined(__NeXT__)
    /* some systems need a union wait as argument to wait3 */
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
        child = (int)(waitpid(-1, &stat_loc, options));
#elif defined(HAVE_WAIT3)
        child = wait3(&status, options, &rusage);
#endif
        if (child < 0)
        {
            if (errno != ECHILD)
            {
                qDebug() << "wait for child failed: " << errno;
            }
        }
    }
#endif
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(PRODUCT_SHORT_NAME);
    app.setOrganizationName(ORGANIZATION_DOMAIN);

    QSettings settings;
    auto port = settings.value("port", DEFAULT_LISTEN_PORT).toInt();
    auto tout = settings.value("timeout", DEFAULT_TIMEOUT).toInt();
    T_ASC_Network *net;
    OFCondition cond = ASC_initializeNetwork(NET_ACCEPTOR, port, tout, &net);
    if (cond.bad())
    {
        qDebug() << "cannot initialise network" << cond.text();
        return 1;
    }

#if defined(HAVE_SETUID) && defined(HAVE_GETUID)
    /* return to normal uid so that we can't do too much damage in case
     * things go very wrong.   Only relevant if the program is setuid root,
     * and run by another user.  Running as root user may be
     * potentially disasterous if this program screws up badly.
     */
    setuid(getuid());
#endif

#ifdef HAVE_FORK
    int listen_timeout=1;
#else
    int listen_timeout=1000;
#endif

    Q_FOREVER
    {
        // Use new print SCP object for each association
        //
        PrintSCP printSCP;

        do
        {
           cleanChildren();
        } while (!ASC_associationWaiting(net, listen_timeout));

        // Ready to accept an association
        //
        auto ass = printSCP.negotiateAssociation(net);

        if (DVPSJ_error == ass)
        {
            // Association has already been deleted,
            // we just wait for the next client to connect.
        }
        else if (DVPSJ_terminate == ass)
        {
            // Our mission is over
            //
            ASC_dropNetwork(&net);
            break;
        }
        else if (DVPSJ_success == ass)
        {
            // A new client just been connected.
            // Do the real work.
            //
            printSCP.handleClient();
        }
    }

    cleanChildren();
    return 0;
}
