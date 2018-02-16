/* Copyright (c) 2009
 *
 * Modular Systems. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Modular Systems nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS AND CONTRIBUTORS “AS IS”
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MODULAR SYSTEMS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Modified, debugged, optimized and improved by Henri Beauchamp Feb 2010.
 */

#ifndef JC_FLOATERAREASEARCH_H
#define JC_FLOATERAREASEARCH_H

#include "llfloater.h"
#include "lluuid.h"
#include "llstring.h"
#include "llframetimer.h"
#include "llsaleinfo.h"

class LLTextBox;
class LLScrollListCtrl;
class LLViewerRegion;
class LLViewerObject;
class LLObjectSelection;
class LLSelectNode;

class JCFloaterAreaSearch : public LLFloater, public LLFloaterSingleton<JCFloaterAreaSearch>
{
public:
	JCFloaterAreaSearch(const LLSD& data);
	virtual ~JCFloaterAreaSearch();

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void close(bool app = false);
	/*virtual*/ void onOpen();

	void results();
	static void update();
	static void receiveObjectProperties(LLUUID object_id, std::string name, std::string desc, LLSaleInfo sale_info, LLUUID owner_id, LLUUID group_id, LLUUID last_owner_id, LLUUID creator_id);
	LLViewerObject* getSelectedObject();
	void onClickEdit();
	void onClickInspect();
	void onClickDerender();
	void onClickTeleport();
	void onClickLook();
	void onClickTrack();
	void onClickUnTrack();
	void onTogglePaused();
	void onRefresh();
	void onClickOwnerProfile();
	void onClickGroupProfile();
	void onClickCreatorProfile();
	void onClickLastOwnerProfile();
	void checkRegion(bool force_clear = false);

	/*virtual*/ BOOL handleRightMouseDown(S32 x, S32 y, MASK mask);

protected:
	LLSafeHandle<LLObjectSelection> mObjectSelection;

private:

	enum OBJECT_COLUMN_ORDER
	{
		LIST_OBJECT_NAME = 0,
		LIST_OBJECT_DESC,
		LIST_OBJECT_PRIMS,
		LIST_OBJECT_IMPACT,
		LIST_OBJECT_PRICE,
		LIST_OBJECT_OWNER,
		LIST_OBJECT_GROUP,
		LIST_OBJECT_LAST_OWNER,
		LIST_OBJECT_CREATOR,
		LIST_OBJECT_COUNT
	};

	void onGetOwnerNameCallback(const LLUUID& id);
	void onGetLastOwnerNameCallback(const LLUUID& id);
	void onGetCreatorNameCallback(const LLUUID& id);

	void onCommitLine(LLUICtrl* caller, const LLSD& value, OBJECT_COLUMN_ORDER type);
	void onCheckCaseSensitive();
	bool canSelect(LLViewerObject* objectp);
	bool list_wants(LLViewerObject* objectp);
	bool requestIfNeeded(LLUUID object_id, LLViewerObject* objectp);

	void onDoubleClick();

	LLTextBox* mCounterText;
	LLScrollListCtrl* mResultList;
	LLFrameTimer mLastUpdateTimer;
	LLViewerRegion* mLastRegion;
	bool mPaused;
	bool mRegionChecked;
	bool mFirstRun;
	bool mCaseSensitive;

	struct ObjectData
	{
		LLUUID object_id;
		std::string name;
		std::string desc;
		LLUUID owner_id;
		LLUUID group_id;
		LLUUID creator_id;
		LLUUID last_owner_id;
		LLSaleInfo sale_info;
	};
	std::set<LLUUID> mPendingObjects;
	std::map<LLUUID, ObjectData> mCachedObjects;

	std::string mFilterStrings[LIST_OBJECT_COUNT];

	std::map<LLUUID, boost::signals2::scoped_connection> mOwnerNameCacheConnection;
	std::map<LLUUID, boost::signals2::scoped_connection> mLastOwnerNameCacheConnection;
	std::map<LLUUID, boost::signals2::scoped_connection> mCreatorNameCacheConnection;

	
};

#endif
