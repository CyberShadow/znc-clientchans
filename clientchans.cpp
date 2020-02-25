/*
 * Copyright (C) 2020 Vladimir Panteleev
 * Copyright (C) 2015 J-P Nurmi
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <znc/Chan.h>
#include <znc/Client.h>
#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>
#include <znc/Modules.h>
#include <znc/Nick.h>
#include <znc/version.h>

#if (VERSION_MAJOR < 1) || (VERSION_MAJOR == 1 && VERSION_MINOR < 6)
#error The clientchans module requires ZNC version 1.6.0 or later.
#endif

class CClientChansMod : public CModule {
  public:
	MODCONSTRUCTOR(CClientChansMod) { Initialize(); }

	virtual EModRet OnUserRaw(CString& line) override;
	virtual EModRet OnSendToClient(CString& line, CClient& client) override;
	virtual void OnClientDisconnect() override;

  private:
	// Holds information regarding which clients think they are currently on a channel.
	// This "in channel" state applies since we receive the JOIN from the client,
	// and until a PART reply is sent to it.
	std::map<CString, std::set<CClient*>> m_channelClients;

	void Initialize();
	EModRet OnClientLeftChannel(const CString& sChannelName);

	bool IsChannelVisible(CClient* pClient, const CString& sChannelName) const;
	void SetChannelVisible(CClient* pClient, const CString& sChannelName, bool visible);
};

#if 0
static void DEBUGLOG(CString s) {
	FILE* f = fopen("/tmp/clientchans.log", "a");
	fprintf(f, "%s\n", s.c_str());
	fclose(f);
}
#else
#define DEBUGLOG(x) ((void)0)
#endif

void CClientChansMod::Initialize() {
	DEBUGLOG("* Initializing");
	// Find which (non-detached) channels are already open
	CIRCNetwork* pNetwork = GetNetwork();
	for (auto pChannel : pNetwork->GetChans()) {
		if (pChannel->IsDetached())
			continue;
		DEBUGLOG("* Adding channel: " + pChannel->GetName());
		auto pChannelClients = &m_channelClients[pChannel->GetName()];
		for (auto pClient : pNetwork->GetClients()) {
			DEBUGLOG("  Adding client: " + pClient->GetIdentifier());
			pChannelClients->insert(pClient);
		}
	}
}

CModule::EModRet CClientChansMod::OnUserRaw(CString& line) {
	CIRCNetwork* pNetwork = GetNetwork();
	const CString sCommand = line.Token(0);
	CClient* pClient = GetClient();
	DEBUGLOG("[" + pClient->GetIdentifier() + "]< " + line);

	if (sCommand.Equals("JOIN")) {
		// 1. ZNC is not in a channel.
		//    -> Join the channel.
		//       The server's JOIN reply should only be sent to the requesting client.
		// 2. ZNC is already in a channel.
		//    -> Attach this client (only) to the channel.
		//       ZNC's fake JOIN reply should only be sent to the requesting client.

		const CString sChannelName = line.Token(1);
		// TODO: normalize channel name
		SetChannelVisible(pClient, sChannelName, true);
		CChan* pChannel = pNetwork->FindChan(sChannelName);
		if (pChannel) {
			pChannel->AttachUser(pClient);
			return HALT;
		}
	} else if (sCommand.Equals("PART")) {
		const CString sChannelName = line.Token(1);
		// TODO: normalize channel name

		return OnClientLeftChannel(sChannelName);
	}

	return CONTINUE;
}

CModule::EModRet CClientChansMod::OnSendToClient(CString& line, CClient& client) {
	DEBUGLOG("[" + client.GetIdentifier() + "]> " + line);
	EModRet result = CONTINUE;
	CIRCNetwork* pNetwork = client.GetNetwork();

	if (pNetwork) {
		// discard message tags
		CString msg = line;
		if (msg.StartsWith("@"))
			msg = msg.Token(1, true);

		const CNick sNick(msg.Token(0).TrimPrefix_n());
		const CString sCommand = msg.Token(1);
		const CString sRest = msg.Token(2, true);

		// Identify the channel token from (possibly) channel specific messages
		CString sChannelName;
		if (sCommand.length() == 3 && isdigit(sCommand[0]) && isdigit(sCommand[1]) && isdigit(sCommand[2])) {
			// Must block the following numeric replies that are automatically sent on attach:
			// RPL_NAMREPLY, RPL_ENDOFNAMES, RPL_TOPIC, RPL_TOPICWHOTIME...
			unsigned int num = sCommand.ToUInt();
			switch (num) {
				case 332:  // RPL_TOPIC
				case 333:  // RPL_TOPICWHOTIME
				case 366:  // RPL_ENDOFNAMES
					sChannelName = sRest.Token(1);
					break;
				case 353:  // RPL_NAMREPLY
					sChannelName = sRest.Token(2);
					break;
				case 322:  // RPL_LIST
				default:
					return CONTINUE;
			}
		} else if (sCommand.Equals("PRIVMSG") || sCommand.Equals("NOTICE") || sCommand.Equals("JOIN") ||
		           sCommand.Equals("PART") || sCommand.Equals("MODE") || sCommand.Equals("KICK") ||
		           sCommand.Equals("TOPIC")) {
			sChannelName = sRest.Token(0).TrimPrefix_n(":");
		} else {
			return CONTINUE;
		}

		// Remove status prefix (#1)
		CIRCSock* pSock = client.GetIRCSock();
		if (pSock)
			sChannelName.TrimLeft(pSock->GetISupport("STATUSMSG", ""));

		// Filter out channel specific messages for hidden channels
		if (pNetwork->IsChan(sChannelName) && !IsChannelVisible(&client, sChannelName)) {
			DEBUGLOG("  (filtered)");
			result = HALTCORE;
		}

		// For a server PART reply, clear the visibility status after the PART was relayed.
		if (sCommand.Equals("PART") && sNick.GetNick().Equals(client.GetNick()))
			SetChannelVisible(&client, sChannelName, false);
	}
	return result;
}

void CClientChansMod::OnClientDisconnect() {
	CClient* pClient = GetClient();
	DEBUGLOG("* Processing disconnect for client " + pClient->GetIdentifier());

	// Make sure to forget disconnecting clients to
	// 1) prevent dangling pointers
	// 2) part channels without any clients left.
	for (auto it = m_channelClients.begin(); it != m_channelClients.end(); ++it) {
		const CString& sChannelName = it->first;
		auto pChannelClients = &it->second;

		if (pChannelClients->find(pClient) == pChannelClients->end())
			continue;

		if (OnClientLeftChannel(sChannelName) == CONTINUE) {
			DEBUGLOG("  Parting channel " + sChannelName + " on behalf of disconnecting client");
			GetNetwork()->PutIRC("PART " + sChannelName + "\r\n");
		}
		pChannelClients->erase(pClient);
	}
}

CModule::EModRet CClientChansMod::OnClientLeftChannel(const CString& sChannelName) {
	CClient* pClient = GetClient();
	CIRCNetwork* pNetwork = GetNetwork();

	// 1. There are still other clients in this channel.
	//    -> Simulate a PART for this client only.
	// 2. There are no other clients, but the channel is "persistent".
	//    -> Tell ZNC to detach the channel.
	//       ZNC will send a PART, which should be filtered to only be sent to this client.
	// 3. There are no other clients, and the channel is not "persistent".
	//    -> Actually PART the channel.
	//       The server's PART reply should only be sent to the requesting client.

	CChan* pChannel = pNetwork->FindChan(sChannelName);

	bool bHaveOtherClients = false;
	auto pChannelClients = &m_channelClients[sChannelName];
	for (std::set<CClient*>::const_iterator it = pChannelClients->begin(); it != pChannelClients->end(); ++it)
		if (*it != pClient) {
			bHaveOtherClients = true;
			break;
		}

	// Case 1
	if (bHaveOtherClients) {
		DEBUGLOG("* Channel " + sChannelName + " has other clients, detaching this client and not PARTing");
		// Like CChan::DetachUser, but only for this client, and don't set IsDetached
		if (pClient->IsConnected())
			pClient->PutClient(":" + pClient->GetNickMask() + " PART " + sChannelName);
		return HALT;  // do not send to IRC server
	}

	// Case 2
	if (pChannel && pChannel->InConfig()) {
		DEBUGLOG("* Channel " + sChannelName + " is persistent, detaching and not PARTing");
		pChannel->DetachUser();
		return HALT;  // do not send to IRC server
	}

	// Case 3
	DEBUGLOG("* Channel " + sChannelName + " has no other clients, PARTing");
	return CONTINUE;
}

bool CClientChansMod::IsChannelVisible(CClient* pClient, const CString& sChannelName) const {
	const auto it = m_channelClients.find(sChannelName);
	return it != m_channelClients.end() && it->second.find(pClient) != it->second.end();
}

void CClientChansMod::SetChannelVisible(CClient* pClient, const CString& sChannelName, bool visible) {
	auto pChannelClients = &m_channelClients[sChannelName];
	if (visible) {
		if (pChannelClients->insert(pClient).second)
			DEBUGLOG("* Added visible channel " + sChannelName + " to client " + pClient->GetIdentifier());
	} else {
		if (pChannelClients->erase(pClient))
			DEBUGLOG("* Removed visible channel " + sChannelName + " from client " + pClient->GetIdentifier());
	}
}

template <>
void TModInfo<CClientChansMod>(CModInfo& Info) {
	Info.SetWikiPage("clientchans");
}

NETWORKMODULEDEFS(CClientChansMod, "Per-client isolation of channel lists")
