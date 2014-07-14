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
                char buf[256];
                qDebug() << "wait for child failed: " << OFStandard::strerror(errno, buf, sizeof(buf));
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

    PrintSCP printSCP;

    OFCondition cond = printSCP.initNetwork();
    if (cond.bad())
    {
        qDebug() << "cannot initialise network" << cond.text();
        return 1;
    }

    Q_FOREVER
    {
        do
        {
           cleanChildren();
        } while (!printSCP.associationWaiting());

        auto ass = printSCP.negotiateAssociation();

        if (DVPSJ_error == ass)
        {
            // association has already been deleted, we just wait for the next client to connect.
        }
        else if (DVPSJ_terminate == ass)
        {
            break;
        }
        else if (DVPSJ_success == ass)
        {
            printSCP.handleClient();
        }
    }

    cleanChildren();
    return 0;
}
