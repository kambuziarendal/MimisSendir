/*
  Copyright (c) 2005-2018 by Jakob Schröter <js@camaya.net>
  Copyright (c) 2019 by Adrien Destugues <pulkomandy@pulkomandy.tk>
  This file is part of the gloox library. http://camaya.net/gloox

  This software is distributed under a license. The full license
  agreement can be found in the file LICENSE in this distribution.
  This software may not be copied, modified, sold or distributed
  other than expressed in the named license agreement.

  This software is distributed without any warranty.
*/


#pragma once

#include <gloox/macros.h>

#include <gloox/bookmarkhandler.h>
#include <gloox/pubsubmanager.h>
#include <gloox/pubsubresulthandler.h>

#include <string>
#include <list>

namespace gloox
{

  class Tag;

  /**
   * @brief This is an implementation of @xep{0402} (Bookmark Storage 2).
   *
   * You can use this class to store bookmarks to multi-user chat rooms or ordinary URLs
   * on the server (and to retrieve them later on).
   * To retrieve all stored bookmarks for the current user you have to create a class which
   * inherits from BookmarkHandler. This handler receives retrieved bookmarks.
   *
   * @code
   * class MyClass : public BookmarkHandler
   * {
   *   public:
   *     // ...
   *     void myFuncRetrieve();
   *     void myFuncStore();
   *     void handleBookmarks( const BookmarkList &bList, const ConferenceList &cList );
   *
   *   private:
   *     BookmarkStorage* m_bs;
   *     BookmarkList m_bList;
   *     ConferenceList m_cList;
   * };
   *
   * void MyClass::myFuncRetrieve()
   * {
   *   m_bs = new Bookmark2Storage( m_client );
   *   m_bs->requestBookmarks();
   * }
   *
   * void MyClass::handleBookmarks( const BookmarkList &bList, const ConferenceList &cList )
   * {
   *   m_bList = bList;
   *   m_cList = cList;
   * }
   * @endcode
   *
   *
   * This protocol stores each bookmark separately, so you can use storeBookmark
   * and storeBookmarks to store additional bookmarks or update existing ones.
   * You can use removeBookmark to remove a single bookmark from the list.
   *
   * @code
   * void MyClass::myFuncStore()
   * {
   *   conferenceListItem ci
   *   ci.name = "jabber/xmpp development room";
   *   ci.jid = "jdev@conference.jabber.org";
   *   ci.nick = "myNick";
   *   ci.password = EmptyString;
   *   ci.autojoin = true;
   *   m_cList.push_back( ci );
   *
   *   m_bs->storeBookmarks( m_bList, m_cList );
   * }
   * @endcode
   *
   * Note that this protocol does not support URL bookmarks. They are present
   * in the interface only for compatibility with the BookmarkHandler API, but
   * will be ignored.
   *
   * @author Adrien Destugues <pulkomandy@pulkomandy.tk>
   * @since 1.1
   */
  class GLOOX_API Bookmark2Storage : public PubSub::Manager, public PubSub::ResultHandler
  {
    public:
      /**
       * Constructs a new BookmarkStorage object.
       * @param parent The ClientBase to use for communication.
       */
      Bookmark2Storage( ClientBase* parent );

      /**
       * Virtual destructor.
       */
      virtual ~Bookmark2Storage();

      /**
       * Use this function to store a number of conference bookmarks on the server.
       * @param cList A list of conferences to store or update.
       */
      void storeBookmarks( const ConferenceList& cList );

      /**
       * Use this function to store a conference bookmark on the server.
       * @param conf A conference to store or update.
       */
      void storeBookmark( const ConferenceListItem& conf );

      /**
	   * Use this function to delete a conference bookmark from the server.
       * @param conf A conference to remove.
       */
      void removeBookmark( const ConferenceListItem& conf );

      /**
       * Use this function to initiate retrieval of bookmarks. Use registerBookmarkHandler()
       * to register an object which will receive the lists of bookmarks.
       */
      void requestBookmarks();

      /**
       * Use this function to register a BookmarkHandler.
       * @param bmh The BookmarkHandler which shall receive retrieved bookmarks.
       */
      void registerBookmarkHandler( BookmarkHandler* bmh )
        { m_bookmarkHandler = bmh; }

      /**
       * Use this function to un-register the BookmarkHandler.
       */
      void removeBookmarkHandler()
        { m_bookmarkHandler = 0; }


	  void handleItem(const gloox::JID&,
		const std::string&, const gloox::Tag*);
      void handleItems(const std::string&, const gloox::JID&, const std::string&,
		const gloox::PubSub::ItemList&, const gloox::Error*);
	  void handleItemPublication(const std::string&, const gloox::JID&,
		const std::string&, const gloox::PubSub::ItemList&, const gloox::Error*);
	  void handleItemDeletion(const std::string&, const gloox::JID&, const std::string&,
		const gloox::PubSub::ItemList&, const gloox::Error*);
	  void handleSubscriptionResult(const std::string&, const gloox::JID&,
		const std::string&, const std::string&, const gloox::JID&,
		gloox::PubSub::SubscriptionType, const gloox::Error*);
	  void handleUnsubscriptionResult(const std::string&, const gloox::JID&,
		const gloox::Error*);
	  void handleSubscriptionOptions(const std::string&, const gloox::JID&,
		const gloox::JID&, const std::string&, const gloox::DataForm*, const std::string&,
		const gloox::Error*);
	  void handleSubscriptionOptionsResult(const std::string&, const gloox::JID&,
		const gloox::JID&, const std::string&, const std::string&, const gloox::Error*);
	  void handleSubscribers(const std::string&, const gloox::JID&, const std::string&,
		const gloox::PubSub::SubscriptionList&, const gloox::Error*);
	  void handleSubscribersResult(const std::string&, const gloox::JID&,
		const std::string&, const gloox::PubSub::SubscriberList*, const gloox::Error*);
	  void handleAffiliates(const std::string&, const gloox::JID&, const std::string&,
		const gloox::PubSub::AffiliateList*, const gloox::Error*);
	  void handleNodeConfig(const std::string&, const gloox::JID&, const std::string&,
		const gloox::DataForm*, const gloox::Error*);
	  void handleNodeConfigResult(const std::string&, const gloox::JID&,
		const std::string&, const gloox::Error*);
	  void handleNodeCreation(const std::string&, const gloox::JID&, const std::string&,
		const gloox::Error*);
	  void handleNodeDeletion(const std::string&, const gloox::JID&, const std::string&,
		const gloox::Error*);
	  void handleNodePurge(const std::string&, const gloox::JID&, const std::string&,
		const gloox::Error*);
	  void handleSubscriptions(const std::string&, const gloox::JID&,
		const gloox::PubSub::SubscriptionMap&, const gloox::Error*);
	  void handleAffiliations(const std::string&, const gloox::JID&,
		const gloox::PubSub::AffiliationMap&, const gloox::Error*);
	  void handleDefaultNodeConfig(const std::string&, const gloox::JID&,
		const gloox::DataForm*, const gloox::Error*);
	  void handleAffiliatesResult(const std::string&, const gloox::JID&,
		const std::string&, const gloox::PubSub::AffiliateList*, const gloox::Error*);

	private:
	  static gloox::DataForm* storageOptions();
    private:
      BookmarkHandler* m_bookmarkHandler;
	  std::string m_jid;
  };

}
