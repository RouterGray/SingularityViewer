/* Copyright (c) 2009
 *
 * Modular Systems Ltd. All rights reserved.
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
 *   3. Neither the name Modular Systems Ltd nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS LTD AND CONTRIBUTORS "AS IS"
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

#include "llviewerprecompiledheaders.h"

#include "jcfloaterareasearch.h"

#include "llcheckboxctrl.h"
#include "llfiltereditor.h"
#include "llscrolllistcolumn.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llavataractions.h"
#include "llavatarnamecache.h"
#include "llcontrol.h"
#include "llfloaterinspect.h"
#include "llfloatertools.h"
#include "llgroupactions.h"
#include "llresmgr.h"
#include "llselectmgr.h"
#include "llpermissions.h"
#include "lltracker.h"
#include "lltrans.h"
#include "llviewerregion.h"
#include "llviewerobjectlist.h"
#include "rlvhandler.h"

const F32 min_refresh_interval = 0.25f;	// Minimum interval between list refreshes in seconds.
const LLUUID get_focused_list_id_selected();
const uuid_vec_t get_focused_list_ids_selected();

const S32& areasearch_name_system()
{
	static const LLCachedControl<S32> name_system("AreaSearchNameSystem", 0);
	return name_system;
}

void handle_object_edit();
bool add_object_to_blacklist(const LLUUID& id, const std::string& entry_name);

JCFloaterAreaSearch::JCFloaterAreaSearch(const LLSD& data) :
	LLFloater(),
	mCounterText(0),
	mResultList(0),
	mLastRegion(0),
	mPaused(false),
	mRegionChecked(false),
	mFirstRun(true),
	mCaseSensitive(false)
{
	mLastUpdateTimer.reset();
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_area_search.xml");
}

JCFloaterAreaSearch::~JCFloaterAreaSearch()
{
}


void JCFloaterAreaSearch::close(bool app)
{
	if (app || mPaused)
	{
		LLFloater::close(app);
	}
	else
	{
		setMinimized(false);
		setVisible(false);
	}
}

BOOL JCFloaterAreaSearch::postBuild()
{
	mResultList = getChild<LLScrollListCtrl>("result_list");
	mResultList->setDoubleClickCallback(boost::bind(&JCFloaterAreaSearch::onDoubleClick,this));
	mResultList->sortByColumn("Name", TRUE);

	mCounterText = getChild<LLTextBox>("counter");

	getChild<LLFilterEditor>("Name query chunk")->setCommitCallback(boost::bind(&JCFloaterAreaSearch::onCommitLine,this,_1,_2,LIST_OBJECT_NAME));
	getChild<LLFilterEditor>("Description query chunk")->setCommitCallback(boost::bind(&JCFloaterAreaSearch::onCommitLine,this,_1,_2,LIST_OBJECT_DESC));
	getChild<LLFilterEditor>("Owner query chunk")->setCommitCallback(boost::bind(&JCFloaterAreaSearch::onCommitLine,this,_1,_2,LIST_OBJECT_OWNER));
	getChild<LLFilterEditor>("Group query chunk")->setCommitCallback(boost::bind(&JCFloaterAreaSearch::onCommitLine,this,_1,_2,LIST_OBJECT_GROUP));
	getChild<LLFilterEditor>("Creator query chunk")->setCommitCallback(boost::bind(&JCFloaterAreaSearch::onCommitLine,this,_1,_2,LIST_OBJECT_CREATOR));
	getChild<LLFilterEditor>("Last Owner query chunk")->setCommitCallback(boost::bind(&JCFloaterAreaSearch::onCommitLine,this,_1,_2,LIST_OBJECT_LAST_OWNER));
	getChild<LLCheckBoxCtrl>("check_case_sensitive")->setCommitCallback(boost::bind(&JCFloaterAreaSearch::onCheckCaseSensitive, this));

	childSetValue("check_case_sensitive", mCaseSensitive);

	return TRUE;
}

void JCFloaterAreaSearch::onOpen()
{
	checkRegion();
	results();
}

BOOL JCFloaterAreaSearch::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	return LLFloater::handleRightMouseDown(x, y, mask);
}

namespace
{
	typedef LLMemberListener<LLView> view_listener_t;

	class AreaSearchEdit : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			JCFloaterAreaSearch::findInstance()->onClickEdit();
			return true;
		}
	};

	class AreaSearchInspect : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			JCFloaterAreaSearch::findInstance()->onClickInspect();
			return true;
		}
	};

	class AreaSearchDerender : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			JCFloaterAreaSearch::findInstance()->onClickDerender();
			return true;
		}
	};

	class AreaSearchTrack : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			JCFloaterAreaSearch::findInstance()->onClickTrack();
			return true;
		}
	};

	class AreaSearchUnTrack : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			JCFloaterAreaSearch::findInstance()->onClickUnTrack();
			return true;
		}
	};

	class AreaSearchLook : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			JCFloaterAreaSearch::findInstance()->onClickLook();
			return true;
		}
	};

	class AreaSearchTeleport : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			JCFloaterAreaSearch::findInstance()->onClickTeleport();
			return true;
		}
	};
	class AreaSearchOwnerProfile : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			JCFloaterAreaSearch::findInstance()->onClickOwnerProfile();
			return true;
		}
	};
	class AreaSearchGroupProfile : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			JCFloaterAreaSearch::findInstance()->onClickGroupProfile();
			return true;
		}
	};
	class AreaSearchLastOwnerProfile : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			JCFloaterAreaSearch::findInstance()->onClickLastOwnerProfile();
			return true;
		}
	};
	class AreaSearchCreatorProfile : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			JCFloaterAreaSearch::findInstance()->onClickCreatorProfile();
			return true;
		}
	};
	class AreaSearchTogglePaused : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			JCFloaterAreaSearch::findInstance()->onTogglePaused();
			return true;
		}
	};
	class AreaSearchRefresh : public view_listener_t
	{
		bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
		{
			JCFloaterAreaSearch::findInstance()->onRefresh();
			return true;
		}
	};
}

void addMenu(view_listener_t* menu, const std::string& name);

void add_areasearch_listeners()
{
	addMenu(new AreaSearchEdit(), "AreaSearch.Edit");
	addMenu(new AreaSearchInspect(), "AreaSearch.Inspect");
	addMenu(new AreaSearchDerender(), "AreaSearch.Derender");
	addMenu(new AreaSearchTrack(), "AreaSearch.Track");
	addMenu(new AreaSearchUnTrack(), "AreaSearch.UnTrack");
	addMenu(new AreaSearchLook(), "AreaSearch.Look");
	addMenu(new AreaSearchTeleport(), "AreaSearch.Teleport");
	addMenu(new AreaSearchOwnerProfile(), "AreaSearch.Owner");
	addMenu(new AreaSearchGroupProfile(), "AreaSearch.Group");
	addMenu(new AreaSearchLastOwnerProfile(), "AreaSearch.Last Owner");
	addMenu(new AreaSearchCreatorProfile(), "AreaSearch.Creator");
	addMenu(new AreaSearchTogglePaused(), "AreaSearch.(Un)Pause");
	addMenu(new AreaSearchRefresh(), "AreaSearch.Refresh");
}

void JCFloaterAreaSearch::checkRegion(bool force_clear)
{
	// Check if we changed region, and if we did, clear the object details cache.
	LLViewerRegion* region = gAgent.getRegion();
	if (force_clear || region != mLastRegion)
	{
		mLastRegion = region;
		mPendingObjects.clear();
		mCachedObjects.clear();
		mResultList->deleteAllItems();
		mCounterText->setText(std::string("Listed/Pending/Total"));
		mLastUpdateTimer.reset();
		mFirstRun = true;
	}
}

bool JCFloaterAreaSearch::canSelect(LLViewerObject* objectp)
{
	return LLSelectMgr::getInstance()->canSelectObject(objectp);
}

bool JCFloaterAreaSearch::list_wants(LLViewerObject* objectp)
{
	if (!objectp->isRoot())
		return false;

	if (!objectp->getRegion())
		return false;

	if (!canSelect(objectp))
		return false;

	if (objectp->flagTemporary())
		return false;

	if (objectp->flagTemporaryOnRez())
		return false;

	return true;
}

LLViewerObject* JCFloaterAreaSearch::getSelectedObject()
{
	auto id = get_focused_list_id_selected();
	return id.isNull() ? nullptr : gObjectList.findObject(id);
}

void JCFloaterAreaSearch::onDoubleClick()
{
	if (LLViewerObject* objectp = getSelectedObject())
		LLTracker::trackLocation(objectp->getPositionGlobal(), mCachedObjects[objectp->getID()].name, "", LLTracker::LOCATION_ITEM);
}

void JCFloaterAreaSearch::onClickTeleport()
{
	if (LLViewerObject* objectp = getSelectedObject())
		gAgent.teleportViaLocation(objectp->getPositionGlobal());
}

void JCFloaterAreaSearch::onClickLook()
{
	if (LLViewerObject* objectp = getSelectedObject())
		gAgentCamera.lookAtObject(objectp->getID(), false);
}

void JCFloaterAreaSearch::onClickEdit()
{
	if (LLViewerObject* objectp = getSelectedObject())
	{
		LLSelectMgr* sel_mgr = LLSelectMgr::getInstance();
		sel_mgr->deselectAll();
		sel_mgr->selectObjectAndFamily(objectp);
		handle_object_edit();
		if (gFloaterTools && gFloaterTools->getVisible())
			gFloaterView->bringToFront(gFloaterTools);
	}
}

void JCFloaterAreaSearch::onClickInspect()
{
	if (LLViewerObject* objectp = getSelectedObject())
	{
		LLFloater* inspect_floater = LLFloaterInspect::getInstance();
		LLSelectMgr* sel_mgr = LLSelectMgr::getInstance();
		sel_mgr->deselectAll();
		inspect_floater->setVisible(true);
		sel_mgr->selectObjectAndFamily(objectp);
		gFloaterView->bringToFront(inspect_floater);
	}
}

void JCFloaterAreaSearch::onClickDerender()
{
	if (LLViewerObject* objectp = getSelectedObject())
	{
		LLViewerRegion* cur_region = gAgent.getRegion();

		if (objectp->getRegion() != cur_region)
			return;

		LLSelectMgr* sel_mgr = LLSelectMgr::getInstance();
		sel_mgr->deselectAll();
		sel_mgr->selectObjectAndFamily(objectp);

		LLUUID id = objectp->getID();

		bool added = false;
		std::string entry_name;

		std::list<LLSelectNode*> nodes;

		for (LLObjectSelection::root_iterator iter = sel_mgr->getSelection()->root_begin();
			iter != sel_mgr->getSelection()->root_end(); iter++)
		{
			nodes.push_back(*iter);
		}
		if (nodes.empty())
		{
			nodes.push_back(sel_mgr->getSelection()->getFirstNode());
		}
		
		for( auto node : nodes )
		{
			if (node)
			{
				id = node->getObject()->getID();
			}
			if (id.isNull())
			{
				continue;
			}
			//LL_INFOS() << "Derender node has key " << id << LL_ENDL;
			if (!node->mName.empty())
			{
				if (cur_region)
					entry_name = llformat("Derendered: %s in region %s", node->mName.c_str(), cur_region->getName().c_str());
				else
					entry_name = llformat("Derendered: %s", node->mName.c_str());
			}
			else
			{
				if (cur_region)
					entry_name = llformat("Derendered: (unknown object) in region %s", cur_region->getName().c_str());
				else
					entry_name = "Derendered: (unknown object)";

			}
			added |= add_object_to_blacklist(id, entry_name);
		}

		if (added)
		{
			sel_mgr->deselectAll();
		}
	}
}

void JCFloaterAreaSearch::onClickTrack()
{
	if (LLViewerObject* objectp = getSelectedObject())
		LLTracker::trackLocation(objectp->getPositionGlobal(), mCachedObjects[objectp->getID()].name, "", LLTracker::LOCATION_ITEM);
}

void JCFloaterAreaSearch::onClickUnTrack()
{
	LLTracker::stopTracking(false);
}

void JCFloaterAreaSearch::onClickOwnerProfile()
{
	if (LLViewerObject* objectp = getSelectedObject())
	{
		LLUUID id;
		if (objectp->flagObjectGroupOwned())
		{
			id = mCachedObjects[objectp->getID()].group_id;
			if (id.notNull())
				LLGroupActions::show(id);
		}
		else
		{
			id = mCachedObjects[objectp->getID()].owner_id;
// [RLVa:KB] - Checked: 2010-08-25 (RLVa-1.2.2a) | Modified: RLVa-1.0.0e
			if ( gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS) )
				return;
// [/RLVa:KB]
				LLAvatarActions::showProfile(id);
		}
	}
}

void JCFloaterAreaSearch::onClickGroupProfile()
{
	if (LLViewerObject* objectp = getSelectedObject())
	{
		LLUUID id = mCachedObjects[objectp->getID()].group_id;
		if (id.notNull())
			LLGroupActions::show(id);
	}
}

void JCFloaterAreaSearch::onClickLastOwnerProfile()
{
	if (LLViewerObject* objectp = getSelectedObject())
	{
		LLUUID id = mCachedObjects[objectp->getID()].last_owner_id;
// [RLVa:KB] - Checked: 2010-08-25 (RLVa-1.2.2a) | Modified: RLVa-1.0.0e
		if ( gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS) )
			return;
// [/RLVa:KB]
		LLAvatarActions::showProfile(id);
	}
}

void JCFloaterAreaSearch::onClickCreatorProfile()
{
	if (LLViewerObject* objectp = getSelectedObject())
	{
		const LLUUID& creator_id = mCachedObjects[objectp->getID()].creator_id;
		const LLUUID& owner_id = mCachedObjects[objectp->getID()].owner_id;
// [RLVa:KB] - Checked: 2010-08-25 (RLVa-1.2.2a) | Modified: RLVa-1.0.0e
		if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS)) &&
			((owner_id == creator_id) || (RlvUtil::isNearbyAgent(creator_id))) )
			return;
// [/RLVa:KB]
		LLAvatarActions::showProfile(creator_id);
	}
}

void JCFloaterAreaSearch::onTogglePaused()
{
	mPaused = !mPaused;
	if (mPaused)
	{
		mPendingObjects.clear();
		mCounterText->setText(std::string("Paused, click (Un)Pause again to resume"));
	}
	else
	{
		checkRegion();
		results();
	}
}

void JCFloaterAreaSearch::onRefresh()
{
	mPaused = false;
	checkRegion(true);
	results();
}

void JCFloaterAreaSearch::onCommitLine(LLUICtrl* caller, const LLSD& value, OBJECT_COLUMN_ORDER type)
{
	std::string text = value.asString();
	mCaseSensitive = childGetValue("check_case_sensitive").asBoolean();
	if (!mCaseSensitive)
		LLStringUtil::toLower(text);
	mFilterStrings[type] = text;
	checkRegion();
	results();
}

void JCFloaterAreaSearch::onCheckCaseSensitive()
{
	mCaseSensitive =  childGetValue("check_case_sensitive").asBoolean();
	checkRegion();
	results();
}

bool JCFloaterAreaSearch::requestIfNeeded(LLUUID object_id, LLViewerObject* objectp)
{
	if (mPaused)
		return true;

	if (!mCachedObjects.count(object_id) && !mPendingObjects.count(object_id))
	{
		mPendingObjects.insert(object_id);

		//try to cut down on retries for land impact by pre-requesting it here
		//objectp->getLinksetCost();

		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_ObjectSelect);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_ObjectLocalID, objectp->getLocalID() );
		gAgent.sendReliableMessage();
		return true;
	}
	return false;
}

void JCFloaterAreaSearch::results()
{
	if (!getVisible() || mPaused) return;
	mRegionChecked = false;
	if (mPendingObjects.size() > 0 && mLastUpdateTimer.getElapsedTimeF32() < min_refresh_interval && !mFirstRun) return;
	uuid_vec_t selected = get_focused_list_ids_selected();
	S32 scrollpos = mResultList->getScrollPos();
	mResultList->deleteAllItems();
	S32 i;
	S32 total = gObjectList.getNumObjects();

	for (i = 0; i < total; i++)
	{
		if (LLViewerObject* objectp = gObjectList.getObject(i))
		{
			if (list_wants(objectp))
			{
				// these functions are relatively slow, do them early as possible - RG
				F32 link_cost = objectp->getLinksetCost();
				S32 link_count = objectp->numChildren() + 1;

				LLUUID object_id = objectp->getID();
				if(!requestIfNeeded(object_id, objectp))
				{

					std::map<LLUUID,ObjectData>::iterator it = mCachedObjects.find(object_id);
					if(it != mCachedObjects.end())
					{
						LLResMgr* res_mgr = LLResMgr::getInstance();

						std::string object_name = it->second.name;
						std::string object_desc = it->second.desc;

						std::string price_string;
						if (it->second.sale_info.isForSale())
						{
							price_string = llformat("%s%d", "L$", it->second.sale_info.getSalePrice());
						}
						else
						{
							price_string = "-";
						}

						std::string object_owner;
						std::string object_group;
						std::string object_creator;
						std::string object_last_owner;
						LLAvatarName av_name;

						if (it->second.owner_id.isNull() && !objectp->flagObjectGroupOwned())
						{
							object_owner = "(Public owned)";
							goto public_owned;
						}

						else if (!objectp->flagObjectGroupOwned())
						{
							if (LLAvatarNameCache::get(it->second.owner_id, &av_name))
							{
// [RLVa:KB] - Checked: 2010-11-01 (RLVa-1.2.2a) | Modified: RLVa-1.2.2a
								bool fRlvFilterOwner = (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS) && it->second.owner_id != gAgent.getID()));
								object_owner = (!fRlvFilterOwner) ? av_name.getNSName(areasearch_name_system()) : RlvStrings::getAnonym(av_name);
// [/RLVa:KB]
							}
							else
							{
								object_owner = LLTrans::getString("RetrievingData");
								if (mOwnerNameCacheConnection.find(it->second.owner_id) == mOwnerNameCacheConnection.end())
									mOwnerNameCacheConnection.emplace(it->second.owner_id, LLAvatarNameCache::get(it->second.owner_id, boost::bind(&JCFloaterAreaSearch::onGetOwnerNameCallback, this, _1)));
							}
						}
						else
						{
							gCacheName->getGroupName(it->second.group_id, object_owner);
						}

						public_owned:

						if (LLAvatarNameCache::get(it->second.last_owner_id, &av_name))
						{
// [RLVa:KB] - Checked: 2010-11-01 (RLVa-1.2.2a) | Modified: RLVa-1.2.2a
							LLAvatarNameCache::get(it->second.last_owner_id, &av_name);
							bool fRlvFilterLastOwner = (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS)) && (it->second.last_owner_id != gAgent.getID()) &&
								( (it->second.owner_id == it->second.last_owner_id) || (RlvUtil::isNearbyAgent(it->second.last_owner_id)) );
							object_last_owner = (!fRlvFilterLastOwner) ? av_name.getNSName(areasearch_name_system()) : RlvStrings::getAnonym(av_name);
// [/RLVa:LF]
						}
						else
						{
							object_last_owner = LLTrans::getString("RetrievingData");
							if (mLastOwnerNameCacheConnection.find(it->second.last_owner_id) == mLastOwnerNameCacheConnection.end())
								mLastOwnerNameCacheConnection.emplace(it->second.last_owner_id,  LLAvatarNameCache::get(it->second.last_owner_id, boost::bind(&JCFloaterAreaSearch::onGetLastOwnerNameCallback, this, _1)));
						}
						// </edit>

						if (LLAvatarNameCache::get(it->second.creator_id, &av_name))
						{
// [RLVa:KB] - Checked: 2010-11-01 (RLVa-1.2.2a) | Modified: RLVa-1.2.2a
							LLAvatarNameCache::get(it->second.creator_id, &av_name);
							bool fRlvFilterCreator = (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS)) && (it->second.creator_id != gAgent.getID()) &&
								( (it->second.owner_id == it->second.creator_id) || (RlvUtil::isNearbyAgent(it->second.creator_id)) );
							object_creator = (!fRlvFilterCreator) ? av_name.getNSName(areasearch_name_system()) : RlvStrings::getAnonym(av_name);
// [/RLVa:KB]
						}
						else
						{
							object_creator = LLTrans::getString("RetrievingData");
							if (mCreatorNameCacheConnection.find(it->second.creator_id) == mCreatorNameCacheConnection.end())
								mCreatorNameCacheConnection.emplace(it->second.creator_id, LLAvatarNameCache::get(it->second.creator_id, boost::bind(&JCFloaterAreaSearch::onGetCreatorNameCallback, this, _1)));
						}

						gCacheName->getGroupName(it->second.group_id, object_group);


						// land impact last to give it maximum time to fetch - RG
						std::string prim_count_string;
						res_mgr->getIntegerString(prim_count_string, link_count);

						std::string land_impact_string;

						if (link_cost > 0.001)
						{
							res_mgr->getIntegerString(land_impact_string, (S32)link_cost);
						}
						else
						{
							land_impact_string = " - ";
						}

						std::string onU = object_owner;
						std::string gnU = object_group;
						std::string cnU = object_creator;
						std::string lnU = object_last_owner;
						if (!mCaseSensitive)
						{
							LLStringUtil::toLower(object_name);
							LLStringUtil::toLower(object_desc);
							LLStringUtil::toLower(object_owner);
							LLStringUtil::toLower(object_group);
							LLStringUtil::toLower(object_creator);
							LLStringUtil::toLower(object_last_owner);
						}

						if ((mFilterStrings[LIST_OBJECT_NAME].empty() || object_name.find(mFilterStrings[LIST_OBJECT_NAME]) != std::string::npos) &&
							(mFilterStrings[LIST_OBJECT_DESC].empty() || object_desc.find(mFilterStrings[LIST_OBJECT_DESC]) != std::string::npos) &&
							(mFilterStrings[LIST_OBJECT_OWNER].empty() || object_owner.find(mFilterStrings[LIST_OBJECT_OWNER]) != std::string::npos) &&
							(mFilterStrings[LIST_OBJECT_GROUP].empty() || object_group.find(mFilterStrings[LIST_OBJECT_GROUP]) != std::string::npos) &&
							(mFilterStrings[LIST_OBJECT_LAST_OWNER].empty() || object_last_owner.find(mFilterStrings[LIST_OBJECT_LAST_OWNER]) != std::string::npos) &&
							(mFilterStrings[LIST_OBJECT_CREATOR].empty() || object_creator.find(mFilterStrings[LIST_OBJECT_CREATOR]) != std::string::npos))
						{
							LLScrollListItem::Params element;
							element.value = object_id;

							LLScrollListCell::Params name;
							name.column = "Name";
							name.value = it->second.name;

							LLScrollListCell::Params desc;
							desc.column = "Description";
							desc.value = it->second.desc;

							std::string spacer = "   ";
							LLScrollListCell::Params prims;
							prims.column = "Prims";

							spacer += prim_count_string;
							prims.value = spacer;

							spacer = "   ";
							LLScrollListCell::Params impact;
							impact.column = "LI";

							spacer += land_impact_string;
							impact.value = spacer;

							spacer = "  ";
							LLScrollListCell::Params price;
							price.column = "Price";

							spacer += price_string;
							price.value = spacer;

							LLScrollListCell::Params owner;
							owner.column = "Owner";

	// [RLVa:KB] - Checked: 2010-08-25 (RLVa-1.2.2a) | Modified: RLVa-1.0.0e
							if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS))
							{
								owner.value = "";
							}
							else
							{
								owner.value = onU;
							}
	// [/RLVa:KB]

							LLScrollListCell::Params group;
							group.column = "Group";
							group.value = gnU;

							LLScrollListCell::Params last_owner;
							last_owner.column = "Last Owner";

	// [RLVa:KB] - Checked: 2010-08-25 (RLVa-1.2.2a) | Modified: RLVa-1.0.0e
							if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS))
							{
								last_owner.value = "";
							}
							else
							{
								last_owner.value = lnU;
							}
	// [/RLVa:KB]

							LLScrollListCell::Params creator;
							creator.column = "Creator";

	// [RLVa:KB] - Checked: 2010-08-25 (RLVa-1.2.2a) | Modified: RLVa-1.0.0e
							if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) || gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS)) &&
								 ((object_owner == object_creator) || (RlvUtil::isNearbyAgent(it->second.creator_id))) )
							{
							creator.value = "";
							}
							else
							{
							creator.value = cnU;
							}
	// [/RLVa:KB]

							element.columns.add(name);
							element.columns.add(desc);
							element.columns.add(prims);
							element.columns.add(impact);
							element.columns.add(price);
							element.columns.add(owner);
							element.columns.add(group);
							element.columns.add(last_owner);
							element.columns.add(creator);

							mResultList->addRow(element);
						}
					}
				}
			}
		}
	}

	mFirstRun = false;
	mResultList->updateSort();
	mResultList->selectMultiple(selected);
	mResultList->setScrollPos(scrollpos);
	mCounterText->setText(llformat("%d listed/%d pending/%d total", mResultList->getItemCount(), mPendingObjects.size(), mPendingObjects.size()+mCachedObjects.size()));
	mLastUpdateTimer.reset();
}

//static
void JCFloaterAreaSearch::update()		// NOTE: called from llviewerregion location updaters - RG
{
	JCFloaterAreaSearch* floater = findInstance();
	if (!floater)
		return;
	if (floater && (floater->mPaused || floater->mRegionChecked))
		return;

	floater->mRegionChecked = true;
	floater->checkRegion();
	floater->results();
}

// static
void JCFloaterAreaSearch::receiveObjectProperties(LLUUID object_id, std::string name, std::string desc, LLSaleInfo sale_info, LLUUID owner_id, LLUUID group_id, LLUUID last_owner_id, LLUUID creator_id)
{
	JCFloaterAreaSearch* floater = findInstance();
	if (!floater)
		return;
	if (floater && (floater->mPaused))
		return;

	floater->checkRegion();

	std::set<LLUUID>::iterator it = floater->mPendingObjects.find(object_id);
	if(it != floater->mPendingObjects.end())
		floater->mPendingObjects.erase(it);

	ObjectData* data = &floater->mCachedObjects[object_id];

	// We cache unknown objects (to avoid having to request them later)
	// and requested objects.
	data->name = name;
	data->desc = desc;
	data->sale_info = sale_info;
	data->owner_id = owner_id;
	data->group_id = group_id;
	data->last_owner_id = last_owner_id;
	data->creator_id = creator_id;

	gCacheName->get(data->owner_id, false, boost::bind(&JCFloaterAreaSearch::results, floater));
	gCacheName->get(data->group_id, true, boost::bind(&JCFloaterAreaSearch::results,floater));
	gCacheName->get(data->creator_id, false, boost::bind(&JCFloaterAreaSearch::results,floater));
	gCacheName->get(data->last_owner_id, false, boost::bind(&JCFloaterAreaSearch::results,floater));
}

void JCFloaterAreaSearch::onGetOwnerNameCallback(const LLUUID& id)
{
	mOwnerNameCacheConnection.erase(id);
	checkRegion();
	results();
}

void JCFloaterAreaSearch::onGetLastOwnerNameCallback(const LLUUID& id)
{
	mLastOwnerNameCacheConnection.erase(id);
	checkRegion();
	results();
}

void JCFloaterAreaSearch::onGetCreatorNameCallback(const LLUUID& id)
{
	mCreatorNameCacheConnection.erase(id);
	checkRegion();
	results();
}



