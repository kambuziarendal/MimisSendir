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


#include "bookmark2storage.h"
#include <gloox/clientbase.h>
#include <gloox/pubsub.h>
#include <gloox/pubsubitem.h>

#include "Extensions.h"


namespace gloox
{

  Bookmark2Storage::Bookmark2Storage(ClientBase* parent)
    : gloox::PubSub::Manager(parent),
      m_bookmarkHandler( 0 ),
	  m_jid(parent->jid().bare())
  {
  }

  Bookmark2Storage::~Bookmark2Storage()
  {
  }

  void Bookmark2Storage::storeBookmarks( const ConferenceList& cList )
  {
    ConferenceList::const_iterator itc = cList.begin();
	gloox::PubSub::ItemList bookmarks;
    for( ; itc != cList.end(); ++itc )
    {
	  Tag* item = new Tag("item", "id", (*itc).jid);
      Tag* i = new Tag(item, "conference", "name", (*itc).name);
      i->setXmlns( XMLNS_PEP_BOOKMARKS );
      i->addAttribute( "autojoin", (*itc).autojoin ? "true" : "false" );

      new Tag( i, "nick", (*itc).nick );

	  bookmarks.push_back(new gloox::PubSub::Item(item));
    }

	publishItem(m_jid, XMLNS_PEP_BOOKMARKS, bookmarks, storageOptions(), this);
  }

  void Bookmark2Storage::storeBookmark( const ConferenceListItem& conf )
  {
	gloox::PubSub::ItemList bookmarks;
	Tag* item = new Tag("item", "id", conf.jid);
	Tag* i = new Tag(item, "conference", "name", conf.name);
	i->setXmlns( XMLNS_PEP_BOOKMARKS );
	i->addAttribute( "autojoin", conf.autojoin ? "true" : "false" );

	new Tag( i, "nick", conf.nick );

	bookmarks.push_back(new gloox::PubSub::Item(item));

	publishItem(m_jid, XMLNS_PEP_BOOKMARKS, bookmarks, storageOptions(), this);
  }

  void Bookmark2Storage::removeBookmark( const ConferenceListItem& conf )
  {
	gloox::PubSub::ItemList bookmarks;
	Tag* item = new Tag("item", "id", conf.jid);

	bookmarks.push_back(new gloox::PubSub::Item(item));

	deleteItem(m_jid, XMLNS_PEP_BOOKMARKS, bookmarks, true, this);
  }

  void Bookmark2Storage::requestBookmarks()
  {
	requestItems(m_jid, XMLNS_PEP_BOOKMARKS, "", 0, this);
  }

  void Bookmark2Storage::handleItem(const gloox::JID&,
	const std::string&, const gloox::Tag*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  void Bookmark2Storage::handleItems(const std::string&, const gloox::JID&,
		const std::string&, const gloox::PubSub::ItemList& items, const gloox::Error* error)
  {
	  if (error) return;
	  BookmarkList bList;
	  ConferenceList cList;

	  for (auto i: items) {
          ConferenceListItem item;

		  const std::string& jid = i->id();
		  const Tag* conference = i->payload();
		  if (!conference || conference->name() != "conference" || jid.empty()) continue;
		  const std::string& name = conference->findAttribute( "name" );
          const std::string& join = conference->findAttribute( "autojoin" );
          const Tag* nick = conference->findChild( "nick" );

          item.jid = jid;
          item.name = name;
          if( nick )
            item.nick = nick->cdata();
          item.autojoin = ( join == "true" || join == "1" );
          cList.push_back( item );
	  }

	  if( m_bookmarkHandler )
		  m_bookmarkHandler->handleBookmarks( bList, cList );
  }

  void Bookmark2Storage::handleItemPublication(const std::string& /*id*/,
	const gloox::JID& /*service*/, const std::string& /*node*/,
	const gloox::PubSub::ItemList& /*items*/, const gloox::Error* error)
  {
	  if (error) {
		switch (error->error()) {
			case gloox::StanzaErrorServiceUnavailable:
				fprintf(stderr, "%s -> %s(%d Service Unavailable)\n", __PRETTY_FUNCTION__,
					error->text().c_str(), error->type());
				break;
			default:
				fprintf(stderr, "%s -> %s(%d %d)\n", __PRETTY_FUNCTION__,
					error->text().c_str(), error->type(), error->error());
				break;
		}
	  }
  }

  void Bookmark2Storage::handleItemDeletion(const std::string&, const gloox::JID&, const std::string&,
		  const gloox::PubSub::ItemList&, const gloox::Error*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  void Bookmark2Storage::handleSubscriptionResult(const std::string&, const gloox::JID&,
		const std::string&, const std::string&, const gloox::JID&,
		gloox::PubSub::SubscriptionType, const gloox::Error*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  void Bookmark2Storage::handleUnsubscriptionResult(const std::string&, const gloox::JID&,
		  const gloox::Error*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  void Bookmark2Storage::handleSubscriptionOptions(const std::string&, const gloox::JID&,
		  const gloox::JID&, const std::string&, const gloox::DataForm*, const std::string&,
		  const gloox::Error*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  void Bookmark2Storage::handleSubscriptionOptionsResult(const std::string&, const gloox::JID&,
		  const gloox::JID&, const std::string&, const std::string&, const gloox::Error*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  void Bookmark2Storage::handleSubscribers(const std::string&, const gloox::JID&, const std::string&,
		  const gloox::PubSub::SubscriptionList&, const gloox::Error*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  void Bookmark2Storage::handleSubscribersResult(const std::string&, const gloox::JID&,
		  const std::string&, const gloox::PubSub::SubscriberList*, const gloox::Error*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  void Bookmark2Storage::handleAffiliates(const std::string&, const gloox::JID&, const std::string&,
		  const gloox::PubSub::AffiliateList*, const gloox::Error*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  void Bookmark2Storage::handleNodeConfig(const std::string&, const gloox::JID&, const std::string&,
		  const gloox::DataForm*, const gloox::Error*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  void Bookmark2Storage::handleNodeConfigResult(const std::string&, const gloox::JID&,
		  const std::string&, const gloox::Error*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  void Bookmark2Storage::handleNodeCreation(const std::string&, const gloox::JID&, const std::string&,
		  const gloox::Error*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  void Bookmark2Storage::handleNodeDeletion(const std::string&, const gloox::JID&, const std::string&,
		  const gloox::Error*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  void Bookmark2Storage::handleNodePurge(const std::string&, const gloox::JID&, const std::string&,
		  const gloox::Error*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  void Bookmark2Storage::handleSubscriptions(const std::string&, const gloox::JID&,
		  const gloox::PubSub::SubscriptionMap&, const gloox::Error*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  void Bookmark2Storage::handleAffiliations(const std::string&, const gloox::JID&,
		  const gloox::PubSub::AffiliationMap&, const gloox::Error*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  void Bookmark2Storage::handleDefaultNodeConfig(const std::string&, const gloox::JID&,
		  const gloox::DataForm*, const gloox::Error*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  void Bookmark2Storage::handleAffiliatesResult(const std::string&, const gloox::JID&,
		  const std::string&, const gloox::PubSub::AffiliateList*, const gloox::Error*)
  {
	  puts(__PRETTY_FUNCTION__);
  }

  gloox::DataForm* Bookmark2Storage::storageOptions()
  {
	  static bool done = false;
	  static DataForm storageOptions(gloox::TypeSubmit);

	  if (!done) {
		  storageOptions.addField(gloox::DataFormField::TypeHidden, "FORM_TYPE",
			"http://jabber.org/protocol/pubsub#publish-options");
		  storageOptions.addField(gloox::DataFormField::TypeNone,
			"pubsub#persist_items", "true");
		  storageOptions.addField(gloox::DataFormField::TypeNone,
			"pubsub#max_items", "max");
		  storageOptions.addField(gloox::DataFormField::TypeNone,
			"pubsub#send_last_published_item", "never");
		  storageOptions.addField(gloox::DataFormField::TypeNone,
			"pubsub#access_model", "whitelist");

		  done = true;
	  }

	  return dynamic_cast<DataForm*>(storageOptions.clone());
  }
}
