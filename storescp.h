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

#ifndef STORESCP_H
#define STORESCP_H

#include <QObject>

#ifdef UNICODE
#define DCMTK_UNICODE_BUG_WORKAROUND
#undef UNICODE
#endif

#define HAVE_CONFIG_H
#include <dcmtk/config/osconfig.h> /* make sure OS specific configuration is included first */
#include <dcmtk/dcmpstat/dvpstyp.h> /* for enum types */
#include <dcmtk/dcmnet/dimse.h>

#ifdef DCMTK_UNICODE_BUG_WORKAROUND
#define UNICODE
#undef DCMTK_UNICODE_BUG_WORKAROUND
#endif

class DicomImage;
struct T_ASC_Association;

class StoreSCP : public QObject
{
    Q_OBJECT

public:
    explicit StoreSCP(const QString& server, QObject *parent = 0);
    ~StoreSCP();

    /** transfers the dataset to the storage server.
     *  @param dataset to send
     *  @param sopInstance unique identifier of the dataset
     *  @return result indicating whether transfer was successful
     */
    OFCondition sendToServer(DcmDataset* dataset, const char* sopInstance);

private:
    /** prepares connection parameters for the Store SCP.
     *  @param peerAet called AETITLE of the server
     *  @param peerAddress network address of the server
     *  @param timeout timeout for network operations, in seconds
     *  @param abstractSyntax SOP class from the dataset
     *  @param transferSyntax transfer syntax from the dataset
     *  @return result indicating whether association negotiation was successful,
     *    unsuccessful or whether termination of the server was requested.
     */
    T_ASC_Parameters* initAssocParams(const QString& peerAet, const QString& peerAddress, int timeout,
                                      const char *abstractSyntax, const char* transferSyntax);

    /** transfers the dataset to the storage server.
     *  @param dataset to send
     *  @param abstractSyntax SOP class from the dataset
     *  @param sopInstance unique identifier of the dataset
     *  @return result indicating whether transfer was successful
     */
    OFCondition cStoreRQ(DcmDataset* dataset, const char *abstractSyntax, const char* sopInstance);

    /** destroys the association managed by this object.
     */
    void dropAssociation();

    // Our section in the configuration file
    //
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

    // Transfer context (usually LittleEndianExplicit)
    //
    T_ASC_PresentationContextID presId;
};

#endif // STORESCP_H
