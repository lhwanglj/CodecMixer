#ifndef __AVM_MIXERCOMMON_H__
#define __AVM_MIXERCOMMON_H__


#define AVM_MAX_URL_LENGTH     1024
#define AVM_MAX_NETPEER_SIZE   1


namespace MediaCloud
{
    typedef enum e_PeerRoleType
    {
        E_AVM_PEERROLE_UNKNOW,
        E_AVM_PEERROLE_LEADING,
        E_AVM_PEERROLE_MINOR,
        E_AVM_PEERROLE_OTHER,
    }E_PEERROLETYPE;
 
    typedef enum e_PicMixType
    {
        E_AVM_PICMIX_UNKNOW,
        E_AVM_PICMIX_RIGHT,
        E_AVM_PICMIX_BOTTOM,
    }E_PICMIXTYPE;

    typedef struct t_PicMixConf
    {
        E_PICMIXTYPE eType;
        int  iEdgeSpace;
        t_PicMixConf()
        {
            eType=E_AVM_PICMIX_UNKNOW;
            iEdgeSpace=0;
        }
        t_PicMixConf& operator=(const t_PicMixConf& rhs)
        {
            if(this == &rhs)
                *this;
            eType = rhs.eType;
            iEdgeSpace = rhs.iEdgeSpace;
            return *this;
        }
    }T_PICMIXCONF, *PT_PICMIXCONF;
    
}

#endif /* __AVM_MIXERCOMMON_H__ */
