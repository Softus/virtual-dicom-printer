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
#include "storescp.h"

#include "transcyrillic.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QJsonDocument>
#include <QJsonObject>
#endif
#include <QRect>
#include <QRegExp>
#include <QSettings>
#include <QStringList>
#include <QXmlStreamReader>

#include <locale.h> // Required for tesseract

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dcvrui.h>
#include <dcmtk/dcmpstat/dvpsdef.h>     /* for constants */
#include <dcmtk/dcmimgle/dcmimage.h>    /* for DicomImage */

static QString fixFileName(QString str)
{
    if (!str.isNull())
    {
        for (int i = 0; i < str.length(); ++i)
        {
            switch (str[i].unicode())
            {
            case '<':
            case '>':
            case ':':
            case '\"':
            case '/':
            case '\\':
            case '|':
            case '?':
            case '*':
                str[i] = '_';
                break;
            }
        }
    }
    return str;
}

static bool saveToDisk(const QString& spoolPath, DcmDataset* rqDataset)
{
    if (!QDir::root().mkpath(spoolPath))
    {
        qDebug() << "Failed to create folder " << spoolPath << ": " << strerror(errno);
    }

    const char* patientId = nullptr;
    rqDataset->findAndGetString(DCM_PatientID, patientId);
    const char* patientName = nullptr;
    rqDataset->findAndGetString(DCM_PatientName, patientName);

    QString fileName = QString(spoolPath)
            .append(QDir::separator()).append(fixFileName(QString::fromUtf8(patientName)))
            .append('_').append(fixFileName(QString::fromUtf8(patientId))).append(".dcm");

    if (QFile::exists(fileName))
    {
        int cnt = 1;
        QString alt;
        do
        {
            alt = QString(fileName).append(" (").append(QString::number(++cnt)).append(')');
        }
        while (QFile::exists(alt));
        fileName = alt;
    }

    DcmFileFormat ff(rqDataset);
    OFCondition cond = ff.saveFile(fileName.toUtf8(),
        EXS_LittleEndianExplicit,  EET_ExplicitLength, EGL_recalcGL, EPD_withoutPadding);

    if (cond.bad())
    {
        qDebug() << "Failed to save " << fileName << ": " << QString::fromLocal8Bit(cond.text());
    }

    return cond.good();
}

static OFCondition putAndInsertVariant(DcmDataset* rspDataset, const DcmTag& tag, const QVariant& value)
{
    if (tag.getVR().isaString())
    {
        return rspDataset->putAndInsertString(tag, value.toString().toUtf8());
    }

    switch (tag.getEVR())
    {
    case EVR_FL:
    case EVR_OF:
        return rspDataset->putAndInsertFloat32(tag, value.toFloat());
    case EVR_FD:
        return rspDataset->putAndInsertFloat64(tag, value.toDouble());
    case EVR_SL:
        return rspDataset->putAndInsertSint32(tag, value.toInt());
    case EVR_UL:
        return rspDataset->putAndInsertUint32(tag, value.toUInt());
    case EVR_SS:
        return rspDataset->putAndInsertSint16(tag, (Sint16)value.toInt());
    case EVR_US:
        return rspDataset->putAndInsertUint16(tag, (Uint16)value.toUInt());
    default:
        break;
    }

    qDebug() << "VR" << tag.getVRName() << "not implemented";
    return EC_IllegalParameter;
}

static OFCondition findAndGetVariant(DcmDataset* rspDataset, const DcmTag& tag, QVariant& value)
{
    OFCondition cond;
    if (tag.getVR().isaString())
    {
        const char* str = nullptr;
        cond = rspDataset->findAndGetString(tag, str);
        if (cond.good())
        {
            value.setValue(QString::fromUtf8(str));
        }
    }
    else
    {
        switch (tag.getEVR())
        {
        case EVR_FL:
        case EVR_OF:
            {
                float f = 0.0f;
                cond = rspDataset->findAndGetFloat32(tag, f);
                if (cond.good()) { value.setValue(f); }
                break;
            }
        case EVR_FD:
            {
                double d = 0.0;
                cond = rspDataset->findAndGetFloat64(tag, d);
                if (cond.good()) { value.setValue(d); }
                break;
            }
        case EVR_SL:
            {
                Sint32 i = 0;
                cond = rspDataset->findAndGetSint32(tag, i);
                if (cond.good()) { value.setValue(i); }
                break;
            }
        case EVR_UL:
            {
                Uint32 u = 0;
                cond = rspDataset->findAndGetUint32(tag, u);
                if (cond.good()) { value.setValue(u); }
                break;
            }
        case EVR_SS:
            {
                Sint16 i = 0;
                cond = rspDataset->findAndGetSint16(tag, i);
                if (cond.good()) { value.setValue(i); }
                break;
            }
        case EVR_US:
            {
                Uint16 u = 0;
                cond = rspDataset->findAndGetUint16(tag, u);
                if (cond.good()) { value.setValue(u); }
                break;
            }
        default:
            {
                cond = EC_IllegalParameter;
                break;
            }
        }
    }

    qDebug() << "VR" << tag.getVRName() << "not implemented";
    return cond;
}

static bool isDatasetPresent(T_DIMSE_Message &msg)
{
    switch (msg.CommandField)
    {
    case DIMSE_C_STORE_RQ:  return msg.msg.CStoreRQ.DataSetType  != DIMSE_DATASET_NULL;
    case DIMSE_C_STORE_RSP: return msg.msg.CStoreRSP.DataSetType != DIMSE_DATASET_NULL;
    case DIMSE_C_GET_RQ:    return msg.msg.CGetRQ.DataSetType    != DIMSE_DATASET_NULL;
    case DIMSE_C_GET_RSP:   return msg.msg.CGetRSP.DataSetType   != DIMSE_DATASET_NULL;
    case DIMSE_C_FIND_RQ:   return msg.msg.CFindRQ.DataSetType   != DIMSE_DATASET_NULL;
    case DIMSE_C_FIND_RSP:  return msg.msg.CFindRSP.DataSetType  != DIMSE_DATASET_NULL;
    case DIMSE_C_MOVE_RQ:   return msg.msg.CMoveRQ.DataSetType   != DIMSE_DATASET_NULL;
    case DIMSE_C_MOVE_RSP:  return msg.msg.CMoveRSP.DataSetType  != DIMSE_DATASET_NULL;
    case DIMSE_C_ECHO_RQ:   return msg.msg.CEchoRQ.DataSetType   != DIMSE_DATASET_NULL;
    case DIMSE_C_ECHO_RSP:  return msg.msg.CEchoRSP.DataSetType  != DIMSE_DATASET_NULL;
    case DIMSE_C_CANCEL_RQ: return msg.msg.CCancelRQ.DataSetType != DIMSE_DATASET_NULL;
    /* there is no DIMSE_C_CANCEL_RSP */

    case DIMSE_N_EVENT_REPORT_RQ:  return msg.msg.NEventReportRQ.DataSetType  != DIMSE_DATASET_NULL;
    case DIMSE_N_EVENT_REPORT_RSP: return msg.msg.NEventReportRSP.DataSetType != DIMSE_DATASET_NULL;
    case DIMSE_N_GET_RQ:           return msg.msg.NGetRQ.DataSetType          != DIMSE_DATASET_NULL;
    case DIMSE_N_GET_RSP:          return msg.msg.NGetRSP.DataSetType         != DIMSE_DATASET_NULL;
    case DIMSE_N_SET_RQ:           return msg.msg.NSetRQ.DataSetType          != DIMSE_DATASET_NULL;
    case DIMSE_N_SET_RSP:          return msg.msg.NSetRSP.DataSetType         != DIMSE_DATASET_NULL;
    case DIMSE_N_ACTION_RQ:        return msg.msg.NActionRQ.DataSetType       != DIMSE_DATASET_NULL;
    case DIMSE_N_ACTION_RSP:       return msg.msg.NActionRSP.DataSetType      != DIMSE_DATASET_NULL;
    case DIMSE_N_CREATE_RQ:        return msg.msg.NCreateRQ.DataSetType       != DIMSE_DATASET_NULL;
    case DIMSE_N_CREATE_RSP:       return msg.msg.NCreateRSP.DataSetType      != DIMSE_DATASET_NULL;
    case DIMSE_N_DELETE_RQ:        return msg.msg.NDeleteRQ.DataSetType       != DIMSE_DATASET_NULL;
    case DIMSE_N_DELETE_RSP:       return msg.msg.NDeleteRSP.DataSetType      != DIMSE_DATASET_NULL;

    default:
        qDebug() << "Unhandled command field" << msg.CommandField;
        break;
    }

    return false;
}

static void dump(const char* desc, DcmItem *dataset)
{
    if (!dataset)
        return;

    std::stringstream ss;
    dataset->print(ss);
    qDebug() << desc << QString::fromLocal8Bit(ss.str().c_str());
}

static void dumpIn(T_DIMSE_Message &msg, DcmItem *dataset)
{
    OFString str;
    DIMSE_dumpMessage(str, msg, DIMSE_INCOMING, dataset);
    qDebug() << QString::fromLocal8Bit(str.c_str());
}

static void dumpOut(T_DIMSE_Message &msg, DcmItem *dataset)
{
    OFString str;
    DIMSE_dumpMessage(str, msg, DIMSE_OUTGOING, dataset);
    qDebug() << QString::fromLocal8Bit(str.c_str());
}

static void copyItems(DcmItem* src, DcmItem *dst)
{
    // The source dataset is optional
    //
    if (!src)
    {
        return;
    }

    DcmObject* obj = nullptr;
    while (obj = src->nextInContainer(obj), obj != nullptr)
    {
        if (obj->getVR() == EVR_SQ)
        {
            // Ignore ReferencedFilmSessionSequence
            //
            continue;
        }

        // Insert with overwrite
        //
        dst->insert(dynamic_cast<DcmElement*>(obj->clone()), true);
    }
}

PrintSCP::PrintSCP(QObject *parent, const QString &printer)
    : QObject(parent)
    , blockMode(DIMSE_BLOCKING)
    , timeout(DEFAULT_TIMEOUT)
    , forceUniqueSeries(false)
    , sessionDataset(nullptr)
    , printer(printer)
    , upstreamNet(nullptr)
    , assoc(nullptr)
    , upstream(nullptr)
{
    QSettings settings;
    auto ocrLang = settings.value("ocr-lang", DEFAULT_OCR_LANG).toString();

    // Set locale to "C" to avoid tesseract crash. Then revert to the system default
    //
    auto oldLocale = setlocale(LC_NUMERIC, "C");
    tess.Init(nullptr, ocrLang.toUtf8(), tesseract::OEM_TESSERACT_ONLY);
    setlocale(LC_NUMERIC, oldLocale);

    blockMode = (T_DIMSE_BlockingMode)settings.value("block-mode", blockMode).toInt();
    timeout   = settings.value("timeout", timeout).toInt();
}

PrintSCP::~PrintSCP()
{
    dropAssociations();
    ASC_dropNetwork(&upstreamNet);
}

DVPSAssociationNegotiationResult PrintSCP::negotiateAssociation(T_ASC_Network *net)
{
    QSettings settings;
    DVPSAssociationNegotiationResult result = DVPSJ_success;
    char buf[BUFSIZ];
    OFBool dropAssoc = OFFalse;

    void *associatePDU=nullptr;
    unsigned long associatePDUlength=0;

    const char *abstractSyntaxes[] =
    {
        UID_BasicGrayscalePrintManagementMetaSOPClass,
        UID_PresentationLUTSOPClass,
        UID_VerificationSOPClass,
    };

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

    OFCondition cond = ASC_receiveAssociation(net, &assoc, DEFAULT_MAXPDU, &associatePDU, &associatePDUlength);
    if (cond.bad())
    {
        qDebug() << "Failed to receive association";
        dropAssoc = OFTrue;
        result = DVPSJ_error;
    }
    else
    {
        printer = QString::fromUtf8(assoc->params->DULparams.calledAPTitle);

        qDebug() << "Client association received (max send PDV: "
             << assoc->sendPDVLength << ")"
             << assoc->params->DULparams.callingPresentationAddress << ":"
             << assoc->params->DULparams.callingAPTitle << "=>"
             << assoc->params->DULparams.calledPresentationAddress << ":"
             << assoc->params->DULparams.calledAPTitle;

        ASC_setAPTitles(assoc->params, nullptr, nullptr, printer.toUtf8());

        /* Application Context Name */
        cond = ASC_getApplicationContextName(assoc->params, buf);
        if (cond.bad() || strcmp(buf, DICOM_STDAPPLICATIONCONTEXT) != 0)
        {
            /* reject: the application context name is not supported */
            qDebug() << "Bad AppContextName: " << buf;
            cond = refuseAssociation(ASC_RESULT_REJECTEDTRANSIENT, ASC_REASON_SU_APPCONTEXTNAMENOTSUPPORTED);
            dropAssoc = OFTrue;
            result = DVPSJ_error;
        }
        else if (!settings.childGroups().contains(printer))
        {
          cond = refuseAssociation(ASC_RESULT_REJECTEDTRANSIENT, ASC_REASON_SU_CALLEDAETITLENOTRECOGNIZED);
          dropAssoc = OFTrue;
          result = DVPSJ_error;
        }
        else
        {
            /* accept presentation contexts */
            cond = ASC_acceptContextsWithPreferredTransferSyntaxes(assoc->params,
                abstractSyntaxes, sizeof(abstractSyntaxes)/sizeof(abstractSyntaxes[0]),
                transferSyntaxes, sizeof(transferSyntaxes)/sizeof(transferSyntaxes[0]));
        }
    } /* receiveAssociation successful */

    if (dropAssoc)
    {
        printer.clear();
        dropAssociations();
    }
    else
    {
        // Initialize connection to upstream printer, if one is configured
        //
        settings.beginGroup(printer);
        auto printerAETitle  = settings.value("upstream-aetitle").toString();
        auto printerAddress  = settings.value("upstream-address").toString();
        auto calleeAETitle   = settings.value("aetitle", assoc->params->DULparams.callingAPTitle).toString().toUpper();
        forceUniqueSeries    = settings.value("force-unique-series").toBool();
        settings.endGroup();

        if (printerAETitle.isEmpty())
        {
            qDebug() << "No upstream connection for" << printer;
        }
        else
        {
            DIC_NODENAME localHost;
            T_ASC_Parameters* params = nullptr;

            auto port  = settings.value("print-port", 0).toInt();
            auto cond = ASC_initializeNetwork(NET_REQUESTOR, port, timeout, &upstreamNet);

            qDebug() << "Creating upstream connection to" << printer;

            cond = ASC_createAssociationParameters(&params, settings.value("pdu-size", ASC_DEFAULTMAXPDU).toInt());
            if (cond.good())
            {
                ASC_setAPTitles(params, calleeAETitle.toUtf8(), printerAETitle.toUtf8(), nullptr);

                // Figure out the presentation addresses and copy the
                // corresponding values into the DcmAssoc parameters.
                //
                gethostname(localHost, sizeof(localHost) - 1);
                ASC_setPresentationAddresses(params, localHost, printerAddress.toUtf8());

                for (size_t i = 0; cond.good() && i < sizeof(abstractSyntaxes)/sizeof(abstractSyntaxes[0]); ++i)
                {
                    cond = ASC_addPresentationContext(params, i*2+1, abstractSyntaxes[i],
                        transferSyntaxes, sizeof(transferSyntaxes)/sizeof(transferSyntaxes[0]));
                }
            }

            if (cond.good())
            {
                cond = ASC_requestAssociation(upstreamNet, params, &upstream);
            }

            if (cond.bad())
            {
                qDebug() << "Failed to create association to" << printerAETitle << QString::fromLocal8Bit(cond.text());
                ASC_destroyAssociation(&upstream);
            }
            else
            {
                // Dump general information concerning the establishment of the network connection if required
                //
                qDebug() << "Connection to upstream printer" << printer
                         << "accepted (max send PDV: " << upstream->sendPDVLength << ")"
                         << upstream->params->DULparams.callingPresentationAddress << ":"
                         << upstream->params->DULparams.callingAPTitle << "=>"
                         << upstream->params->DULparams.calledPresentationAddress << ":"
                         << upstream->params->DULparams.calledAPTitle;
            }
        }

        // First of all, store the calee AE title.
        // Later we will add all attributes comes from client/server to the
        // final message. And store the message to the storage server.
        //
        sessionDataset = new DcmDataset;
        sessionDataset->putAndInsertString(DCM_RETIRED_DestinationAE, calleeAETitle.toUtf8());

        // Fill in with some defaults
        //
        sessionDataset->putAndInsertString(DCM_PatientID,   "0", false);
        sessionDataset->putAndInsertString(DCM_PatientName, "^", false);
    }

    delete[] (char *)associatePDU;
    return result;
}

OFCondition PrintSCP::refuseAssociation(T_ASC_RejectParametersResult result, T_ASC_RejectParametersReason reason)
{
    qDebug() << __FUNCTION__ << result << reason;
    T_ASC_RejectParameters rej = { result, ASC_SOURCE_SERVICEUSER, reason };

    void *associatePDU = nullptr;
    unsigned long associatePDUlength=0;
    OFCondition cond = ASC_rejectAssociation(assoc, &rej, &associatePDU, &associatePDUlength);
    delete[] (char *)associatePDU;
    return cond;
}

void PrintSCP::dropAssociations()
{
    if (assoc)
    {
        qDebug() << "Client connection closed";
        ASC_dropSCPAssociation(assoc);
        ASC_destroyAssociation(&assoc);
    }

    if (upstream)
    {
        qDebug() << "Upstream connection closed";
        ASC_dropSCPAssociation(upstream);
        ASC_destroyAssociation(&upstream);
        ASC_dropNetwork(&upstreamNet);
    }

    delete sessionDataset;
    sessionDataset = nullptr;
}

void PrintSCP::handleClient()
{
    void *associatePDU = nullptr;
    unsigned long associatePDUlength = 0;

    OFCondition cond = ASC_acknowledgeAssociation(assoc, &associatePDU, &associatePDUlength);
    delete[] (char *)associatePDU;

    // Do  the real work
    //
    while (cond.good())
    {
        T_DIMSE_Message rq;
        T_DIMSE_Message rsp;
        T_ASC_PresentationContextID presId;
        T_ASC_PresentationContextID upstreamPresId = 0;
        DcmDataset *rawCommandSet = nullptr;
        DcmDataset *statusDetail = nullptr;
        DcmDataset *rqDataset = nullptr;
        DcmDataset *rspDataset = nullptr;

        cond = DIMSE_receiveCommand(assoc, DIMSE_BLOCKING, 0, &presId, &rq, &statusDetail, &rawCommandSet);

        if (cond.bad())
        {
            qDebug() << "DIMSE_receiveCommand" << QString::fromLocal8Bit(cond.text());
            break;
        }

        dump("statusDetail", statusDetail);
        dump("rawCommandSet", rawCommandSet);
        delete rawCommandSet;
        rawCommandSet = nullptr;

        if (isDatasetPresent(rq))
        {
            cond = DIMSE_receiveDataSetInMemory(assoc, blockMode, timeout, &presId, &rqDataset, nullptr, nullptr);
            if (cond.bad())
            {
                qDebug() << "DIMSE_receiveDataSetInMemory" << QString::fromLocal8Bit(cond.text());
                break;
            }
        }

        dumpIn(rq, rqDataset);

        if (upstream)
        {
            cond = DIMSE_sendMessageUsingMemoryData(upstream, presId, &rq, statusDetail, rqDataset, nullptr, nullptr, &rawCommandSet);
            dump("rawCommandSet", rawCommandSet);
            delete rawCommandSet;
            rawCommandSet = nullptr;
            delete statusDetail;
            statusDetail = nullptr;

            if (cond.bad())
            {
                qDebug() << "DIMSE_sendMessageUsingMemoryData(upstream) failed" << QString::fromLocal8Bit(cond.text())
                         << "presId" << presId;
                break;
            }

            cond = DIMSE_receiveCommand(upstream, blockMode, timeout, &upstreamPresId, &rsp, &statusDetail, &rawCommandSet);
            dump("rawCommandSet", rawCommandSet);
            delete rawCommandSet;
            rawCommandSet = nullptr;
            dump("statusDetail", statusDetail);

            if (cond.bad())
            {
                qDebug() << "DIMSE_recv(upstream) failed" << QString::fromLocal8Bit(cond.text());
                break;
            }

            if (rq.CommandField != (rsp.CommandField & ~0x8000))
            {
                qDebug() << "Mismatched response: rq" << rq.CommandField << "rsp" << rsp.CommandField;
            }

            if (isDatasetPresent(rsp))
            {
                cond = DIMSE_receiveDataSetInMemory(upstream, blockMode, timeout, &upstreamPresId, &rspDataset, nullptr, nullptr);
                if (cond.bad())
                {
                    qDebug() << "DIMSE_receiveDataSetInMemory(upstream)" << QString::fromLocal8Bit(cond.text());
                    break;
                }
            }
        }
        else
        {
            /* process command */
            switch (rq.CommandField)
            {
            case DIMSE_C_ECHO_RQ:
                cond = handleCEcho(rq, rqDataset, rsp, rspDataset);
                break;
            case DIMSE_N_GET_RQ:
                cond = handleNGet(rq, rqDataset, rsp, rspDataset);
                break;
            case DIMSE_N_SET_RQ:
                cond = handleNSet(rq, rqDataset, rsp, rspDataset);
                break;
            case DIMSE_N_ACTION_RQ:
                cond = handleNAction(rq, rqDataset, rsp, rspDataset);
                break;
            case DIMSE_N_CREATE_RQ:
                cond = handleNCreate(rq, rqDataset, rsp, rspDataset);
                break;
            case DIMSE_N_DELETE_RQ:
                cond = handleNDelete(rq, rqDataset, rsp, rspDataset);
                break;
            default:
                cond = DIMSE_BADCOMMANDTYPE; /* unsupported command */
                qDebug() << "Cannot handle command: 0x" << QString::number((unsigned)rq.CommandField, 16);
                break;
            }
        }

        if (DIMSE_N_SET_RQ == rq.CommandField
           && QString(rq.msg.NSetRQ.RequestedSOPClassUID).startsWith(UID_BasicGrayscaleImageBoxSOPClass))
        {
            SOPInstanceUID = QString::fromUtf8(rq.msg.NSetRQ.RequestedSOPInstanceUID);
            if (forceUniqueSeries)
            {
                char uid[100] = {0};
                dcmGenerateUniqueIdentifier(uid,  SITE_SERIES_UID_ROOT);
                seriesInstanceUID = QString::fromUtf8(uid);
            }
            storeImage(rqDataset);
        }
        else
        {
            if (DIMSE_N_CREATE_RQ == rq.CommandField)
            {
                if (0 == strcmp(rq.msg.NCreateRQ.AffectedSOPClassUID, UID_BasicFilmSessionSOPClass))
                {
                    filmSessionUID = QString::fromUtf8(rsp.msg.NCreateRSP.AffectedSOPInstanceUID);
                }
                else if (0 == strcmp(rq.msg.NCreateRQ.AffectedSOPClassUID, UID_BasicFilmBoxSOPClass))
                {
                    seriesInstanceUID  = QString::fromUtf8(rsp.msg.NCreateRSP.AffectedSOPInstanceUID);
                }
            }

            copyItems(rqDataset, sessionDataset);
            copyItems(rspDataset, sessionDataset);
        }

        delete rqDataset;
        rqDataset = nullptr;

        dumpOut(rsp, rspDataset);
        cond = DIMSE_sendMessageUsingMemoryData(assoc, presId, &rsp, statusDetail, rspDataset, nullptr, nullptr, &rawCommandSet);
        dump("rawCommandSet", rawCommandSet);
        delete rawCommandSet;
        rawCommandSet = nullptr;
        delete statusDetail;
        statusDetail = nullptr;
        delete rspDataset;
        rspDataset = nullptr;

        if (cond.bad())
        {
            qDebug() << "DIMSE_sendMessageUsingMemoryData" << QString::fromLocal8Bit(cond.text());
            break;
        }

    } /* while */

    qDebug() << "Done";

    // close client association
    //
    if (cond == DUL_PEERREQUESTEDRELEASE)
    {
        qDebug() << "Association Release";
        cond = ASC_acknowledgeRelease(assoc);
    }
    else if (cond == DUL_PEERABORTEDASSOCIATION)
    {
        qDebug() << "Association Aborted";
    }
    else
    {
      qDebug() << "DIMSE Failure (aborting association)";
      cond = ASC_abortAssociation(assoc);
    }

    // close upstream printer association
    //
    if (upstream)
    {
        ASC_releaseAssociation(upstream);
    }

    dropAssociations();
}

OFCondition PrintSCP::handleCEcho(T_DIMSE_Message& rq, DcmDataset *, T_DIMSE_Message& rsp, DcmDataset *&)
{
    rsp.CommandField = DIMSE_C_ECHO_RSP;
    rsp.msg.CEchoRSP.MessageIDBeingRespondedTo = rq.msg.CEchoRQ.MessageID;
    rsp.msg.CEchoRSP.AffectedSOPClassUID[0] = 0;
    rsp.msg.CEchoRSP.DataSetType = DIMSE_DATASET_NULL;
    rsp.msg.CEchoRSP.DimseStatus = STATUS_Success;
    rsp.msg.CEchoRSP.opts = 0;

    OFCondition cond = EC_Normal;

    return cond;
}

OFCondition PrintSCP::handleNGet(T_DIMSE_Message& rq, DcmDataset *, T_DIMSE_Message& rsp, DcmDataset *& rspDataset)
{
    // initialize response message
    rsp.CommandField = DIMSE_N_GET_RSP;
    rsp.msg.NGetRSP.MessageIDBeingRespondedTo = rq.msg.NGetRQ.MessageID;
    rsp.msg.NGetRSP.AffectedSOPClassUID[0] = 0;
    rsp.msg.NGetRSP.DimseStatus = STATUS_Success;
    rsp.msg.NGetRSP.AffectedSOPInstanceUID[0] = 0;
    rsp.msg.NGetRSP.DataSetType = DIMSE_DATASET_NULL;
    rsp.msg.NGetRSP.opts = 0;

    OFCondition cond = EC_Normal;

    QString sopClass(rq.msg.NGetRQ.RequestedSOPClassUID);
    if (sopClass == UID_PrinterSOPClass)
    {
        // Print N-GET
        printerNGet(rq, rsp, rspDataset);
    }
    else
    {
        qDebug() << "N-GET unsupported for SOP class '" << sopClass << "'";
        rsp.msg.NGetRSP.DimseStatus = STATUS_N_NoSuchSOPClass;
    }

    return cond;
}

OFCondition PrintSCP::handleNSet(T_DIMSE_Message& rq, DcmDataset *, T_DIMSE_Message& rsp, DcmDataset *&)
{
    // initialize response message
    rsp.CommandField = DIMSE_N_SET_RSP;
    rsp.msg.NSetRSP.MessageIDBeingRespondedTo = rq.msg.NSetRQ.MessageID;
    rsp.msg.NSetRSP.AffectedSOPClassUID[0] = 0;
    rsp.msg.NSetRSP.DimseStatus = STATUS_Success;
    rsp.msg.NSetRSP.AffectedSOPInstanceUID[0] = 0;
    rsp.msg.NSetRSP.DataSetType = DIMSE_DATASET_NULL;
    rsp.msg.NSetRSP.opts = 0;

    OFCondition cond = EC_Normal;

    return cond;
}


OFCondition PrintSCP::handleNAction(T_DIMSE_Message& rq, DcmDataset *, T_DIMSE_Message& rsp, DcmDataset *&)
{
    // initialize response message
    rsp.CommandField = DIMSE_N_ACTION_RSP;
    rsp.msg.NActionRSP.MessageIDBeingRespondedTo = rq.msg.NActionRQ.MessageID;
    rsp.msg.NActionRSP.AffectedSOPClassUID[0] = 0;
    rsp.msg.NActionRSP.DimseStatus = STATUS_Success;
    rsp.msg.NActionRSP.AffectedSOPInstanceUID[0] = 0;
    rsp.msg.NActionRSP.ActionTypeID = rq.msg.NActionRQ.ActionTypeID;
    rsp.msg.NActionRSP.DataSetType = DIMSE_DATASET_NULL;
    rsp.msg.NActionRSP.opts = O_NACTION_ACTIONTYPEID;

    OFCondition cond = EC_Normal;

    return cond;
}

OFCondition PrintSCP::handleNCreate(T_DIMSE_Message& rq, DcmDataset *rqDataset, T_DIMSE_Message& rsp, DcmDataset *& rspDataset)
{
    // initialize response message
    rsp.CommandField = DIMSE_N_CREATE_RSP;
    rsp.msg.NCreateRSP.MessageIDBeingRespondedTo = rq.msg.NCreateRQ.MessageID;
    rsp.msg.NCreateRSP.AffectedSOPClassUID[0] = 0;
    rsp.msg.NCreateRSP.DimseStatus = STATUS_Success;
    if (rq.msg.NCreateRQ.opts & O_NCREATE_AFFECTEDSOPINSTANCEUID)
    {
        // instance UID is provided by SCU
        strncpy(rsp.msg.NCreateRSP.AffectedSOPInstanceUID, rq.msg.NCreateRQ.AffectedSOPInstanceUID, sizeof(DIC_UI));
    }
    else
    {
        // we generate our own instance UID
        dcmGenerateUniqueIdentifier(rsp.msg.NCreateRSP.AffectedSOPInstanceUID);
    }
    rsp.msg.NCreateRSP.DataSetType = DIMSE_DATASET_NULL;
    rsp.msg.NCreateRSP.opts = O_NCREATE_AFFECTEDSOPINSTANCEUID | O_NCREATE_AFFECTEDSOPCLASSUID;
    strncpy(rsp.msg.NCreateRSP.AffectedSOPClassUID, rq.msg.NCreateRQ.AffectedSOPClassUID, sizeof(DIC_UI));

    OFCondition cond = EC_Normal;

    QString sopClass(rq.msg.NCreateRQ.AffectedSOPClassUID);
    if (sopClass == UID_BasicFilmSessionSOPClass)
    {
        // BFS N-CREATE
        filmSessionNCreate(rqDataset, rsp, rspDataset);
    }
    else if (sopClass == UID_BasicFilmBoxSOPClass)
    {
        // BFB N-CREATE
        filmBoxNCreate(rqDataset, rsp, rspDataset);
    }
    else if (sopClass == UID_PresentationLUTSOPClass)
    {
        // P-LUT N-CREATE
        presentationLUTNCreate(rqDataset, rsp, rspDataset);
    }
    else
    {
        qDebug() << "N-CREATE unsupported for SOP class '" << sopClass << "'";
        rsp.msg.NCreateRSP.DimseStatus = STATUS_N_NoSuchSOPClass;
        rsp.msg.NCreateRSP.opts = 0;  // don't include affected SOP instance UID
    }

    return cond;
}

OFCondition PrintSCP::handleNDelete(T_DIMSE_Message& rq, DcmDataset *, T_DIMSE_Message& rsp, DcmDataset *&)
{
    // initialize response message
    rsp.CommandField = DIMSE_N_DELETE_RSP;
    rsp.msg.NDeleteRSP.MessageIDBeingRespondedTo = rq.msg.NDeleteRQ.MessageID;
    rsp.msg.NDeleteRSP.AffectedSOPClassUID[0] = 0;
    rsp.msg.NDeleteRSP.DimseStatus = STATUS_Success;
    rsp.msg.NDeleteRSP.AffectedSOPInstanceUID[0] = 0;
    rsp.msg.NDeleteRSP.DataSetType = DIMSE_DATASET_NULL;
    rsp.msg.NDeleteRSP.opts = 0;

    OFCondition cond = EC_Normal;

    QString sopClass(rq.msg.NDeleteRQ.RequestedSOPClassUID);
    if (sopClass == UID_BasicFilmSessionSOPClass)
    {
        // BFS N-DELETE
        filmSessionNDelete(rq, rsp);
    }
    else if (sopClass == UID_BasicFilmBoxSOPClass)
    {
        // BFB N-DELETE
        filmBoxNDelete(rq, rsp);
    }
    else if (sopClass == UID_PresentationLUTSOPClass)
    {
        // P-LUT N-DELETE
        presentationLUTNDelete(rq, rsp);
    }
    else
    {
        qDebug() << "N-DELETE unsupported for SOP class '" << sopClass << "'";
        rsp.msg.NDeleteRSP.DimseStatus = STATUS_N_NoSuchSOPClass;
    }

    return cond;
}

void PrintSCP::printerNGet(T_DIMSE_Message& rq, T_DIMSE_Message& rsp, DcmDataset *& rspDataset)
{
    QString printerInstance(UID_PrinterSOPInstance);
    if (printerInstance == rq.msg.NGetRQ.RequestedSOPInstanceUID)
    {
        rsp.msg.NSetRSP.DataSetType = DIMSE_DATASET_PRESENT;
        rspDataset = new DcmDataset;

        // By default, send only PrinterStatus & PrinterStatusInfo
        //
        if (rq.msg.NGetRQ.ListCount == 0)
        {
            rspDataset->putAndInsertString(DCM_PrinterStatus, DEFAULT_printerStatus);
            rspDataset->putAndInsertString(DCM_PrinterStatusInfo, DEFAULT_printerStatusInfo);
        }
        else
        {
            QSettings settings;
            settings.beginGroup(printer);
            QMap<DcmTag, QVariant> info;
            auto size = settings.beginReadArray("info");
            for (int idx = 0; idx < size; ++idx)
            {
                settings.setArrayIndex(idx);
                auto key = settings.value("key").toString();
                DcmTag tag;
                if (DcmTag::findTagFromName(key.toUtf8(), tag).good())
                {
                    info[tag] = settings.value("value");
                }
                else
                {
                    qDebug() << "Bad DICOM tag" << key << "in" << printer << "info" << idx;
                }
            }
            settings.endArray();
            settings.endGroup();

            for (int i = 0; i < rq.msg.NGetRQ.ListCount / 2; ++i)
            {
                auto group   = rq.msg.NGetRQ.AttributeIdentifierList[i*2];
                auto element = rq.msg.NGetRQ.AttributeIdentifierList[i*2 + 1];
                if (element == 0x0000)
                {
                    // Group length
                    //
                    continue;
                }

                if (group == DCM_PrinterStatus.getGroup())
                {
                    if (element == DCM_PrinterStatus.getElement())
                    {
                        rspDataset->putAndInsertString(DCM_PrinterStatus, DEFAULT_printerStatus);
                        continue;
                    }
                    if (element == DCM_PrinterStatusInfo.getElement())
                    {
                        rspDataset->putAndInsertString(DCM_PrinterStatusInfo, DEFAULT_printerStatusInfo);
                        continue;
                    }
                }

                // Some unknown element was requested.
                //
                DcmTag tag(group, element);
                if (!info.contains(tag))
                {
                    qDebug() << "cannot retrieve printer information: unknown attribute ("
                        << QString::number(group, 16) << "," << QString::number(element, 16)
                        << ") in attribute list.";
                    rsp.msg.NGetRSP.DimseStatus = STATUS_N_NoSuchAttribute;
                    delete rspDataset;
                    rspDataset = nullptr;
                    break;
                }

                putAndInsertVariant(rspDataset, tag, info[tag]);
            }
        }
    }
    else
    {
        qDebug() << "cannot retrieve printer information, unknown printer SOP instance UID" << printerInstance;
        rsp.msg.NGetRSP.DimseStatus = STATUS_N_NoSuchObjectInstance;
    }
}

void PrintSCP::filmSessionNCreate(DcmDataset *, T_DIMSE_Message& rsp, DcmDataset *&)
{
    if (!filmSessionUID.isEmpty())
    {
        // film session exists already, refuse n-create
        qDebug() << "cannot create two film sessions concurrently.";
        rsp.msg.NCreateRSP.DimseStatus = STATUS_N_DuplicateSOPInstance;
        rsp.msg.NCreateRSP.opts = 0;  // don't include affected SOP instance UID
    }
}

void PrintSCP::filmBoxNCreate(DcmDataset *rqDataset, T_DIMSE_Message& rsp, DcmDataset *& rspDataset)
{
    rsp.msg.NCreateRSP.DataSetType = DIMSE_DATASET_PRESENT;
    rspDataset = rqDataset? new DcmDataset(*rqDataset): new DcmDataset;
    auto dseq = new DcmSequenceOfItems(DCM_ReferencedImageBoxSequence);
    char uid[100];
    OFString fmt;
    unsigned long count = 1;

    if (rspDataset->findAndGetOFStringArray(DCM_ImageDisplayFormat, fmt).good() && fmt.substr(0,9) == "STANDARD\\")
    {
        unsigned long rows = 0;
        unsigned long cols = 0;
        if (2 == sscanf(fmt.c_str() + 9, "%lu,%lu", &cols, &rows))
        {
            count = rows * cols;
        }
    }

    while (count-- > 0)
    {
        auto ditem = new DcmItem();
        ditem->putAndInsertString(DCM_ReferencedSOPClassUID, UID_BasicGrayscaleImageBoxSOPClass);
        ditem->putAndInsertString(DCM_ReferencedSOPInstanceUID, dcmGenerateUniqueIdentifier(uid, SITE_INSTANCE_UID_ROOT));
        dseq->insert(ditem);
    }

    rspDataset->insert(dseq);
}

void PrintSCP::presentationLUTNCreate(DcmDataset *rqDataset, T_DIMSE_Message& rsp, DcmDataset *& rspDataset)
{
    if (rqDataset)
    {
        rsp.msg.NCreateRSP.DataSetType = DIMSE_DATASET_PRESENT;
        rspDataset = (DcmDataset*)rqDataset->clone();
    }
}

void PrintSCP::filmSessionNDelete(T_DIMSE_Message& rq, T_DIMSE_Message& rsp)
{
    if (filmSessionUID == rq.msg.NDeleteRQ.RequestedSOPInstanceUID)
    {
        filmSessionUID.clear();
        SOPInstanceUID.clear();
        seriesInstanceUID.clear();
    }
    else
    {
        // film session does not exist or wrong instance UID
        //
        qDebug() << "cannot delete film session with instance UID '" << rq.msg.NDeleteRQ.RequestedSOPInstanceUID << "': object does not exist.";
        rsp.msg.NDeleteRSP.DimseStatus = STATUS_N_NoSuchObjectInstance;
    }
}

void PrintSCP::filmBoxNDelete(T_DIMSE_Message&, T_DIMSE_Message&)
{
}

void PrintSCP::presentationLUTNDelete(T_DIMSE_Message&, T_DIMSE_Message&)
{
}

void PrintSCP::storeImage(DcmDataset *rqDataset)
{
    if (!rqDataset)
    {
        qDebug() << __FUNCTION__ << "Request dataset is missing";
        return;
    }

    DcmItem *item = nullptr;
    auto cond = rqDataset->findAndGetSequenceItem(DCM_BasicGrayscaleImageSequence, item);
    if (cond.good())
    {
        // Pull up children items from sequence to the dataset
        //
        DcmElement* obj = nullptr;
        while (obj = item->remove(0UL), obj != nullptr)
        {
            rqDataset->insert(obj);
        }
        rqDataset->remove(DCM_BasicGrayscaleImageSequence);
        delete item;
    }

    copyItems(sessionDataset, rqDataset);

    rqDataset->putAndInsertString(DCM_SpecificCharacterSet, "ISO_IR 192"); // UTF-8

    rqDataset->putAndInsertString(DCM_StudyInstanceUID,  filmSessionUID.toUtf8(), false);
    rqDataset->putAndInsertString(DCM_SeriesInstanceUID, seriesInstanceUID.toUtf8(), false);
    rqDataset->putAndInsertString(DCM_SOPInstanceUID,    SOPInstanceUID.toUtf8(), false);

    auto now = QDateTime::currentDateTime();
    rqDataset->putAndInsertString(DCM_InstanceCreationDate, now.toString("yyyyMMdd").toUtf8(), false);
    rqDataset->putAndInsertString(DCM_InstanceCreationTime, now.toString("HHmmss").toUtf8(), false);
    rqDataset->putAndInsertString(DCM_StudyDate, now.toString("yyyyMMdd").toUtf8(), false);
    rqDataset->putAndInsertString(DCM_StudyTime, now.toString("HHmmss").toUtf8(), false);

    rqDataset->putAndInsertString(DCM_Manufacturer, ORGANIZATION_FULL_NAME, false);
    rqDataset->putAndInsertString(DCM_ManufacturerModelName, PRODUCT_FULL_NAME, false);

    QSettings settings;
    auto spoolPath = settings.value("spool-path").toString();

    if (!webQuery(rqDataset))
    {
        if (!spoolPath.isEmpty())
        {
            rqDataset->putAndInsertString(DCM_RETIRED_PrintQueueID, printer.toUtf8());
            saveToDisk(spoolPath, rqDataset);
        }
    }
    else
    {
        if (settings.value("debug").toInt() > 1)
        {
            saveToDisk(".", rqDataset);
        }

        foreach (auto server, settings.value("storage-servers").toStringList())
        {
            StoreSCP sscp(server);
            cond = sscp.sendToServer(rqDataset, SOPInstanceUID.toUtf8());
            if (cond.bad())
            {
                qDebug() << "Failed to store to" << server << QString::fromLocal8Bit(cond.text());
                if (!spoolPath.isEmpty())
                {
                    saveToDisk(spoolPath.append(QDir::separator()).append(server), rqDataset);
                }
            }
        }
    }
}

static QByteArray writeXmlRequest(const QString& root, const QVariantMap& map)
{
    QByteArray data;
    QXmlStreamWriter xml(&data);

    xml.writeStartDocument();
    xml.writeStartElement(root);
    for (auto i = map.constBegin(); i != map.constEnd(); ++i)
    {
        xml.writeTextElement(i.key(), i.value().toString());
    }
    xml.writeEndElement();
    xml.writeEndDocument();

    return data;
}

static QVariantMap readXmlResponse(const QByteArray& data)
{
    QVariantMap map;
    QXmlStreamReader xml(data);

    while (xml.readNextStartElement())
    {
        if (xml.name() == "element")
        {
            auto key = xml.attributes().value("tag").toString();
            map[key] = xml.readElementText();
        }
        else if (xml.name() != "data-set")
        {
            qDebug() << "Unexpected element" << xml.name();
        }
    }

    return map;
}

bool PrintSCP::webQuery(DcmDataset *rqDataset)
{
    QSettings settings;
    QVariantMap queryParams;
    QVariantMap ret;

    settings.beginGroup("query");
    auto url         = settings.value("url").toUrl();
    auto userName    = settings.value("username").toString();
    auto password    = settings.value("password").toString();
    auto contendType = settings.value("content-type", DEFAULT_CONTENT_TYPE).toString();
    auto extraParams = settings.value("query-parameters").toStringList();
    settings.endGroup();

    settings.beginGroup(printer);
    settings.beginGroup("query");
    url         = settings.value("url",              url).toUrl();
    userName    = settings.value("username",         userName).toString();
    password    = settings.value("password",         password).toString();
    contendType = settings.value("content-type",     contendType).toString();
    extraParams = settings.value("query-parameters", extraParams).toStringList();
    settings.endGroup();
    settings.endGroup();

    if (url.isEmpty())
    {
        return true;
    }

    DicomImage di(rqDataset, rqDataset->getOriginalXfer());
    void *img = nullptr;

    if (di.createJavaAWTBitmap(img, 0, 32) && img)
    {
        tess.SetImage((const unsigned char*)img, di.getWidth(), di.getHeight(), 4, 4 * di.getWidth());

        // Global tags
        //
        insertTags(rqDataset, queryParams, &di, settings);

        // This printer tags
        //
        settings.beginGroup(printer);
        insertTags(rqDataset, queryParams, &di, settings);
        settings.endGroup();
        delete[] (Uint32*)img;
    }

    Q_FOREACH (auto extraParam, extraParams)
    {
        auto parts = extraParam.split("=:");
        QVariant value;

        if (parts.size() > 1)
        {
            DcmTag tag;
            if (DcmTag::findTagFromName(parts[1].toUtf8(), tag).bad())
            {
                qDebug() << "Unknown DCM tag" << parts[1];
            }
            else if (findAndGetVariant(rqDataset, tag, value).bad())
            {
                qDebug() << "Failed te retrieve DCM tag" << tag.getTagName() << "from the dataset";
            }
        }
        queryParams[parts[0]] = value;
    }

    bool error = false;
    QNetworkAccessManager mgr;

    QByteArray data;
    if (contendType.endsWith("xml", Qt::CaseInsensitive))
    {
        // These parameters are hardcoded
        //
        queryParams["study-instance-uid"] = filmSessionUID;
        queryParams["medical-service-date"] = QDate::currentDate().toString("yyyy-MM-dd");

        data = writeXmlRequest("save-hardcopy-grayscale-image-request", queryParams);
    }
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    else if (contendType.endsWith("json", Qt::CaseInsensitive))
    {
        // These parameters are hardcoded
        //
        queryParams["studyInstanceUID"] = filmSessionUID;
        queryParams["medicalServiceDate"] = QDate::currentDate().toString("yyyy-MM-dd");

        data = QJsonDocument(QJsonObject::fromVariantMap(queryParams)).toJson();
    }
#endif
    else
    {
        qDebug() << contendType << "not supported";
    }

    QNetworkRequest rq(url);
    rq.setRawHeader("Accept", "*");
    if (!userName.isEmpty())
    {
        rq.setRawHeader("Authorization", "Basic " + QByteArray(userName.append(':').append(password).toUtf8()).toBase64());
    }

    rq.setHeader(QNetworkRequest::ContentTypeHeader, contendType);
    rq.setHeader(QNetworkRequest::ContentLengthHeader, data.size());
    qDebug() << url << data;
    auto reply = mgr.post(rq, data);
    auto start = QDateTime::currentMSecsSinceEpoch();

    while (reply->isRunning() && (timeout <= 0 || timeout > (QDateTime::currentMSecsSinceEpoch() - start) / 1000))
    {
        qApp->processEvents(QEventLoop::AllEvents, 100);
    }

    if (reply->isRunning())
    {
        qDebug() << "Web query request timeout, aborting";
        reply->abort();
        qApp->processEvents(QEventLoop::AllEvents, 100);
        ++error;
    }

    auto responseContentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    auto response = reply->readAll();

    if (settings.value("debug").toBool())
    {
        qDebug() << reply->error() << reply->errorString()
                 << responseContentType << QString::fromUtf8(response);
    }

    if (reply->error())
    {
        ++error;
    }

    if (responseContentType.endsWith("xml"))
    {
        ret = readXmlResponse(response);
    }
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    else if (responseContentType.endsWith("json"))
    {
        ret = QJsonDocument::fromJson(response).object().toVariantMap();
    }
#endif
    else
    {
        qDebug() << "response content type" << responseContentType << "not supported";
        ++error;
    }

    if (error)
    {
        // Reset the patient name & id in case of error
        //
        rqDataset->putAndInsertString(DCM_PatientID,   "0", true);
        rqDataset->putAndInsertString(DCM_PatientName, "^", true);
    }
    else
    {
        for (auto i = ret.constBegin(); i != ret.constEnd(); ++i)
        {
            DcmTag tag;
            if (DcmTag::findTagFromName(i.key().toUtf8(), tag).bad())
            {
                qDebug() << "Unknown DCM tag" << i.key();
            }
            else
            {
                // We shouldn't call translateToLatin for integers & dates,
                // but it is clear that translateToLatin does not affect
                // numbers nor punctuation characters.
                //
                auto str = translateToLatin(i.value().toString());
                qDebug() << tag.getXTag().toString().c_str() << tag.getTagName() << str;
                putAndInsertVariant(rqDataset, tag, str);
            }
        }
    }

    reply->deleteLater();
    return !error;
}

void PrintSCP::insertTags(DcmDataset *rqDataset, QVariantMap &queryParams, DicomImage *di, QSettings& settings)
{
    auto tagCount = settings.beginReadArray("tag");
    QRect prevRect;
    QString ocrText;

    for (int i = 0; i < tagCount; ++i)
    {
        settings.setArrayIndex(i);
        auto key = settings.value("key").toString();

        auto rect = settings.value("rect").toRect();
        if (!rect.isEmpty())
        {
            if (rect.left() < 0) rect.moveLeft(di->getWidth() + rect.left());
            if (rect.top() < 0) rect.moveTop(di->getHeight() + rect.top());
            if (prevRect != rect)
            {
                tess.SetRectangle(rect.left(), rect.top(), rect.width(), rect.height());
                prevRect = rect;
                ocrText = QString::fromUtf8(tess.GetUTF8Text());
            }
        }

        QString str;
        auto pattern = settings.value("pattern").toString();
        if (!pattern.isEmpty())
        {
            if (ocrText.isEmpty())
            {
                qDebug() << "No text on the image for idx" << i << "key" << key << "rect" << rect;
            }
            else
            {
                QRegExp re(pattern);
                if (re.indexIn(ocrText) < 0)
                {
                    qDebug() << ocrText << "does not match" << pattern;
                }
                else
                {
                    str = re.cap(1);
                }
            }
        }

        // The pattern is absent or mismatched - send the default value
        //
        if (str.isEmpty())
        {
            str = settings.value("value").toString();
        }

        auto param = settings.value("query-parameter").toString();
        if (!param.isEmpty())
        {
            queryParams[param] = str;
        }

        if (!key.isEmpty())
        {
            DcmTag tag;
            if (DcmTag::findTagFromName(key.toUtf8(), tag).bad())
            {
                qDebug() << "Unknown DCM tag" << key;
            }
            else
            {
                qDebug() << tag.getXTag().toString().c_str() << tag.getTagName() << str << param;
                rqDataset->putAndInsertString(tag, str.toUtf8());
            }
        }
    }
    settings.endArray();
}
