/*
 *  The Mana World Server
 *  Copyright 2004 The Mana World Development Team
 *
 *  This file is part of The Mana World.
 *
 *  The Mana World is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  The Mana World is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with The Mana World; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _TMWSERV_NETCOMPUTER_H_
#define _TMWSERV_NETCOMPUTER_H_

#include <iostream>
#include <enet/enet.h>

class MessageOut;

/**
 * This class represents a known computer on the network. For example a
 * connected client or a server we're connected to.
 */
class NetComputer
{
    public:
        /**
         * Constructor.
         */
        NetComputer(ENetPeer *peer);

        /**
         * Destructor.
         */
        virtual ~NetComputer() {}

        /**
         * Returns <code>true</code> if this computer is connected.
         */
        bool isConnected();

        /**
         * Disconnects the computer from the server, after sending a message.
         *
         * The caller of this method should prepare the message, because
         * NetComputer does not know which handler is sending it
         * (could have been chat/game/account)
         */
        void disconnect(const MessageOut &msg);

        /**
         * Queues a message for sending to a client.
         *
         * Reliable packets always arrive, if the client stays connected.
         * Unreliable packets may not arrive, and may not even be sent.
         *
         * Channels are used to ensure that unrelated reliable packets do not
         * hold each other up. In essence, each channel represents a different
         * queue.
         *
         * @param msg      The message to be sent.
         * @param reliable Defines if a reliable or an unreliable packet
         *                 should be sent.
         * @param channel  The channel number of which the packet should
         *                 be sent.
         */
        void send(const MessageOut &msg, bool reliable = true,
                  unsigned int channel = 0);

        /**
         * Returns IP address of computer in 32bit int form
         */
        int getIP() const;

    private:
        ENetPeer *mPeer;              /**< Client peer */

        /**
         * Converts the ip-address of the peer to a stringstream.
         * Example:
         * <code> std::cout << comp </code>
         */
        friend std::ostream& operator <<(std::ostream &os,
                                         const NetComputer &comp);
};

#endif // _TMWSERV_NETCOMPUTER_H_
