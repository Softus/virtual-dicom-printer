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

#ifndef STORESCP_H
#define STORESCP_H

#include <QObject>

#define HAVE_CONFIG_H
#include <dcmtk/config/osconfig.h> /* make sure OS specific configuration is included first */
#include <dcmtk/dcmpstat/dvpstyp.h>         /* for enum types */
#include <dcmtk/dcmnet/dimse.h>

class DicomImage;
class T_ASC_Association;

class StoreSCP : public QObject
{
    Q_OBJECT
public:
    explicit StoreSCP(const QString& server, QObject *parent = 0);
    ~StoreSCP();

    bool sendToServer(DcmDataset* rqDataset, const char* sopInstance);

signals:

public slots:

private:
    T_ASC_Parameters* initAssocParams(const QString& peerAet, const QString& peerAddress, int timeout,
                                      const char *abstractSyntax, const char* transferSyntax);

    bool cStoreRQ(DcmDataset* dset, const char *abstractSyntax, const char* sopInstance);

    /** destroys the association managed by this object.
     */
    void dropAssociation();

    QString server;

    // blocking mode for receive
    //
    T_DIMSE_BlockingMode blockMode;

    // timeout for receive
    //
    int timeout;

    // the DICOM network and listen port
    //
    T_ASC_Network *net;

    // the network association over which the print SCP is operating
    //
    T_ASC_Association *assoc;

    T_ASC_PresentationContextID presId;
};

#endif // STORESCP_H
