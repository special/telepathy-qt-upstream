/*
 * This file is part of TelepathyQt4
 *
 * Copyright (call) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "call-handler.h"
#include "_gen/call-handler.moc.hpp"

#include "call-widget.h"

#include <TelepathyQt4/Client/Channel>
#include <TelepathyQt4/Client/Connection>
#include <TelepathyQt4/Client/Contact>
#include <TelepathyQt4/Client/ContactManager>
#include <TelepathyQt4/Client/PendingChannel>
#include <TelepathyQt4/Client/PendingReady>
#include <TelepathyQt4/Client/ReferencedHandles>
#include <TelepathyQt4/Client/StreamedMediaChannel>

#include <QDebug>
#include <QMessageBox>

using namespace Telepathy::Client;

CallHandler::CallHandler(QObject *parent)
    : QObject(parent)
{
}

CallHandler::~CallHandler()
{
    foreach (CallWidget *call, mCalls) {
        call->close();
    }
}

void CallHandler::addOutgoingCall(const ContactPtr &contact)
{
    QVariantMap request;
    request.insert(QLatin1String(TELEPATHY_INTERFACE_CHANNEL ".ChannelType"),
                   TELEPATHY_INTERFACE_CHANNEL_TYPE_STREAMED_MEDIA);
    request.insert(QLatin1String(TELEPATHY_INTERFACE_CHANNEL ".TargetHandleType"),
                   Telepathy::HandleTypeContact);
    request.insert(QLatin1String(TELEPATHY_INTERFACE_CHANNEL ".TargetHandle"),
                   contact->handle()[0]);

    Connection *conn = contact->manager()->connection();
    connect(conn->ensureChannel(request),
            SIGNAL(finished(Telepathy::Client::PendingOperation*)),
            SLOT(onOutgoingChannelCreated(Telepathy::Client::PendingOperation*)));
}

void CallHandler::addIncomingCall(StreamedMediaChannel *chan)
{
    mChannels.append(chan);
    connect(chan->becomeReady(),
            SIGNAL(finished(Telepathy::Client::PendingOperation*)),
            SLOT(onIncomingChannelReady(Telepathy::Client::PendingOperation*)));
}

void CallHandler::onOutgoingChannelCreated(PendingOperation *op)
{
    if (op->isError()) {
        qWarning() << "CallHandler::onOutgoingChannelCreated: channel cannot be created:" <<
            op->errorName() << "-" << op->errorMessage();

        QMessageBox msgBox;
        msgBox.setText(QString("Unable to call (%1)").arg(op->errorMessage()));
        msgBox.exec();
        return;
    }

    PendingChannel *pc = qobject_cast<PendingChannel *>(op);

    StreamedMediaChannel *chan = new StreamedMediaChannel(pc->connection(),
            pc->objectPath(), pc->immutableProperties());
    mChannels.append(chan);
    connect(chan->becomeReady(),
            SIGNAL(finished(Telepathy::Client::PendingOperation*)),
            SLOT(onOutgoingChannelReady(Telepathy::Client::PendingOperation*)));
}

void CallHandler::onOutgoingChannelReady(PendingOperation *op)
{
    PendingReady *pr = qobject_cast<PendingReady *>(op);
    StreamedMediaChannel *chan = qobject_cast<StreamedMediaChannel *>(pr->object());

    if (op->isError()) {
        qWarning() << "CallHandler::onOutgoingChannelReady: channel cannot become ready:" <<
            op->errorName() << "-" << op->errorMessage();

        mChannels.removeOne(chan);

        QMessageBox msgBox;
        msgBox.setText(QString("Unable to call (%1)").arg(op->errorMessage()));
        msgBox.exec();
        return;
    }

    ContactManager *cm = chan->connection()->contactManager();
    ContactPtr contact = cm->lookupContactByHandle(chan->targetHandle());
    Q_ASSERT(contact);

    CallWidget *call = new CallWidget(chan, contact);
    mCalls.append(call);
    connect(call,
            SIGNAL(destroyed(QObject *)),
            SLOT(onCallTerminated(QObject *)));
    call->show();
}

void CallHandler::onIncomingChannelReady(PendingOperation *op)
{
    PendingReady *pr = qobject_cast<PendingReady *>(op);
    StreamedMediaChannel *chan = qobject_cast<StreamedMediaChannel *>(pr->object());

    if (op->isError()) {
        // ignore - channel cannot be ready
        qWarning() << "CallHandler::onIncomingChannelReady: channel cannot become ready:" <<
            op->errorName() << "-" << op->errorMessage();

        mChannels.removeOne(chan);
        return;
    }

    ContactPtr contact = chan->initiatorContact();

    QMessageBox msgBox;
    msgBox.setText("Incoming call");
    msgBox.setInformativeText(QString("%1 is calling you, do you want to answer?").arg(contact->id()));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    int ret = msgBox.exec();

    if (ret == QMessageBox::No) {
        chan->requestClose();
        return;
    }

    chan->acceptCall();

    CallWidget *call = new CallWidget(chan, contact);
    mCalls.append(call);
    connect(call,
            SIGNAL(destroyed(QObject *)),
            SLOT(onCallTerminated(QObject *)));
    call->show();
}

void CallHandler::onCallTerminated(QObject *obj)
{
    CallWidget *call = (CallWidget *) obj;
    StreamedMediaChannel *chan = call->channel();
    mCalls.removeOne(call);
    mChannels.removeOne(chan);
    delete chan;
}