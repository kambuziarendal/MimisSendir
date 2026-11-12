//////////////////////////////////////////////////
// Blabber [EditingFilter.h]
//////////////////////////////////////////////////

#ifndef EDITING_FILTER_H
#define EDITING_FILTER_H

#ifndef _MESSAGE_FILTER_H
	#include <MessageFilter.h>
#endif

#ifndef _TEXT_VIEW_H
	#include <interface/TextView.h>
#endif

class TalkView;

class EditingFilter : public BMessageFilter {
public:
	                      EditingFilter(BTextView *view, TalkView *window);

	virtual filter_result Filter(BMessage *message, BHandler **target);

private:
	BTextView           *_view;
	TalkView          *_window;
};
#endif

