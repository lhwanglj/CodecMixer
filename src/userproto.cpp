#include "userproto.h"
#include "userproto.pb.h"

using namespace cppcmn;

bool UserProtocol::ParseUserLogin(void *packet, int packlen, UserProtoLogin &userLogin)
{
    MEPMessage message;
    if (message.ParseFromArray(packet, packlen)) {
        if (message.has_login_request()) {
            const MEPLoginRequest &login = message.login_request();
            if (login.has_identity() && login.has_token() && login.token().size() == 16) {
                userLogin.identity = login.identity();
                memcpy(userLogin.token, login.token().c_str(), 16);
                return true;
            }
        }
    }
    return false;
}

bool UserProtocol::ParseUserJoin(void *packet, int packlen, UserProtoJoin &userJoin) 
{
    MEPMessage message;
    if (message.ParseFromArray(packet, packlen)) {
        if (message.has_join_session_request()) {
            const MEPJoinSessionRequest &request = message.join_session_request();
            if (request.has_session_id() && request.session_id().size() == 16) {
                memcpy(userJoin.session, request.session_id().c_str(), 16);
                return true;
            }
        }
    }
    return false;
}

bool UserProtocol::ParseUserPing(void *packet, int packlen, UserProtoPing &userPing)
{
    MEPMessage message;
    if (message.ParseFromArray(packet, packlen)) {
        if (message.has_ping_request()) {
            const MEPPingRequest &request = message.ping_request();
            if (request.has_tick()) {
                userPing.delay = 0;
                userPing.tick = request.tick();
                return true;
            }
        }
    }
    return false;
}

bool UserProtocol::ParseUserEnd(void *packet, int packlen, UserProtoEnd &userEnd)
{
    MEPMessage message;
    if (message.ParseFromArray(packet, packlen)) {
        if (message.has_end_session()) {
            const MEPEndSession &request = message.end_session();
            if (request.has_streams()) {
                userEnd.streams = request.streams();
                return true;
            }
        }
    }
    return false;
}

int UserProtocol::SerializeUserLoginResp(int errcode, void *buffer)
{
    MEPMessage message;
    message.mutable_base()->set_type(UserProtoTypeLoginResp);
    message.mutable_base()->set_id(0);

    MEPLoginResp *resp = message.mutable_login_resp();
    resp->set_errcode(errcode);
    
    int bytes = message.ByteSize();
    message.SerializeToArray(buffer, bytes);
    return bytes;
}

int UserProtocol::SerializeUserJoinResp(const char sessionid[], int errcode, void *buffer) 
{
    MEPMessage message;
    message.mutable_base()->set_type(UserProtoTypeJoinResp);
    message.mutable_base()->set_id(0);

    MEPJoinSessionResp *resp = message.mutable_join_session_resp();
    resp->set_session_id(sessionid, 16);
    resp->set_errcode(errcode);
    
    int bytes = message.ByteSize();
    message.SerializeToArray(buffer, bytes);
    return bytes;
}

int UserProtocol::SerializeUserPingResp(uint32_t tick, uint32_t delay, void *buffer)
{
    MEPMessage message;
    message.mutable_base()->set_type(UserProtoTypePing);
    message.mutable_base()->set_id(0);

    MEPPingResp *resp = message.mutable_ping_resp();
    resp->set_delay(delay);
    resp->set_tick(tick);
    
    int bytes = message.ByteSize();
    message.SerializeToArray(buffer, bytes);
    return bytes;
}

int UserProtocol::SerializeUserIdentities(const UserProtoIdentities &identities, void *buffer) {
    MEPMessage message;
    message.mutable_base()->set_type(UserProtoTypeIdentity);
    message.mutable_base()->set_id(0);

    MEPIdentityInfos *resp = message.mutable_identity_infos();
    for (int i = 0; i < identities.infoNum; i++) {
        resp->add_identities(identities.infos[i].identity);
        resp->add_uids(identities.infos[i].uid, identities.infos[i].uidlen);
    }
    
    int bytes = message.ByteSize();
    message.SerializeToArray(buffer, bytes);
    return bytes;
}

