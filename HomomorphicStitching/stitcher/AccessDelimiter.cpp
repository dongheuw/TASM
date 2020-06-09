//
// Created by sophi on 4/26/2018.
//

#include "AccessDelimiter.h"

namespace lightdb {

    bytestring AccessDelimiter::GetBytes() const {
        bytestring data = GetData();
        bytestring nal_marker = GetNalMarker();
        data.insert(data.begin(), nal_marker.begin(), nal_marker.end());
        return data;
    }
}