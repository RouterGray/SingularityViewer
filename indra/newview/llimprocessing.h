/**
* @file LLIMMgr.h
* @brief Container for Instant Messaging
*
* $LicenseInfo:firstyear=2001&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2010, Linden Research, Inc.
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation;
* version 2.1 of the License only.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*
* Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
* $/LicenseInfo$
*/

#ifndef LL_LLIMPROCESSING_H
#define LL_LLIMPROCESSING_H

#include "llinstantmessage.h"

struct LLCoroResponder;

class LLIMProcessing
{
public:
    // Pre-process message for IM manager
    static void processNewMessage(const LLUUID& from_id,
        BOOL from_group,
        const LLUUID& to_id,
        U8 offline,
        EInstantMessage dialog, // U8
        const LLUUID& session_id,
        U32 timestamp,
        std::string& agentName,
        std::string& message,
        U32 parent_estate_id,
        const LLUUID& region_id,
        const LLVector3& position,
        U8 *binary_bucket,
        S32 binary_bucket_size,
        LLHost &sender,
        const LLUUID& aux_id = LLUUID::null);

    // Either receives list of offline messages from 'ReadOfflineMsgs' capability
    // or uses legacy method
    static void requestOfflineMessages();

private:
    static void requestOfflineMessagesCoro(std::string url);
    static void requestOfflineMessagesLegacy();
};

// Replace wild cards in message strings
std::string replace_wildcards(std::string input, const LLUUID& id, const std::string& name);
bool handle_obj_auth(const LLUUID& from_id, const std::string& mesg);


#endif  // LL_LLLLIMPROCESSING_H
