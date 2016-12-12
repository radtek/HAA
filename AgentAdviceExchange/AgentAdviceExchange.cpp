// AgentAdviceExchange.cpp : Defines the initialization routines for the DLL.

#include "stdafx.h"

#include "..\\autonomic\\autonomic.h"

#include "AgentAdviceExchange.h"
#include "AgentAdviceExchangeVersion.h"


#include <fstream>      // std::ifstream


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//*****************************************************************************
// AgentAdviceExchange

//-----------------------------------------------------------------------------
// Constructor	

AgentAdviceExchange::AgentAdviceExchange(spAddressPort ap, UUID *ticket, int logLevel, char *logDirectory, char playbackMode, char *playbackFile) : AgentBase(ap, ticket, logLevel, logDirectory, playbackMode, playbackFile) {
	// allocate state
	ALLOCATE_STATE(AgentAdviceExchange, AgentBase)

	// Assign the UUID
	sprintf_s(STATE(AgentBase)->agentType.name, sizeof(STATE(AgentBase)->agentType.name), "AgentAdviceExchange");
	UUID typeId;
	UuidFromString((RPC_WSTR)_T(AgentAdviceExchange_UUID), &typeId);
	STATE(AgentBase)->agentType.uuid = typeId;

	if (AgentAdviceExchange_TRANSFER_PENALTY < 1) STATE(AgentBase)->noCrash = false; // we're ok to crash

	// Initialize state members
	STATE(AgentAdviceExchange)->parametersSet = false;
	STATE(AgentAdviceExchange)->startDelayed = false;
	STATE(AgentAdviceExchange)->updateId = -1;
	STATE(AgentAdviceExchange)->setupComplete = false;


	//---------------------------------------------------------------
	// TODO Data that must be loaded in

	// Initialize advice data
	this->num_state_vrbls_ = 7;
	this->num_actions_ = 5;

	// Seed the random number generator
	srand(static_cast <unsigned> (time(0)));

	// Prepare callbacks
	this->callback[AgentAdviceExchange_CBR_convGetAgentList] = NEW_MEMBER_CB(AgentAdviceExchange, convGetAgentList);
	this->callback[AgentAdviceExchange_CBR_convGetAgentInfo] = NEW_MEMBER_CB(AgentAdviceExchange, convGetAgentInfo);
	this->callback[AgentAdviceExchange_CBR_convAdviceQuery] = NEW_MEMBER_CB(AgentAdviceExchange, convAdviceQuery);

}// end constructor

//-----------------------------------------------------------------------------
// Destructor

AgentAdviceExchange::~AgentAdviceExchange() {

	if (STATE(AgentBase)->started) {
		this->stop();
	}// end started if

}// end destructor

//-----------------------------------------------------------------------------
// Configure

int AgentAdviceExchange::configure() {
	// Create logger
	if (Log.getLogMode() == LOG_MODE_OFF) {
		// Init logging
		RPC_WSTR rpc_wstr;
		char logName[256];
		char timeBuf[64];
		time_t t_t;
		struct tm stm;
		apb->apbtime(&t_t);
		localtime_s(&stm, &t_t);
		strftime(timeBuf, 64, "%y.%m.%d [%H.%M.%S]", &stm);
		UuidToString(&STATE(AgentBase)->uuid, &rpc_wstr);
		sprintf_s(logName, "%s\\AgentAdviceExchange %s %ls.txt", logDirectory, timeBuf, rpc_wstr);

		Log.setLogMode(LOG_MODE_COUT);
		Log.setLogMode(LOG_MODE_FILE, logName);
		Log.setLogLevel(LOG_LEVEL_VERBOSE);
		Log.log(0, "AgentAdviceExchange %.2d.%.2d.%.5d.%.2d", AgentAdviceExchange_MAJOR, AgentAdviceExchange_MINOR, AgentAdviceExchange_BUILDNO, AgentAdviceExchange_EXTEND);
	}// end if

	if (AgentBase::configure()) {
		return 1;
	}// end if

	return 0;
}// end configure

int AgentAdviceExchange::configureParameters(DataStream *ds) {
	UUID uuid;
	// Read in owner ID
	ds->unpackUUID(&STATE(AgentAdviceExchange)->ownerId);

	Log.log(LOG_LEVEL_NORMAL, "AgentAdviceExchange::configureParameters: ownerId %s", Log.formatUUID(LOG_LEVEL_NORMAL, &STATE(AgentAdviceExchange)->ownerId));

	this->backup();

	return 0;
}// end configureParameters

int	AgentAdviceExchange::finishConfigureParameters() {

	STATE(AgentAdviceExchange)->parametersSet = true;


	if (STATE(AgentAdviceExchange)->startDelayed) {
		STATE(AgentAdviceExchange)->startDelayed = false;
		this->start(STATE(AgentBase)->missionFile);
	}// end if

	this->backup();

	return 0;
}// end finishConfigureParameters

//-----------------------------------------------------------------------------
// Start

int AgentAdviceExchange::start(char *missionFile) {
	DataStream lds;

	// Delay start if parameters not set
	if (!STATE(AgentAdviceExchange)->parametersSet) { // delay start
		strcpy_s(STATE(AgentBase)->missionFile, sizeof(STATE(AgentBase)->missionFile), missionFile);
		STATE(AgentAdviceExchange)->startDelayed = true;
		return 0;
	}// end if

	if (AgentBase::start(missionFile)) {
		return 1;
	}// end if

	STATE(AgentBase)->started = false;

	// register as avatar watcher
	Log.log(0, "AgentAdviceExchange::start: registering as agent watcher");
	lds.reset();
	lds.packUUID(&STATE(AgentBase)->uuid);
	lds.packInt32(DDB_AGENT);
	this->sendMessage(this->hostCon, MSG_DDB_WATCH_TYPE, lds.stream(), lds.length());
	lds.unlock();
	// NOTE: we request a list of agents once DDBE_WATCH_TYPE notification is received

	STATE(AgentBase)->started = true;
	return 0;
}// end start


//-----------------------------------------------------------------------------
// Stop

int AgentAdviceExchange::stop() {

	if (!this->frozen) {
		// TODO
	}// end if

	return AgentBase::stop();
}// end stop


//-----------------------------------------------------------------------------
// Step

int AgentAdviceExchange::step() {
	//Log.log(0, "AgentAdviceExchange::step()");

	return AgentBase::step();
}// end step

 /* askAdvisers
 *
 * Sends the state vector to all the known advisers
 */
int AgentAdviceExchange::askAdvisers() {
	// Send state vector to all advisers, to receive their advice in return
	// TODO (demo below)

	// Demonstration using only the parent agent, since we are not yet aware of the other advisers
	// (later, this will not be necessary, since the parent agent's q values are received when advice is requested)

	// Temporarily add the parent agent as the only adviser
	if (this->adviceQuery.empty()) {
		Log.log(0, "AgentAdviceExchange::askAdvisers: adviceQuery is empty, adding myself.");
		adviceQueryData temp_data;
		this->adviceQuery[STATE(AgentAdviceExchange)->ownerId] = temp_data;
	}
	
	DataStream lds;
	std::map<UUID, adviceQueryData, UUIDless>::iterator iter;
	for (iter = this->adviceQuery.begin(); iter != this->adviceQuery.end(); iter++) {
		// Clear the response
		iter->second.response = false;
	
        // Initiate conversation with adviser
		iter->second.conv = this->conversationInitiate(AgentAdviceExchange_CBR_convAdviceQuery, DDB_REQUEST_TIMEOUT);
		if (iter->second.conv == nilUUID) {
			return 1;
		}

		// Send a message with the state vector
		lds.reset();
		lds.packUUID(&iter->second.conv); // Add the thread
		lds.packUUID(&STATE(AgentBase)->uuid);  // Add sender Id
		
		// Pack the state vector
		for (std::vector<unsigned int>::iterator state_iter = this->state_vector_in.begin(); state_iter != this->state_vector_in.end(); ++state_iter) {
			lds.packUInt32(*state_iter);
		}

		this->sendMessageEx(this->hostCon, MSGEX(AgentAdviceExchange_MSGS, MSG_GET_Q_VALUES), lds.stream(), lds.length(), &STATE(AgentAdviceExchange)->ownerId);
		lds.unlock();
	}

	return 0;
}

/* formAdvice
*
* Performs the AdviceExchange Algorithm
*/
int AgentAdviceExchange::formAdvice() {
	// TODO: Add the actual algorithm
	
	// Temporarily default to forwarding back the original q_vals
	std::vector<float> advice = this->adviceQuery[STATE(AgentAdviceExchange)->ownerId].quality;
	
	DataStream lds;
	lds.reset();
	lds.packUUID(&this->adviceRequestConv);
	lds.packUUID(&STATE(AgentBase)->uuid);

	// Pack the advised Q values
	for (std::vector<float>::iterator q_iter = advice.begin(); q_iter != advice.end(); ++q_iter) {
		lds.packFloat32(*q_iter);
	}
	this->sendMessage(this->hostCon, MSG_RESPONSE, lds.stream(), lds.length(), &STATE(AgentAdviceExchange)->ownerId);
	lds.unlock();

	Log.log(0, "AgentAdviceExchange::formAdvice: Sent advice.");

	return 0;
}

int AgentAdviceExchange::ddbNotification(char *data, int len) {
	DataStream lds, sds;
	UUID uuid;

	int offset;
	int type;
	char evt;
	lds.setData(data, len);
	lds.unpackData(sizeof(UUID)); // key
	type = lds.unpackInt32();
	lds.unpackUUID(&uuid);
	evt = lds.unpackChar();
	offset = 4 + sizeof(UUID) * 2 + 1;
	if (evt == DDBE_WATCH_TYPE) {
		if (type == DDB_AGENT) {
			// request list of avatars
			UUID thread = this->conversationInitiate(AgentAdviceExchange_CBR_convGetAgentList, DDB_REQUEST_TIMEOUT);
			if (thread == nilUUID) {
				return 1;
			}
			sds.reset();
			sds.packUUID(this->getUUID()); // dummy id 
			sds.packInt32(DDBAVATARINFO_ENUM);
			sds.packUUID(&thread);
			this->sendMessage(this->hostCon, MSG_DDB_AVATARGETINFO, sds.stream(), sds.length());
			sds.unlock();
		}
	}
	if (type == DDB_AVATAR) {
		if (evt == DDBE_ADD) {
			// add avatar

			// request avatar info
			UUID thread = this->conversationInitiate(AgentAdviceExchange_CBR_convGetAgentInfo, DDB_REQUEST_TIMEOUT, &uuid, sizeof(UUID));
			if (thread == nilUUID) {
				return 1;
			}
			sds.reset();
			sds.packUUID(&uuid);
			sds.packInt32(DDBAVATARINFO_RAGENT| DDBAVATARINFO_RPF | DDBAVATARINFO_RRADII | DDBAVATARINFO_RTIMECARD);
			sds.packUUID(&thread);
			this->sendMessage(this->hostCon, MSG_DDB_AVATARGETINFO, sds.stream(), sds.length());
			sds.unlock();
		}
		else if (evt == DDBE_UPDATE) {
			int infoFlags = lds.unpackInt32();
			if (infoFlags & DDBAVATARINFO_RETIRE) {
				// request avatar info
				UUID thread = this->conversationInitiate(AgentAdviceExchange_CBR_convGetAgentInfo, DDB_REQUEST_TIMEOUT, &uuid, sizeof(UUID));
				if (thread == nilUUID) {
					return 1;
				}
				sds.reset();
				sds.packUUID(&uuid);
				sds.packInt32(DDBAVATARINFO_RAGENT | DDBAVATARINFO_RPF | DDBAVATARINFO_RRADII | DDBAVATARINFO_RTIMECARD);
				sds.packUUID(&thread);
				this->sendMessage(this->hostCon, MSG_DDB_AVATARGETINFO, sds.stream(), sds.length());
				sds.unlock();
			}
		}
		else if (evt == DDBE_REM) {
			// TODO
		}
	}
	lds.unlock();

	return 0;
}

//-----------------------------------------------------------------------------
// Process message

int AgentAdviceExchange::conProcessMessage(spConnection con, unsigned char message, char *data, unsigned int len) {
	DataStream lds;

	if (!AgentBase::conProcessMessage(con, message, data, len)) // message handled
		return 0;

	switch (message) {
	case AgentAdviceExchange_MSGS::MSG_CONFIGURE:
	{
		lds.setData(data, len);
		this->configureParameters(&lds);
		lds.unlock();
		break;
	}
	break;
	case AgentAdviceExchange_MSGS::MSG_GET_ADVICE:
	{
		// Clear the old data
		this->q_vals_in.clear();
		this->state_vector_in.clear();

		UUID sender;
		lds.setData(data, len);

		lds.unpackUUID(&this->adviceRequestConv);
		lds.unpackUUID(&sender);
	
		// Unpack Q values
		for (int i = 0; i < this->num_actions_; i++) {
			this->q_vals_in.push_back(lds.unpackFloat32());
		}
		// Unpack state vector
		for (int j = 0; j < this->num_state_vrbls_; j++) {
			this->state_vector_in.push_back(lds.unpackUInt32());
		}
		lds.unlock();

		// Proceed to ask advisers for advice
		this->askAdvisers();
	}
	break;
	default:
		return 1; // unhandled message
	}

	return 0;
}// end conProcessMessage

//-----------------------------------------------------------------------------
// Callbacks


bool AgentAdviceExchange::convGetAgentList(void *vpConv) {
	//DataStream lds, sds;
	//spConversation conv = (spConversation)vpConv;

	//if (conv->response == NULL) { // timed out
	//	Log.log(0, "AgentAdviceExchange::convGetAvatarList: timed out");
	//	return 0; // end conversation
	//}

	//lds.setData(conv->response, conv->responseLen);
	//lds.unpackData(sizeof(UUID)); // discard thread

	//char response = lds.unpackChar();
	//if (response == DDBR_OK) { // succeeded
	//	int i, count;

	//	if (lds.unpackInt32() != DDBAVATARINFO_ENUM) {
	//		lds.unlock();
	//		return 0; // what happened here?
	//	}

	//	count = lds.unpackInt32();
	//	Log.log(LOG_LEVEL_VERBOSE, "AgentAdviceExchange::convGetAvatarList: recieved %d avatars", count);

	//	UUID thread;
	//	UUID avatarId;
	//	UUID agentId;
	//	AgentType agentType;

	//	for (i = 0; i < count; i++) {
	//		lds.unpackUUID(&avatarId);
	//		lds.unpackString(); // avatar type
	//		lds.unpackUUID(&agentId);
	//		lds.unpackUUID(&agentType.uuid);
	//		agentType.instance = lds.unpackInt32();

	//		if (agentId == STATE(AgentAdviceExchange)->ownerId) {
	//			// This is our avatar
	//			this->avatar.ready = false;
	//			this->avatar.start.time = 0;
	//			this->avatar.retired = 0;
	//			this->avatarId = avatarId;
	//		}
	//		else {
	//			this->otherAvatars[avatarId].ready = false;
	//			this->otherAvatars[avatarId].start.time = 0;
	//			this->otherAvatars[avatarId].retired = 0;
	//			//this->otherAvatarsLocUpdateId[avatarId] = -1;
	//		}

	//		// request avatar info
	//		thread = this->conversationInitiate(AgentAdviceExchange_CBR_convGetAgentInfo, DDB_REQUEST_TIMEOUT, &avatarId, sizeof(UUID));
	//		if (thread == nilUUID) {
	//			return 1;
	//		}
	//		sds.reset();
	//		sds.packUUID((UUID *)&avatarId);
	//		sds.packInt32(DDBAVATARINFO_RAGENT | DDBAVATARINFO_RPF | DDBAVATARINFO_RRADII | DDBAVATARINFO_RTIMECARD);
	//		sds.packUUID(&thread);
	//		this->sendMessage(this->hostCon, MSG_DDB_AVATARGETINFO, sds.stream(), sds.length());
	//		sds.unlock();
	//	}
	//	lds.unlock();

	//}
	//else {
	//	lds.unlock();
	//	// TODO try again?
	//}

	return 0;
}

bool AgentAdviceExchange::convGetAgentInfo(void *vpConv) {

	//DataStream lds;
	//spConversation conv = (spConversation)vpConv;

	//if (conv->response == NULL) { // timed out
	//	Log.log(0, "AgentAdviceExchange::convGetAgentInfo: timed out");
	//	return 0; // end conversation
	//}

	//UUID avatarId = *(UUID *)conv->data;

	//lds.setData(conv->response, conv->responseLen);
	//lds.unpackData(sizeof(UUID)); // discard thread

	//char response = lds.unpackChar();
	//if (response == DDBR_OK) { // succeeded
	//	UUID pfId, avatarAgent;
	//	char retired;
	//	float innerR, outerR;
	//	_timeb startTime, endTime;

	//	if (lds.unpackInt32() != (DDBAVATARINFO_RAGENT | DDBAVATARINFO_RPF | DDBAVATARINFO_RRADII | DDBAVATARINFO_RTIMECARD)) {
	//		lds.unlock();
	//		return 0; // what happened here?
	//	}

	//	lds.unpackUUID(&avatarAgent);
	//	lds.unpackUUID(&pfId);
	//	innerR = lds.unpackFloat32();
	//	outerR = lds.unpackFloat32();
	//	startTime = *(_timeb *)lds.unpackData(sizeof(_timeb));
	//	retired = lds.unpackChar();
	//	if (retired)
	//		endTime = *(_timeb *)lds.unpackData(sizeof(_timeb));
	//	lds.unlock();

	//	Log.log(LOG_LEVEL_VERBOSE, "AgentAdviceExchange::convGetAgentInfo: recieved agent (%s) ",
	//		Log.formatUUID(LOG_LEVEL_VERBOSE, &agentId));

	//	if (avatarId == this->avatarId) {
	//		// This is our avatar
	//		this->avatarAgentId = avatarAgent;
	//		this->avatar.pf = pfId;
	//		this->avatar.start = startTime;
	//		if (retired)
	//			this->avatar.end = endTime;
	//		this->avatar.retired = retired;
	//		this->avatar.innerRadius = innerR;
	//		this->avatar.outerRadius = outerR;
	//		this->avatar.ready = true;
	//	}
	//	else {
	//		// This is a teammate
	//		this->otherAvatars[avatarId].pf = pfId;
	//		this->otherAvatars[avatarId].start = startTime;
	//		if (retired)
	//			this->otherAvatars[avatarId].end = endTime;
	//		this->otherAvatars[avatarId].retired = retired;
	//		this->otherAvatars[avatarId].innerRadius = innerR;
	//		this->otherAvatars[avatarId].outerRadius = outerR;
	//		this->otherAvatars[avatarId].ready = true;
	//	}
	//}
	//else {
	//	lds.unlock();

	//	// TODO try again?
	//}

	return 0;
}

bool AgentAdviceExchange::convAdviceQuery(void *vpConv) {
	spConversation conv = (spConversation)vpConv;

	if (conv->response == NULL) {
		Log.log(0, "AgentAdviceExchange::convAdviceQuery: request timed out");
		return 0; // end conversation
	}

	// Start unpacking
	DataStream lds;
	UUID thread;
	UUID sender;
	
	lds.setData(conv->response, conv->responseLen);
	lds.unpackUUID(&thread);
	lds.unpackUUID(&sender);

	int response_count = 0;
	
	std::map<UUID, adviceQueryData, UUIDless>::iterator iter;
	for (iter = this->adviceQuery.begin(); iter != this->adviceQuery.end(); iter++) {
		// Check for the right adviser and thread
		if (iter->first == sender) {
			// Unpack Q values
			iter->second.quality.clear();
			for (int i = 0; i < this->num_actions_; i++) {
				iter->second.quality.push_back(lds.unpackFloat32());
			}
			this->adviceQuery[iter->first].response = true;
			response_count++;
		}
	}
	lds.unlock();
	
	// Only proceed to form advice once we have heard from all advisers
	if (response_count == this->adviceQuery.size()) {
		this->formAdvice();
	}

	return 0;
}

//-----------------------------------------------------------------------------
// State functions

int AgentAdviceExchange::freeze(UUID *ticket) {
	return AgentBase::freeze(ticket);
}// end freeze

int AgentAdviceExchange::thaw(DataStream *ds, bool resumeReady) {
	return AgentBase::thaw(ds, resumeReady);
}// end thaw

int	AgentAdviceExchange::writeState(DataStream *ds, bool top) {
	if (top) _WRITE_STATE(AgentAdviceExchange);

	return AgentBase::writeState(ds, false);
}// end writeState

int	AgentAdviceExchange::readState(DataStream *ds, bool top) {
	if (top) _READ_STATE(AgentAdviceExchange);

	return AgentBase::readState(ds, false);
}// end readState

int AgentAdviceExchange::recoveryFinish() {
	if (AgentBase::recoveryFinish())
		return 1;

	return 0;
}// end recoveryFinish

int AgentAdviceExchange::writeBackup(DataStream *ds) {

	// Agent Data
	ds->packUUID(&STATE(AgentAdviceExchange)->ownerId);
	ds->packBool(STATE(AgentAdviceExchange)->parametersSet);
	ds->packBool(STATE(AgentAdviceExchange)->startDelayed);
	ds->packInt32(STATE(AgentAdviceExchange)->updateId);
	ds->packBool(STATE(AgentAdviceExchange)->setupComplete);

	return AgentBase::writeBackup(ds);
}// end writeBackup

int AgentAdviceExchange::readBackup(DataStream *ds) {
	DataStream lds;

	// configuration
	ds->unpackUUID(&STATE(AgentAdviceExchange)->ownerId);
	STATE(AgentAdviceExchange)->parametersSet = ds->unpackBool();
	STATE(AgentAdviceExchange)->startDelayed = ds->unpackBool();
	STATE(AgentAdviceExchange)->updateId = ds->unpackInt32();
	STATE(AgentAdviceExchange)->setupComplete = ds->unpackBool();

	if (STATE(AgentAdviceExchange)->setupComplete) {
		this->finishConfigureParameters();
	}
	else {
		//TODO
	}

	return AgentBase::readBackup(ds);
}// end readBackup


//*****************************************************************************
// Threading

DWORD WINAPI RunThread(LPVOID vpAgent) {
	AgentAdviceExchange *agent = (AgentAdviceExchange *)vpAgent;

	if (agent->configure()) {
		return 1;
	}

	while (!agent->step());

	if (agent->isStarted())
		agent->stop();
	delete agent;

	return 0;
}

int Spawn(spAddressPort ap, UUID *ticket, int logLevel, char *logDirectory, char playbackMode, char *playbackFile) {
	AgentAdviceExchange *agent = new AgentAdviceExchange(ap, ticket, logLevel, logDirectory, playbackMode, playbackFile);

	HANDLE hThread;
	DWORD  dwThreadId;

	hThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		RunThread,				// thread function name
		agent,					// argument to thread function 
		0,                      // use default creation flags 
		&dwThreadId);			// returns the thread identifier 

	if (hThread == NULL) {
		delete agent;
		return 1;
	}

	return 0;
}// end Spawn

int Playback(spAddressPort ap, UUID *ticket, int logLevel, char *logDirectory, char playbackMode, char *playbackFile) {
	AgentAdviceExchange *agent = new AgentAdviceExchange(ap, ticket, logLevel, logDirectory, playbackMode, playbackFile);

	if (agent->configure()) {
		delete agent;
		return 1;
	}

	while (!agent->step());

	if (agent->isStarted())
		agent->stop();

	printf_s("Playback: agent finished\n");

	delete agent;

	return 0;
}// end Playback


// CAgentPathPlannerDLL: Override ExitInstance for debugging memory leaks
CAgentAdviceExchangeDLL::CAgentAdviceExchangeDLL() {}

// The one and only CAgentPathPlannerDLL object
CAgentAdviceExchangeDLL theApp;

int CAgentAdviceExchangeDLL::ExitInstance() {
	TRACE(_T("--- ExitInstance() for regular DLL: CAgentAdviceExchange ---\n"));
	return CWinApp::ExitInstance();
}