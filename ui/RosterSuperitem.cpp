//////////////////////////////////////////////////
// Blabber [RosterSuperitem.cpp]
//////////////////////////////////////////////////

#include "RosterSuperitem.h"

RosterSuperitem::RosterSuperitem(const char *text)
	: BStringItem(text) {
}

RosterSuperitem::~RosterSuperitem() {
}

void RosterSuperitem::DrawItem(BView *owner, BRect frame, bool complete) {
	BStringItem::DrawItem(owner, frame, complete);
}
