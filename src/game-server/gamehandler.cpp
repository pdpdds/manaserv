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
 *
 *  $Id$
 */

#include <cassert>
#include <map>

#include "map.h"
#include "game-server/gamehandler.hpp"
#include "game-server/state.hpp"
#include "net/messagein.hpp"
#include "net/messageout.hpp"
#include "net/netcomputer.hpp"
#include "utils/logger.h"

struct GameClient: NetComputer
{
    GameClient(ENetPeer *peer): NetComputer(peer), character(NULL) {}
    Player *character;
};

struct GamePendingLogin
{
    Player *character;
    int timeout;
};

typedef std::map< std::string, GamePendingLogin > GamePendingLogins;
typedef std::map< std::string, GameClient * > GamePendingClients;

/**
 * The pending logins represent clients who were given a magic token by the
 * account server but who have not yet logged in to the game server.
 */
static GamePendingLogins pendingLogins;

/**
 * The pending clients represent clients who tried to login to the game server,
 * but for which no magic token is available yet. This can happen when the
 * communication between the account server and client went faster than the
 * communication between the account server and the game server.
 */
static GamePendingClients pendingClients;

/**
 * Notification that a particular token has been given to allow a certain
 * player to enter the game.
 */
void registerGameClient(std::string const &token, Player *ch)
{
    GamePendingClients::iterator i = pendingClients.find(token);
    if (i != pendingClients.end())
    {
        GameClient *computer = i->second;
        computer->character = ch;
        ch->setClient(computer);
        gameState->addObject(ch);
        pendingClients.erase(i);
        MessageOut result;
        result.writeShort(GPMSG_CONNECT_RESPONSE);
        result.writeByte(ERRMSG_OK);
        computer->send(result);
    }
    else
    {
        GamePendingLogin p;
        p.character = ch;
        p.timeout = 300; // world ticks
        pendingLogins.insert(std::make_pair(token, p));
    }
}

bool
GameHandler::startListen(enet_uint16 port)
{
    LOG_INFO("Game handler started:", 0);
    return ConnectionHandler::startListen(port);
}

void GameHandler::removeOutdatedPending()
{
    GamePendingLogins::iterator i = pendingLogins.begin();

    while (i != pendingLogins.end())
    {
        if (--i->second.timeout <= 0)
        {
            delete i->second.character;
            pendingLogins.erase(i++);
        }
        else
        {
            ++i;
        }
    }
}

NetComputer *GameHandler::computerConnected(ENetPeer *peer)
{
    return new GameClient(peer);
}

void GameHandler::computerDisconnected(NetComputer *computer)
{
    for (GamePendingClients::iterator i = pendingClients.begin(),
         i_end = pendingClients.end(); i != i_end; ++i)
    {
        if (i->second == computer)
        {
            pendingClients.erase(i);
            break;
        }
    }
    if (Player *ch = static_cast< GameClient * >(computer)->character)
    {
        gameState->removeObject(ch);
        delete ch;
    }
    delete computer;
}

void GameHandler::process()
{
    ConnectionHandler::process();
    removeOutdatedPending();
}

void GameHandler::processMessage(NetComputer *comp, MessageIn &message)
{
    GameClient &computer = *static_cast< GameClient * >(comp);
    MessageOut result;

    if (computer.character == NULL)
    {
        if (message.getId() != PGMSG_CONNECT) return;
        std::string magic_token = message.readString(32);
        GamePendingLogins::iterator i = pendingLogins.find(magic_token);
        if (i == pendingLogins.end())
        {
            for (GamePendingClients::iterator j = pendingClients.begin(),
                 j_end = pendingClients.end(); j != j_end; ++j)
            {
                if (j->second == &computer) return;
            }
            pendingClients.insert(std::make_pair(magic_token, &computer));
            return;
        }
        computer.character = i->second.character;
        computer.character->setClient(&computer);
        gameState->addObject(computer.character);
        pendingLogins.erase(i);
        result.writeShort(GPMSG_CONNECT_RESPONSE);
        result.writeByte(ERRMSG_OK);
        computer.send(result);
        return;
    }

    switch (message.getId())
    {
        case PGMSG_SAY:
            {
                std::string say = message.readString();
                gameState->sayAround(computer.character, say);
            } break;

        /*
        case PGMSG_PICKUP:
            {
                // add item to inventory (this is too simplistic atm)
                unsigned int itemId = message.readLong();

                // remove the item from world map

                // send feedback
                computer.character->addItem(itemId);
                result.writeShort(GPMSG_PICKUP_RESPONSE);
                result.writeByte(ERRMSG_OK);
            } break;

        case PGMSG_USE_ITEM:
            {
                unsigned int itemId = message.readLong();

                result.writeShort(GPMSG_USE_RESPONSE);

                if (computer.character->hasItem(itemId)) {
                    // use item
                    // this should execute a script which will do the appropriate action
                    // (the script will determine if the item is 1 use only)
                    result.writeByte(ERRMSG_OK);
                } else {
                    result.writeByte(ERRMSG_FAILURE);
                }
            } break;
        */

        case PGMSG_WALK:
            {
                unsigned x = message.readShort();
                unsigned y = message.readShort();
                Point dst = {x, y};
                computer.character->setDestination(dst);

                // no response should be required
            } break;

        /*
        case PGMSG_EQUIP:
            {
                message.readLong(); // ItemId: Not useful, the inventory knows it
                char slot = message.readByte();

                result.writeShort(GPMSG_EQUIP_RESPONSE);
                result.writeByte(computer.character->equip(slot) ?
                                 ERRMSG_OK : ERRMSG_FAILURE);
            } break;
        */

        case PGMSG_ATTACK:
            {
                LOG_DEBUG("Player " << computer.character->getPublicID()
                          << " attacks", 0);
                computer.character->setDirection(message.readByte());
                computer.character->setAttacking(true);
            } break;

        default:
            LOG_WARN("Invalid message type", 0);
            result.writeShort(XXMSG_INVALID);
            break;
    }

    if (result.getLength() > 0)
        computer.send(result);
}

void GameHandler::sendTo(Player *beingPtr, MessageOut &msg)
{
    GameClient *client = beingPtr->getClient();
    assert(client != NULL);
    client->send(msg);
}