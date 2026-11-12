/*
 * Copyright (C) 2019 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#include "../ui/RegisterAccountWindow.h"

#include <Application.h>

int main(void)
{
	BApplication app("application/x-vnd.Renga-test");

	BWindow* w = new RegisterAccountWindow();
	w->Show();
	app.Run();

	return 0;
}
