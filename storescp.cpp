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

#include <QCoreApplication>
#include <QDebug>
#include "QUtf8Settings"

#ifdef UNICODE
#define DCMTK_UNICODE_BUG_WORKAROUND
#undef UNICODE
#endif

#include "storescp.h"
#include <dcmtk/dcmdata/dcdeftag.h>

#ifdef DCMTK_UNICODE_BUG_WORKAROUND
#define UNICODE
#undef DCMTK_UNICODE_BUG_WORKAROUND
#endif

StoreSCP::StoreSCP(const QString& server, QObject *parent)
    : QObject(parent)
    , server(server)
    , blockMode(DIMSE_BLOCKING)
    , timeout(0)
    , net(nullptr)
    , assoc(nullptr)
{
    QUtf8Settings settings;
    blockMode = (T_DIMSE_BlockingMode)settings.value("block-mode", blockMode).toInt();
    timeout   = settings.value("timeout", timeout).toInt();
}

StoreSCP::~StoreSCP()
{
    dropAssociation();
    ASC_dropNetwork(&net);
}

void StoreSCP::dropAssociation()
{
    if (assoc)
    {
        ASC_dropSCPAssociation(assoc);
        ASC_destroyAssociation(&assoc);
        assoc = nullptr;
    }
}

T_ASC_Parameters* StoreSCP::initAssocParams(const QString& peerAet, const QString& peerAddress, int timeout,
                                            const char* abstractSyntax, const char* transferSyntax)
{
    QUtf8Settings settings;

    DIC_NODENAME localHost;
    T_ASC_Parameters* params = nullptr;

    auto cond = ASC_initializeNetwork(NET_REQUESTOR, settings.value("store-port").toInt(), timeout, &net);
    if (cond.good())
    {
        cond = ASC_createAssociationParameters(&params, settings.value("store-pdu-size", ASC_DEFAULTMAXPDU).toInt());
        if (cond.good())
        {
            auto appAet = settings.value("store-aetitle", qApp->applicationName()).toString().toUpper().toUtf8();
            ASC_setAPTitles(params, appAet, peerAet.toUtf8(), nullptr);

            /* Figure out the presentation addresses and copy the */
            /* corresponding values into the DcmAssoc parameters.*/
            gethostname(localHost, sizeof(localHost) - 1);
            ASC_setPresentationAddresses(params, localHost, peerAddress.toUtf8());

            if (transferSyntax)
            {
                const char* arr[] = { transferSyntax };
                cond = ASC_addPresentationContext(params, 1, abstractSyntax, arr, 1);
            }
            else
            {
                /* Set the presentation contexts which will be negotiated */
                /* when the network connection will be established */
                const char* transferSyntaxes[] =
                {
#if __BYTE_ORDER == __LITTLE_ENDIAN
                    UID_LittleEndianExplicitTransferSyntax, UID_BigEndianExplicitTransferSyntax,
#elif __BYTE_ORDER == __BIG_ENDIAN
                    UID_BigEndianExplicitTransferSyntax, UID_LittleEndianExplicitTransferSyntax,
#else
#error "Unsupported byte order"
#endif
                    UID_LittleEndianImplicitTransferSyntax
                };

                cond = ASC_addPresentationContext(params, 1, abstractSyntax,
                    transferSyntaxes, sizeof(transferSyntaxes)/sizeof(transferSyntaxes[0]));
            }

            if (cond.good())
            {
                return params;
            }

            ASC_destroyAssociationParameters(&params);
        }
    }

    qDebug() << QString::fromLocal8Bit(cond.text());
    return nullptr;
}

OFCondition StoreSCP::cStoreRQ(DcmDataset* dataset, const char* abstractSyntax, const char* sopInstance)
{
    T_DIMSE_C_StoreRQ req;
    T_DIMSE_C_StoreRSP rsp;
    bzero((char*)&req, sizeof(req));
    bzero((char*)&rsp, sizeof(rsp));
    DcmDataset *statusDetail = nullptr;

    /* prepare the transmission of data */
    req.MessageID = assoc->nextMsgID++;
    strcpy(req.AffectedSOPClassUID, abstractSyntax);
    strcpy(req.AffectedSOPInstanceUID, sopInstance);
    req.DataSetType = DIMSE_DATASET_PRESENT;
    req.Priority = DIMSE_PRIORITY_LOW;

    /* finally conduct transmission of data */
    auto cond = DIMSE_storeUser(assoc, presId, &req, nullptr, dataset, nullptr, nullptr,
        0 == timeout? DIMSE_BLOCKING: DIMSE_NONBLOCKING, timeout, &rsp, &statusDetail);

    if (rsp.DimseStatus)
    {
        OFString err;
        if (!statusDetail || statusDetail->findAndGetOFString(DCM_ErrorComment, err).bad() || err.length() == 0)
        {
            err.assign(QString::number(rsp.DimseStatus).toUtf8());
        }

        cond = makeOFCondition(0, rsp.DimseStatus, OF_error, err.c_str());
    }

    delete statusDetail;
    return cond;
}

OFCondition StoreSCP::sendToServer(DcmDataset* rqDataset, const char *sopInstance)
{
    DcmXfer filexfer(rqDataset->getOriginalXfer());
    auto xfer = filexfer.getXferID();
    OFString sopClass;
    rqDataset->findAndGetOFString(DCM_SOPClassUID, sopClass);

    QUtf8Settings settings;
    settings.beginGroup(server);
    auto timeout = settings.value("timeout").toInt();
    T_ASC_Parameters* params = initAssocParams(settings.value("aetitle").toString(),
                                               settings.value("address").toString(),
                                               timeout, sopClass.c_str(), xfer);

    auto cond = ASC_requestAssociation(net, params, &assoc);
    if (cond.bad())
    {
        qDebug() << "Failed to create association to" << server;
    }
    else
    {
        // Dump general information concerning the establishment of the network connection if required
        //
        qDebug() << "DcmAssoc to" << server << "accepted (max send PDV: " << assoc->sendPDVLength << ")";

        // Figure out which of the accepted presentation contexts should be used
        //
        presId = ASC_findAcceptedPresentationContextID(assoc, sopClass.c_str(), xfer);
        if (presId != 0)
        {
            cond = cStoreRQ(rqDataset, sopClass.c_str(), sopInstance);
        }
        else
        {
            cond = makeOFCondition(0, 1, OF_error, "Presentation context id not found");
        }

        ASC_releaseAssociation(assoc);
        ASC_destroyAssociation(&assoc);
        assoc = nullptr;
    }
    settings.endGroup();

    if (cond.bad())
    {
        qDebug() << "Failed to store " << sopInstance << QString::fromLocal8Bit(cond.text());
    }

    return cond;
}

