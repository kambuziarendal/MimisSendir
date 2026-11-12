/*
 * PeopleListView.h
 * Copyright (C) 2024 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#pragma once


#include "ListView.h"


class PeopleListView: public BListView
{
	public:
				PeopleListView();

		void	MouseDown(BPoint where) override;
};
