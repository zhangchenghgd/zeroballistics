
#ifndef NETWORK_VERSION_INFO_INCLUDED
#define NETWORK_VERSION_INFO_INCLUDED

#include "Datatypes.h"


namespace RakNet
{
    class BitStream;
}

//------------------------------------------------------------------------------
class VersionInfo
{
 public:
    VersionInfo();
    VersionInfo(uint8_t t, uint8_t major, uint8_t minor);

    void writeToBitstream (RakNet::BitStream & stream) const;
    bool readFromBitstream(RakNet::BitStream & stream);

    bool operator==(const VersionInfo & other) const;
    bool operator!=(const VersionInfo & other) const;
    
    uint8_t type_;
    uint8_t major_;
    uint8_t minor_;    
};

std::ostream & operator<<(std::ostream & out, const VersionInfo & info);


extern VersionInfo g_version;

const VersionInfo VERSION_PATCH_CLIENT('p', 1, 0);
const VersionInfo VERSION_PATCH_SERVER('P', 1, 0);

const VersionInfo VERSION_ZB_CLIENT('z', 2, 10);
const VersionInfo VERSION_ZB_SERVER('Z', 2, 10);

const VersionInfo VERSION_RANKING_SERVER('R', 1, 0);


// XXX rewrite master server to use handshake?
// VersionInfo g_version_master_server("M", 1, 0);

#endif
