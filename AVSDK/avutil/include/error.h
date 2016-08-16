
#ifndef _MEDIACLOUD_ERROR_H
#define _MEDIACLOUD_ERROR_H

#include <string>

namespace MediaCloud
{
    namespace Common
    {
        enum ErrCode
        {
            ErrCodeNone = 0,
            ErrCodeUnknown = -1000,
            ErrCodeArgument,
            ErrCodeNoImpl,
            ErrCodeNoData,
            ErrCodeNoRoom,
            ErrCodeNotAvailable,
            ErrCodeNotInit,
            ErrCodeOutOfMemory,
            ErrCodeNotReady,
            ErrCodeAlready,
            ErrCodeRecovery,
            ErrCodeDiscover,
            ErrCodeNetwork,
            ErrCodeNoUser,      /// UserOnline doesn't called before making session operations
            ErrCodeIdentity,    /// the user/mr identity is not valid on server
            ErrCodeAccount,
            ErrCodeSignature,
            ErrCodeToken,
            ErrCodeAttribute,
            ErrCodeBuffer,
            ErrCodeLimitation,  /// the limitation is met, like: the max online user number is satisfied.

            ErrCodeAccountCookie,   /// the account cookie is not valid, the app need to verify its account again to get new cookie
            ErrCodeUserCookie,      /// the user cookie is not valid on server matching the user identity

            ErrCodeTerminated,
            ErrCodeNotSupport,
            ErrCodeReject,
            ErrCodeTimeout,
            ErrCodeClosed,
            ErrCodeSocket,
            ErrCodeSocketEagain,
            ErrCodeInPhoneCall,
            ErrCodeBusy,
            ErrCodeSession,
            ErrCodeAccessRight,
            ErrCodeVersion,
            ErrCodeKickOut,

            ErrCodeEncoder,

            ErrCodeAudioProperty,
            ErrCodeAudioSession,
            ErrCodeAudioDevice,
            ErrCodeAudioFormat,
            ErrCodeAudioResampler,

            ErrCodePacket,
            ErrCodePort,

            ErrCodeRender,
            ErrCodeOpenGL,
            ErrCodeTexture,
            ErrCodeShader,

            ErrCodeFile,
        };

        struct Error
        {
            int code;
            std::string message;
        };
    }
}

#endif